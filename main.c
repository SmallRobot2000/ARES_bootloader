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

//Where kernel and DTB is
#define CONFIG_KERNEL_DST   0x90800000
#define CONFIG_DTB_DST      0x90400000

#ifndef CONFIG_UARTLITE_BASE
#define CONFIG_UART_BASE 0x10000000
#endif
#define CONFIG_XSPI_BASE 0x10001000
#define CONFIG_XSPI_GPIO_BASE 0x10003000
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
//-----------------------------------------------------------------
// main:
//-----------------------------------------------------------------
int main(void)
{
    serial_init(CONFIG_UART_BASE, 0);
    spi_init(CONFIG_XSPI_BASE);
    
    //dump_csrs();

    serial_putstr("\n");
    serial_putstr(" _____  _____  _____  _____   __      __  _      _                    ____              _   \n");
    serial_putstr("|  __ \\|_   _|/ ____|/ ____|  \\ \\    / / | |    (_)                  |  _ \\            | |  \n");
    serial_putstr("| |__) | | | | (___ | |   _____\\ \\  / /  | |     _ _ __  _   ___  __ | |_) | ___   ___ | |_ \n");
    serial_putstr("|  _  /  | |  \\___ \\| |  |______\\ \\/ /   | |    | | '_ \\| | | \\ \\/ / |  _ < / _ \\ / _ \\| __|\n");
    serial_putstr("| | \\ \\ _| |_ ____) | |____      \\  /    | |____| | | | | |_| |>  <  | |_) | (_) | (_) | |_ \n");
    serial_putstr("|_|  \\_\\_____|_____/ \\_____|      \\/     |______|_|_| |_|\\__,_/_/\\_\\ |____/ \\___/ \\___/ \\__|\n");
    serial_putstr("\n");

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


    serial_putstr("\nLoading tree.dtb\n");
    // Open file
    res = f_open(&file, "tree.dtb", FA_READ);
    if (res != FR_OK) {
        serial_putstr_hex("Open tree.dtb failed: ", res);
        return -1;
    }


    res = f_read(&file, (uint8_t*)(CONFIG_DTB_DST), 0x10000, &bytes_read); //Max 64k
    if (res != FR_OK) {
        serial_putstr_hex("Read tree.dtb failed: ", res);
        f_close(&file);
        return -1;
    }
    serial_putstr("Done\n");


    serial_putstr("\nLoading image.bin\n");
    // Open file
    res = f_open(&file, "image.bin", FA_READ);
    if (res != FR_OK) {
        serial_putstr_hex("Open image.bin failed: ", res);
        return -1;
    }

    // Read image file
    file_offset = 0;
    do{
        res = f_read(&file, (uint8_t*)(CONFIG_KERNEL_DST + file_offset), LOAD_SIZE, &bytes_read);
        file_offset += bytes_read;
        serial_putchar('#');
        if (res != FR_OK) {
            serial_putstr_hex("Read image.bin failed: ", res);
            f_close(&file);
            return -1;
        }
    } while(bytes_read == LOAD_SIZE);
    // Close file
    f_close(&file);

    serial_putstr("\nDone\n");
    // Unmount
    f_mount(NULL, "", 0);

    



    serial_putstr("Booting...\n");
    boot_kernel(CONFIG_KERNEL_DST, CONFIG_DTB_DST);
    return 0;

} 
