#include "exception.h"
#include "csr.h"
#include "serial.h"
#include "assert.h"

//-----------------------------------------------------------------
// Locals
//-----------------------------------------------------------------
#define CAUSE_MAX_EXC      (CAUSE_PAGE_FAULT_STORE + 1)
static fp_exception        _exception_table[CAUSE_MAX_EXC];

static fp_irq              _irq_handler     = 0;

void exception_set_irq_handler(fp_irq handler)         { _irq_handler = handler; }
void exception_set_syscall_handler(fp_syscall handler) 
{ 
    _exception_table[CAUSE_ECALL_U] = handler;
    _exception_table[CAUSE_ECALL_S] = handler;
    _exception_table[CAUSE_ECALL_M] = handler;
}
//-----------------------------------------------------------------
// exception_set_handler: Register exception handler
//-----------------------------------------------------------------
void exception_set_handler(int cause, fp_exception handler)
{
    _exception_table[cause] = handler;
}


#define PTE_V 0x001
#define PTE_R 0x002
#define PTE_X 0x008

static void dump_sv32_translation(uint32_t va)
{
    uint32_t satp = csr_read(satp);

    uint32_t mode = satp >> 31;
    uint32_t root_ppn = satp & 0x003fffff;

    serial_putstr_hex("SATP      ", satp);

    if (mode != 1) {
        serial_putstr("Not Sv32\n");
        return;
    }

    uint32_t root = root_ppn << 12;

    uint32_t vpn1 = (va >> 22) & 0x3ff;
    uint32_t vpn0 = (va >> 12) & 0x3ff;

    serial_putstr_hex("ROOT      ", root);
    serial_putstr_hex("VPN1      ", vpn1);
    serial_putstr_hex("VPN0      ", vpn0);

    uint32_t pte1 = *(volatile uint32_t *)(root + vpn1 * 4);

    serial_putstr_hex("PTE1      ", pte1);

    if (!(pte1 & PTE_V)) {
        serial_putstr("L1 invalid\n");
        return;
    }

    // leaf at level 1 (superpage)
    if (pte1 & (PTE_R | PTE_X)) {
        uint32_t pa =
            ((pte1 >> 10) << 22) |
            (va & 0x003fffff);

        serial_putstr_hex("PA        ", pa);
        return;
    }

    uint32_t next = (pte1 >> 10) << 12;

    uint32_t pte0 = *(volatile uint32_t *)(next + vpn0 * 4);

    serial_putstr_hex("PTE0      ", pte0);

    if (!(pte0 & PTE_V)) {
        serial_putstr("L0 invalid\n");
        return;
    }

    uint32_t pa =
        ((pte0 >> 10) << 12) |
        (va & 0xfff);

    serial_putstr_hex("PA        ", pa);
}

//-----------------------------------------------------------------
// exception_handler:
//-----------------------------------------------------------------
struct irq_context * exception_handler(struct irq_context *ctx)
{
/*
    serial_putstr("\n=== TRAP ===\n");

    serial_putstr_hex("ctx cause = ", ctx->cause);
    serial_putstr_hex("ctx pc    = ", ctx->pc);
    serial_putstr_hex("mepc      = ", csr_read(mepc));
    serial_putstr_hex("mcause    = ", csr_read(mcause));
    serial_putstr_hex("mtval     = ", csr_read(mtval));
    serial_putstr_hex("mstatus   = ", csr_read(mstatus));
*/
    // External interrupt
    if (ctx->cause & CAUSE_INTERRUPT)
    {
        if (_irq_handler)
            ctx = _irq_handler(ctx);
        else
            serial_putstr_hex("ERROR: Unhandled IRQ: ", ctx->cause);
    }
    // Exception
    else
    {
        switch (ctx->cause)
        {
            case CAUSE_ECALL_U:
            case CAUSE_ECALL_S:
            case CAUSE_ECALL_M:
                ctx->pc += 4;
                break;
        }

        if (ctx->cause < CAUSE_MAX_EXC && _exception_table[ctx->cause])
            ctx = _exception_table[ctx->cause](ctx);
        else
        {

            serial_putstr_hex("ERROR: Unhandled exception: ", ctx->cause);
            serial_putstr_hex("       at PC: ", ctx->pc);
            if(ctx->cause == 0x0C)
            {
                int32_t va = csr_read(mepc);
                uint32_t satp = csr_read(satp);

                serial_putstr_hex("MCAUSE", csr_read(mcause));
                serial_putstr_hex("MEPC", csr_read(mepc));
                serial_putstr_hex("MTVAL", csr_read(mtval));
                serial_putstr_hex("MTVEC", csr_read(mtvec));
                serial_putstr_hex("MSTATUS", csr_read(mstatus));

                serial_putstr_hex("FAULT VA: ", va);
                serial_putstr_hex("SATP: ", satp);


                dump_sv32_translation(csr_read(mtval));
                dump_sv32_translation(0xc0000088);
            }
            assert(!"Unhandled exception");
        }
    }
    return ctx;
}
