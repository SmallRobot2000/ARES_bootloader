#include "VDP.h"

#define VDP_T0_MAP_BASE     0x10020000
#define VDP_T0_DATA_BASE    0x10010000

#define VDP_T1_MAP_BASE     0x10050000
#define VDP_T1_DATA_BASE    0x10040000

#define VDP_S0_DATA_BASE    0x10028000
#define VDP_S0_ATT_BASE     0x10030000

#define VDP_REG_BASE        0x10032000

#define VDP_B0_BASE         0x9F800000

#define VDP_CTRL_REG        0x00
#define VDP_STAT_REG        0x01
#define VDP_LINE_REG        0x02

#define VDP_T0_X_OFF_REG    0x04
#define VDP_T0_Y_OFF_REG    0x05

#define VDP_T1_X_OFF_REG    0x08
#define VDP_T1_Y_OFF_REG    0x09

#define VDP_BIT_CTRL_T0_EN  0x00
#define VDP_BIT_CTRL_T1_EN  0x01
#define VDP_BIT_CTRL_S0_EN  0x02
#define VDP_BIT_CTRL_B0_EN  0x03
#define VDP_BIT_CTRL_B0_LC  0x04    //Linux compatable mod e.g. no x/y offset and bitmap is raw 640x480x16bpp RGB565 with auto truncation
                                    //Normal mode bitmap is 1024x1024 with x and y offset and ARGB4444

#define VDP_BIT_STAT_H_BLK  0x00
#define VDP_BIT_STAT_V_BLK  0x01


#define FB_BASE       VDP_B0_BASE
#define FB_SIZE       (640 * 480 * 2)
#define CACHE_LINE    64u

static inline void cache_clean(void *addr)
{
    __asm__ volatile (
        "cbo.clean 0(%0)"
        :
        : "r"(addr)
        : "memory"
    );
}

void framebuffer_flush(void)
{
    uintptr_t start = FB_BASE;
    uintptr_t end   = FB_BASE + FB_SIZE;

    __asm__ volatile ("fence rw, rw" ::: "memory");

    for (uintptr_t p = start; p < end; p += CACHE_LINE)
        cache_clean((void *)p);

    __asm__ volatile ("fence rw, rw" ::: "memory");
}


static volatile uint16_t *t0_map;
static volatile uint8_t *t0_data;

static volatile uint16_t *t1_map;
static volatile uint8_t *t1_data;

static volatile uint8_t *s_data;
static volatile uint64_t *s_att;

static volatile uint32_t *r_map;

static volatile uint16_t *b0_base;
static const uint8_t char_0[64] =
    {
        0x00, 0x00, 0x00, 0x02, 0x02, 0x00, 0x00, 0x00,

        0x00, 0x00, 0x02, 0x00, 0x00, 0x02, 0x00, 0x00,

        0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00,

        0x00, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x00,

        0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00,

        0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00,

        0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00,

        0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08


    };

static const uint8_t char_1[64] =
    {
        0x00, 0x03, 0x03, 0x03, 0x03, 0x03, 0x00, 0x00,

        0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00,

        0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00,

        0x00, 0x03, 0x03, 0x03, 0x03, 0x03, 0x00, 0x00,

        0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00,

        0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00,

        0x00, 0x03, 0x03, 0x03, 0x03, 0x03, 0x00, 0x00,

        0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04


    };



static const uint8_t sprite32_smile[32 * 32] = {
    // row 0
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,

    // row 1
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,

    // row 2
    0,0,0,0,0,0,0,0,0,0,0,0,6,6,6,6,6,6,6,6,0,0,0,0,0,0,0,0,0,0,0,0,

    // row 3
    0,0,0,0,0,0,0,0,0,6,6,6,6,6,6,6,6,6,6,6,6,6,6,0,0,0,0,0,0,0,0,0,

    // row 4
    0,0,0,0,0,0,0,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,0,0,0,0,0,0,0,

    // row 5
    0,0,0,0,0,0,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,0,0,0,0,0,0,

    // row 6
    0,0,0,0,0,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,0,0,0,0,0,

    // row 7
    0,0,0,0,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,0,0,0,0,

    // row 8
    0,0,0,0,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,0,0,0,0,

    // row 9
    0,0,0,0,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,0,0,0,0,

    // row 10 - eyes
    0,0,0,0,6,6,6,6,6,6,1,1,6,6,6,6,6,6,6,6,1,1,6,6,6,6,6,6,0,0,0,0,

    // row 11 - eyes
    0,0,0,0,6,6,6,6,6,6,1,1,6,6,6,6,6,6,6,6,1,1,6,6,6,6,6,6,0,0,0,0,

    // row 12 - eyes
    0,0,0,0,6,6,6,6,6,6,1,1,6,6,6,6,6,6,6,6,1,1,6,6,6,6,6,6,0,0,0,0,

    // row 13 - eyes
    0,0,0,0,6,6,6,6,6,6,1,1,6,6,6,6,6,6,6,6,1,1,6,6,6,6,6,6,0,0,0,0,

    // row 14
    0,0,0,0,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,0,0,0,0,

    // row 15
    0,0,0,0,6,6,6,6,6,6,6,1,1,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,0,0,0,0,

    // row 16
    0,0,0,0,6,6,6,6,6,6,6,1,1,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,0,0,0,0,

    // row 17
    0,0,0,0,6,6,6,6,6,6,6,1,1,1,1,6,6,6,6,6,6,6,6,6,6,6,6,6,0,0,0,0,

    // row 18
    0,0,0,0,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,0,0,0,0,

    // row 19 - smile edges
    0,0,0,0,6,6,6,6,1,6,6,6,6,6,6,6,6,6,6,6,6,6,6,1,6,6,6,6,0,0,0,0,

    // row 20
    0,0,0,0,6,6,6,6,6,1,6,6,6,6,6,6,6,6,6,6,6,6,1,6,6,6,6,6,0,0,0,0,

    // row 21
    0,0,0,0,6,6,6,6,6,6,1,6,6,6,6,6,6,6,6,6,6,1,6,6,6,6,6,6,0,0,0,0,

    // row 22
    0,0,0,0,6,6,6,6,6,6,6,1,6,6,6,6,6,6,6,6,1,6,6,6,6,6,6,6,0,0,0,0,

    // row 23
    0,0,0,0,6,6,6,6,6,6,6,6,1,1,1,1,1,1,1,1,6,6,6,6,6,6,6,6,0,0,0,0,

    // row 24
    0,0,0,0,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,0,0,0,0,

    // row 25
    0,0,0,0,0,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,0,0,0,0,0,

    // row 26
    0,0,0,0,0,0,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,0,0,0,0,0,0,

    // row 27
    0,0,0,0,0,0,0,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,0,0,0,0,0,0,0,

    // row 28
    0,0,0,0,0,0,0,0,0,6,6,6,6,6,6,6,6,6,6,6,6,6,6,0,0,0,0,0,0,0,0,0,

    // row 29
    0,0,0,0,0,0,0,0,0,0,0,0,6,6,6,6,6,6,6,6,0,0,0,0,0,0,0,0,0,0,0,0,

    // row 30
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,

    // row 31
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};


void delay_ms(uint64_t ms)
{
    uint64_t start = time_ms();

    while ((time_ms() - start) < ms);
}

extern int load_from_sd(const char *name, void *addr);

void VDP_test()
{
    t0_map = (volatile uint16_t *) VDP_T0_MAP_BASE;
    t0_data = (volatile uint8_t *) VDP_T0_DATA_BASE;

    t1_map = (volatile uint16_t *) VDP_T1_MAP_BASE;
    t1_data = (volatile uint8_t *) VDP_T1_DATA_BASE;

    s_data = (volatile uint8_t *) VDP_S0_DATA_BASE;
    s_att =  (volatile uint64_t *) VDP_S0_ATT_BASE;

    r_map = (volatile uint32_t *) VDP_REG_BASE;

    b0_base = (volatile uint16_t *)VDP_B0_BASE;

    t0_map[0] = 0x00;
    t0_map[1] = 0x01;
    t0_map[2] = 0x02;

    for(int i = 0; i < 64; i++)
    {
        t0_data[i] = 0;
        t1_data[i] = 0;

        t0_data[64+i] = char_0[i];
        t1_data[64+i] = char_0[i];

        t0_data[128+i] = char_1[i];
        t1_data[128+i] = char_1[i];
    }

    for(int i = 0; i < 128*128; i++)
    {
        t0_map[i] = ((i/128) % 8 == 0) ? 1 : 0;
        t1_map[i] = (i % 8 == 0) ? 2 : 0;
    }

    for(int i = 0; i < 32*32; i++)
    {
        s_data[i] = sprite32_smile[i];
    }

    s_att[0] = 0x8010000000070044;

    uint16_t x = 0;
    uint16_t y = 0;
    uint16_t off = 0;
    uint16_t flags = 0x8010;

    uint64_t time;

    int x_dir = 0;
    int y_dir = 0;

    //Enable all used layers
    uint32_t ctrl = 0;
    ctrl |= (1 << VDP_BIT_CTRL_S0_EN) |
            (1 << VDP_BIT_CTRL_T0_EN) |
            (1 << VDP_BIT_CTRL_T1_EN) |
            (1 << VDP_BIT_CTRL_B0_EN) |
            (1 << VDP_BIT_CTRL_B0_LC);
    r_map[VDP_CTRL_REG] = ctrl;
    
    load_from_sd("idk.raw", (void *)b0_base);

    framebuffer_flush();
    //for(int i = 0; i < 640*480; i++)
    //{
    //    b0_base[i] = 1024-i;
    //}


    //while(1)
    //{
        if(x_dir)
            x++;
        else
            x--;

        if(y_dir)
            y++;
        else
            y--;

        if(x <= 0)
        {
            x = 0;
            x_dir = 1;
            flags &= ~0x00000004;
        } else if(x >= 640-32)
        {
            flags |= 0x04;
            x = 640-32;
            x_dir = 0;
        }

        if(y <= 0)
        {
            y = 0;
            y_dir = 1;
            flags &= ~0x00000008;
        } else if(y >= 480-32)
        {
            flags |= 0x08;
            y = 480-32;
            y_dir = 0;
        }
        

        r_map[VDP_T0_X_OFF_REG] = x;
        r_map[VDP_T0_Y_OFF_REG] = y;

        r_map[VDP_T1_X_OFF_REG] = y;
        r_map[VDP_T1_Y_OFF_REG] = x;

        
        while((r_map[VDP_STAT_REG] & 1 << VDP_BIT_STAT_V_BLK) != 0);
        while((r_map[VDP_STAT_REG] & 1 << VDP_BIT_STAT_V_BLK) == 0);
        
                                        
        s_att[0] =
            ((uint64_t)flags << 48) |
            ((uint64_t)off   << 32) |
            ((uint64_t)y     << 16) |
            ((uint64_t)x);


    //}

}

