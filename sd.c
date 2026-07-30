#include "sd.h"
#include "spi.h"
#include "rdtime.h"
#include "serial.h"

#define SD_CMD0 0x40
#define SD_CMD0_CRC 0x95
#define SD_CMD1 0x41
#define SD_CMD8 0x48
#define SD_CMD8_ARG 0x000001AA
#define SD_CMD8_CRC 0x87

#define SD_CMD55 0x77
#define SD_CMD55_ARG 0x00
#define SD_ACMD41 0x69
#define SD_ACMD41_ARG 0x40000000

#define SD_CMD58 0x7a
#define SD_CMD58_ARG 0x00000000

#define SD_CMD18 0x52
#define SD_CMD12 0x4C
#define SD_CMD17 0x51

int sd_init()
{
    uint8_t r1;
    int resp;
    uint64_t time;

    time = time_ms();
    spi_deselect();
    while(time_ms() < time + 2); //Wait 2 ms for SD card

    
    //Pump sck
    for(int i = 0; i < 10; i++) {
        spi_transfer(0xFF); //80 pulses CS high MOSI high
    }
    
    
    resp = sd_send_cmd(SD_CMD0, 0x00, SD_CMD0_CRC, &r1, NULL, 0);
    if(resp)
    {
        serial_putstr("\nFaild to send CMD0!");
        return -1;
    }
    if(r1 == 0x01){
        serial_putstr("\nSD card is in IDLE");
    } else {
        serial_putstr("\nSD didnt go into idle - error! ):");
        return -2;
        
    }

    uint8_t r7_tail[4];
    resp = sd_send_cmd(SD_CMD8, SD_CMD8_ARG, SD_CMD8_CRC, &r1, r7_tail, sizeof(r7_tail));
    if(resp)
    {
        serial_putstr("\nFaild to send CMD8!");
        return -3;
    }

    if(r1 != 0x01)
    {
        serial_putstr_hex("\nFaild to send CMD8! r1: ",r1);
        return -4;
    }
    uint32_t r7_resp =
    ((uint32_t)r7_tail[0] << 24) |
    ((uint32_t)r7_tail[1] << 16) |
    ((uint32_t)r7_tail[2] << 8)  |
    ((uint32_t)r7_tail[3]);

    if(r7_resp != 0x1aa)
    {
        serial_putstr_hex("\nFaild to send CMD8! invalid r7 tail: ",r7_resp);
        return -5;
    }

    
    //Wait to get out of IDLE
    do{
        resp = sd_send_cmd(SD_CMD55, SD_CMD55_ARG, 0xFF, &r1, NULL, 0);

        if(resp || (r1 != 0x01 && r1 != 0x00))
        {
            serial_putstr_hex("\nFailed to send CMD55! r1 = ",r1);
            return -6;
        }
        resp = sd_send_cmd(SD_ACMD41, SD_ACMD41_ARG, 0xFF, &r1, NULL, 0);
        if(resp || (r1 != 0x01 && r1 != 0x00))
        {
            serial_putstr_hex("\nFailed to send ACMD41! r1 = ",r1);
            return -7;
        }
    }while(r1 == 0x01);
    if(r1 == 0x00)
    {
        serial_putstr("\nSD card is out of IDLE!");
    } else {
        serial_putstr_hex("\nSD card ACMD41 failed: r1 = ",r1);
    }
    resp = sd_send_cmd(SD_CMD58, SD_CMD58_ARG, 0xFF, &r1, r7_tail, sizeof(r7_tail));
    if(resp || r1 != 0x00)
    {
        serial_putstr_hex("\nFailed to send ACMD58! r1 = ",r1);
        return -8;
    }

    r7_resp =
    ((uint32_t)r7_tail[0] << 24) |
    ((uint32_t)r7_tail[1] << 16) |
    ((uint32_t)r7_tail[2] << 8)  |
    ((uint32_t)r7_tail[3]);

    

    if((r7_resp & 0x40000000) == 0)
    {
        //Unsuported SDSC
        serial_putstr_hex("\nUnsupported SDSC card detected!\nOnly SDHC or SDXC cards are supported OCR = ",r7_resp);
        return -9;
    }else{
        //Supported!
        serial_putstr_hex("\nDetected supported SDHC or SDXC card, OCR = ",r7_resp);
    }

    serial_putstr("Done!");
    return 0;
}


int sd_send_cmd(uint8_t cmd, uint32_t arg, uint8_t crc, uint8_t *r1, uint8_t *resp_tail, size_t resp_tail_len)
{
    spi_select(SPI_SD_DEV);
    //Illegal parameters chck
    if(r1 == NULL)
    {
        return -1;
    }

    if(resp_tail_len != 0 && resp_tail == NULL)
    {
        return -1;
    }

    //Start SPI transfer
    spi_transfer( 0x40 | (cmd & 0x3F)); //Send command index

    //Send command frame
    spi_transfer((uint8_t)(arg >> 24));
    spi_transfer((uint8_t)(arg >> 16));
    spi_transfer((uint8_t)(arg >> 8));
    spi_transfer((uint8_t)(arg));
    spi_transfer(crc);

    //Wait R1 resp
    uint64_t time = time_ms();
    uint8_t data = 0;
    //Timout after 100ms
    while(time_ms() < time + 100)
    { 
        data = spi_transfer(0xFF);
        if((data & 0x80) == 0)    //Got valid R1
            break;
    }

    

    if(data == 0xFF)
    {
        return -1; //Timeout
    }
    //Fill r1 parameter with R1

    *r1 = data;
    //want after R1
    if(resp_tail_len != 0)
    {
        for(int n = 0; n < resp_tail_len; n++)
        {
            resp_tail[n] = spi_transfer(0xFF);
        }
    
    }
    spi_deselect();
    
    spi_transfer(0xFF); //IDK why this is needed

    return 0;
}

int sd_send_cmd_no_sel(uint8_t cmd, uint32_t arg, uint8_t crc, uint8_t *r1, uint8_t *resp_tail, size_t resp_tail_len)
{

    //Illegal parameters chck
    if(r1 == NULL)
    {
        return -1;
    }

    if(resp_tail_len != 0 && resp_tail == NULL)
    {
        return -1;
    }

    //Start SPI transfer
    spi_transfer( 0x40 | (cmd & 0x3F)); //Send command index

    //Send command frame
    spi_transfer((uint8_t)(arg >> 24));
    spi_transfer((uint8_t)(arg >> 16));
    spi_transfer((uint8_t)(arg >> 8));
    spi_transfer((uint8_t)(arg));
    spi_transfer(crc);

    //Wait R1 resp
    uint64_t time = time_ms();
    uint8_t data = 0;
    //Timout after 100ms
    while(time_ms() < time + 100)
    { 
        data = spi_transfer(0xFF);
        if((data & 0x80) == 0)    //Got valid R1
            break;
    }

    

    if(data == 0xFF)
    {
        return -1; //Timeout
    }
    //Fill r1 parameter with R1

    *r1 = data;
    //want after R1
    if(resp_tail_len != 0)
    {
        for(int n = 0; n < resp_tail_len; n++)
        {
            resp_tail[n] = spi_transfer(0xFF);
        }
    
    }
    
    spi_transfer(0xFF); //IDK why this is needed

    return 0;
}

int sd_read_block_single(uint32_t lba, uint8_t *buf)
{
    uint8_t resp;
    uint8_t r1;
    uint64_t time;

    spi_select(SPI_SD_DEV);
    resp = sd_send_cmd_no_sel(SD_CMD17, lba, 0xFF, &r1, NULL, 0);
    if(resp)
    {
        serial_putstr_hex("\nResp err CMD17 : ",resp);
        spi_deselect();
        return -1;
    }
    if(r1)
    {
        serial_putstr_hex("\nR1 err CMD17 : ",r1);
        spi_deselect();
        return -2;
    }

    time = time_ms();
    while(time_ms() < time + 100){
        resp = spi_transfer(0xFF);
        if(resp != 0xFF)
            break;
    }

     if(resp == 0xFF) 
    {
        serial_putstr("\nTimeout");
        spi_deselect();
        return -3; //Timeout
    } else if(resp != 0xFE )
    {
        serial_putstr_hex("\nResp expected 0xFE got : ",resp);
        spi_deselect();
        return -4;
    }

    for(int i = 0; i < 512; i++)
    {
        *buf++ = spi_transfer(0xFF);
    }
    //CRC dont care
    spi_transfer(0xFF);
    spi_transfer(0xFF);

    spi_deselect();
    spi_transfer(0xFF);
    return 0;


}

int sd_read_block(uint32_t lba, uint8_t* buf, size_t block_count)
{
    uint8_t resp;
    uint8_t r1;
    uint64_t time;

    if(block_count == 1)
        return sd_read_block_single(lba, buf);
    spi_select(SPI_SD_DEV);
    resp = sd_send_cmd_no_sel(SD_CMD18, lba, 0xFF, &r1, NULL, 0);
    if(resp)
    {
        serial_putstr_hex("\nResp err CMD18 : ",resp);
        spi_deselect();
        return -1;
    }
    if(r1)
    {
        serial_putstr_hex("\nR1 err CMD18 : ",r1);
        spi_deselect();
        return -2;
    }

    for(int i = 0; i < block_count; i++)
    {
        //Wait for data token, 0xFE for CMD18 with timeout of 100ms
        time = time_ms();
        while(time_ms() < time + 100){
            resp = spi_transfer(0xFF);
            if(resp != 0xFF)
                break;
        }
        if(resp == 0xFF) 
        {
            serial_putstr("\nTimeout");
            spi_deselect();
            return -3; //Timeout
        } else if(resp != 0xFE )
        {
            serial_putstr_hex("\nResp expected 0xFE got : ",resp);
            spi_deselect();
            return -4;
        }
        //Everyting is ok
        for(int n = 0; n < 512; n++)
        {
            //Read 512 bytes int buf and increment buf
            *buf++ = spi_transfer(0xFF);
        }
        //CRC dont care
        spi_transfer(0xFF);
        spi_transfer(0xFF);

    }

    //End transfer
    sd_send_cmd_no_sel(SD_CMD12, 0x00, 0xFF, &r1, NULL, 0x00);

    if(r1 != 0x00)
    {
        serial_putstr_hex("\nR1 error got : ",r1);
        spi_deselect();
        return -5;
    }
    //wait for not bussy
    time = time_ms();
    while(time_ms() < time + 100){
        resp = spi_transfer(0xFF);
        if(resp == 0xFF)
            break;
    }
    if(resp != 0xFF)
    {
        serial_putstr_hex("\nBussy timout : ",resp);
        spi_deselect();
        return -6;
    }
    spi_deselect();
    spi_transfer(0xFF);
    return 0;

}

