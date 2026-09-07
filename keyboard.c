#include <stdint.h>
#include "serial.h"

#define KBD_BASE        0x10003000UL

#define KBD_STATUS     (KBD_BASE + 0x00)
#define KBD_EVENT      (KBD_BASE + 0x04)
#define KBD_LED        (KBD_BASE + 0x08)

#define KBD_STATUS_EVENT   (1u << 0)

static inline uint32_t kbd_read(uint32_t addr)
{
    return *(volatile uint32_t *)addr;
}

static inline void kbd_write(uint32_t addr, uint32_t value)
{
    *(volatile uint32_t *)addr = value;
}

void keyboard_test(void)
{
    uint32_t event;
    uint32_t key;
    uint32_t row;
    uint32_t col;
    uint32_t pressed;

    serial_putstr("\n");
    serial_putstr("Keyboard test\n");
    serial_putstr("-------------\n");
    serial_putstr("Press keys...\n");

    while (1)
    {
        /*
         * Drain all events currently in FIFO.
         */
        while (kbd_read(KBD_STATUS) & KBD_STATUS_EVENT)
        {
            /*
             * Reading EVENT pops one event from FIFO.
             */
            event = kbd_read(KBD_EVENT);

            key     = event & 0xFF;
            pressed = (event >> 15) & 1;

            /*
             * 4 rows x 8 columns:
             *
             * key = row * 8 + column
             */
            row = key >> 3;
            col = key & 7;

            if (pressed)
                serial_putstr("DOWN ");
            else
                serial_putstr("UP   ");

            serial_putstr_hex("KEY = ", key);
            serial_putstr_hex("ROW = ", row);
            serial_putstr_hex("COL = ", col);

            serial_putstr("\n");
        }
    }
}