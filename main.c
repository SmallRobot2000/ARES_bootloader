#include "csr.h"
#include "exception.h"
#include "sbi.h"
#include "emulation.h"
#include "serial.h"
#include "assert.h"
#include "spi.h"
#include "rdtime.h"
#include "sd.h"
#include "ff.h"

//build/platform/generic/firmware/fw_jump.elf
#define OPENSBI_ADDR  0x90400000
#define KERNEL_ADDR   0x90800000
#define DTB_ADDR      0x9f000000

#ifndef CONFIG_UARTLITE_BASE
#define CONFIG_UART_BASE 0x10000000
#endif
#define CONFIG_XSPI_BASE 0x10001000
#undef CONFIG_KERNEL_EMBEDDED

#define LOAD_SIZE 0x20000 //128K how much at a time to load form a file
//-----------------------------------------------------------------
// Defines:
//-----------------------------------------------------------------
#define MSTATUS_MPP_SHIFT   11
#define PRV_S 1
#define PRV_M 3

//-----------------------------------------------------------------
// Defines:
//-----------------------------------------------------------------
#define CSR_MSTATUS  0x300
#define CSR_MIE      0x304
#define CSR_MTVEC    0x305
#define CSR_MEDELEG  0x302
#define CSR_MIDELEG  0x303
#define CSR_MIP      0x344
#define CSR_MEPC     0x341
#define CSR_MSCRATCH 0x340
#define CSR_SATP     0x180
extern uint32_t _sp;


typedef void (*entry_fn_t)(void);


//-----------------------------------------------------------------
// jump_to_program: run program at known address
//-----------------------------------------------------------------
void jump_to_program(uintptr_t entry_address)
{
    entry_fn_t entry = (entry_fn_t)entry_address;
    entry();
}

//-----------------------------------------------------------------
// irqctrl_handler: Interrupt handler
//-----------------------------------------------------------------
static struct irq_context *irq_callback(struct irq_context *ctx)
{
    serial_putstr_hex("Unexpected M-mode interrupt: ",
                      ctx->cause & 0xF);
    _exit(-1);
    return ctx;
}

static struct irq_context * illegal_handler(struct irq_context *ctx)
{
    uint32_t mcause;
    uint32_t mepc;
    uint32_t mtval;

    asm volatile ("csrr %0, mcause" : "=r"(mcause));
    asm volatile ("csrr %0, mepc"   : "=r"(mepc));
    asm volatile ("csrr %0, mtval"  : "=r"(mtval));

    serial_putstr("ILLEGAL INSTRUCTION\n");
    serial_putstr_hex("mcause: ", mcause);
    serial_putstr_hex("mepc:   ", mepc);
    serial_putstr_hex("mtval:  ", mtval);

    while (1);

    return ctx;
}

static void dump_csrs(void)
{
    serial_putstr("\n--- CSR dump ---\n");

    serial_putstr_hex("mstatus = ", csr_read(mstatus));
    serial_putstr_hex("mie     = ", csr_read(mie));
    serial_putstr_hex("mip     = ", csr_read(mip));
    serial_putstr_hex("medeleg = ", csr_read(medeleg));
    serial_putstr_hex("mideleg = ", csr_read(mideleg));
    serial_putstr_hex("mtvec   = ", csr_read(mtvec));
    serial_putstr_hex("mepc    = ", csr_read(mepc));
    serial_putstr_hex("satp    = ", csr_read(satp));
    serial_putstr_hex("mscratch= ", csr_read(mscratch));
}


#define CACHE_BLOCK_SIZE 64u
static inline void cache_flush_block(uintptr_t address)
{
    __asm__ volatile (
        "cbo.flush 0(%0)"
        :
        : "r"(address)
        : "memory"
    );
}

void cache_flush_range(void *start, size_t length)
{
    if (length == 0)
        return;

    uintptr_t first =
        (uintptr_t)start & ~(uintptr_t)(CACHE_BLOCK_SIZE - 1u);

    uintptr_t last =
        ((uintptr_t)start + length + CACHE_BLOCK_SIZE - 1u) &
        ~(uintptr_t)(CACHE_BLOCK_SIZE - 1u);

    for (uintptr_t p = first; p < last; p += CACHE_BLOCK_SIZE)
        cache_flush_block(p);

    /*
     * Wait until cache writebacks are visible to subsequent
     * memory accesses.
     */
    __asm__ volatile ("fence rw, rw" ::: "memory");
}

//-----------------------------------------------------------------
// boot_kernel:
//-----------------------------------------------------------------
static int boot_kernel(uint32_t entry_addr, uint32_t dtb_addr)
{

    exception_set_handler(CAUSE_ECALL_S, sbi_syscall);
    exception_set_handler(CAUSE_ILLEGAL_INSTRUCTION, illegal_handler);

    exception_set_irq_handler(irq_callback);


    


    csr_write(mie, 0);
    //csr_write(mip, 0);


    uint32_t deleg =
        (1 << CAUSE_ECALL_U) |
        (1 << CAUSE_ILLEGAL_INSTRUCTION) |
        (1 << CAUSE_MISALIGNED_LOAD) |
        (1 << CAUSE_MISALIGNED_STORE);

    csr_write(medeleg, deleg);
    csr_write(mideleg, 0);


    csr_write(mepc, entry_addr);


    //uint32_t status = csr_read(mstatus);

    //serial_putstr_hex("mstatus old = ", status);




    csr_write(mscratch, &_sp);


    //serial_putstr("\n--- Final state before mret ---\n");
    //dump_csrs();


    //serial_putstr("\nJumping to S-mode...\n");


    // Important when writing code into executable RAM
    //asm volatile("fence.i");

    serial_putstr("Testing S CSRs...\n");

    csr_write(sie, 0);
    serial_putstr("sie OK\n");

    csr_write(stvec, 0);
    serial_putstr("stvec OK\n");

    csr_write(sscratch, 0);
    serial_putstr("sscratch OK\n");

    csr_write(satp, 0);
    serial_putstr("satp OK\n");

    asm volatile("sfence.vma zero, zero" ::: "memory");
    asm volatile("fence.i" ::: "memory");
    csr_write(mepc, entry_addr);

    uint32_t status = csr_read(mstatus);
    status &= ~(3 << MSTATUS_MPP_SHIFT);
    status |= (PRV_S << MSTATUS_MPP_SHIFT);
    status |= SR_MPIE;
    csr_write(mstatus, status);

    cache_flush_range((void *)entry_addr, 0x100000);
    cache_flush_range((void *)dtb_addr, 0x100000);

    asm volatile(
        "mv a0, zero\n"
        "mv a1, %0\n"
        "mret\n"
        :
        : "r"(dtb_addr)
        : "a0", "a1", "memory"
    );



    serial_putstr("ERROR: returned from mret\n");

    return 0;
}


typedef uintptr_t uptr;

/*
 * Transfer control to OpenSBI.
 *
 * opensbi_entry:
 *     Address where fw_jump.bin was loaded, for example 0x90000000.
 *
 * hart_id:
 *     Boot hart ID, normally 0 for your single-core VexiiSoc.
 *
 * dtb_address:
 *     Physical address of the loaded DTB, for example 0x9f000000.
 */
__attribute__((noreturn))
void jump_to_opensbi(uptr opensbi_entry,
                     uptr hart_id,
                     uptr dtb_address)
{
    /*
     * Keep the arguments in the registers required by the OpenSBI
     * boot protocol.
     */
    register uptr reg_a0 __asm__("a0") = hart_id;
    register uptr reg_a1 __asm__("a1") = dtb_address;
    register uptr target __asm__("t0") = opensbi_entry;

    __asm__ volatile (
        /*
         * Disable machine interrupts before transferring control.
         */
        "csrci mstatus, 8\n"
        "csrw  mie, zero\n"

        /*
         * OpenSBI must start with address translation disabled.
         */
        "csrw  satp, zero\n"
        "sfence.vma zero, zero\n"

        /*
         * Make SD-loaded data/code visible before executing it.
         *
         * Flush loaded ranges explicitly before calling this function
         * when the D-cache is enabled.
         */
        "fence rw, rw\n"
        "fence.i\n"

        /*
         * Jump without modifying ra.
         */
        "jr %[entry]\n"
        :
        : [entry] "r" (target),
          "r" (reg_a0),
          "r" (reg_a1)
        : "memory"
    );

    __builtin_unreachable();
}


/*
    Load a file form SD to memory
*/
int load_from_sd(const char *name, void *addr)
{
    int res;
    static FIL file;
    UINT bytes_read;
    UINT file_offset = 0;
    res = f_open(&file, name, FA_READ);
    if (res != FR_OK) {
        serial_putstr_hex("Opening file failed: ", res);
        return -1;
    }


    do{
        res = f_read(&file, (uint8_t*)(addr + file_offset), LOAD_SIZE, &bytes_read);
        file_offset += bytes_read;
        serial_putchar('#');
        if (res != FR_OK) {
            serial_putstr_hex("Reading file failed: ", res);
            f_close(&file);
            return -1;
        }
    } while(bytes_read == LOAD_SIZE);

    f_close(&file);

    return 0;
}
//-----------------------------------------------------------------
// main:
//-----------------------------------------------------------------
int main(void)
{
    serial_init(CONFIG_UART_BASE, 0);
    
    
    //dump_csrs();

    serial_putstr("\n");
    serial_putstr(" _____  _____  _____  _____   __      __  _      _                    ____              _   \n");
    serial_putstr("|  __ \\|_   _|/ ____|/ ____|  \\ \\    / / | |    (_)                  |  _ \\            | |  \n");
    serial_putstr("| |__) | | | | (___ | |   _____\\ \\  / /  | |     _ _ __  _   ___  __ | |_) | ___   ___ | |_ \n");
    serial_putstr("|  _  /  | |  \\___ \\| |  |______\\ \\/ /   | |    | | '_ \\| | | \\ \\/ / |  _ < / _ \\ / _ \\| __|\n");
    serial_putstr("| | \\ \\ _| |_ ____) | |____      \\  /    | |____| | | | | |_| |>  <  | |_) | (_) | (_) | |_ \n");
    serial_putstr("|_|  \\_\\_____|_____/ \\_____|      \\/     |______|_|_| |_|\\__,_/_/\\_\\ |____/ \\___/ \\___/ \\__|\n");
    serial_putstr("\n");

    spi_init(CONFIG_XSPI_BASE);
    //while(1);
    emulation_init();
    exception_set_handler(CAUSE_ILLEGAL_INSTRUCTION, illegal_handler);

    serial_putstr_hex("misa = ", csr_read(misa));

    //Initalize FS
    static FATFS fs;
    static FIL file;
    static char buffer[128];
    uint32_t file_offset = 0;
    FRESULT res;
    UINT bytes_read;

    // Mount filesystem
    res = f_mount(&fs, "", 1);
    if (res != FR_OK) {
        serial_putstr_hex("Mount failed: ", res);
        return -1;
    }


    serial_putstr("\nLoading image.bin\n");
    if(load_from_sd("image.bin", (void*)KERNEL_ADDR))
    {
        return -1;
    }

    serial_putstr("\nLoading tree.dtb\n");
    if(load_from_sd("tree.dtb", (void*)DTB_ADDR))
    {
        return -1;
    }

    serial_putstr("\nLoading fw_jump.bin\n");
    if(load_from_sd("fw_jump.bin", (void*)OPENSBI_ADDR))
    {
        return -1;
    }

    // Unmount
    f_mount(NULL, "", 0);

    serial_putstr("Booting...\n");
    jump_to_opensbi(OPENSBI_ADDR, 0, DTB_ADDR);

    return 0;

} 
