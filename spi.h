#ifndef __SPI_H__
#define __SPI_H__

#include <stdint.h>


#define SPI_SD_DEV 0
//-----------------------------------------------------------------
// Prototypes
//-----------------------------------------------------------------
void spi_init(uint32_t base_addr);
void spi_select(uint8_t slave_num);
void spi_deselect();
uint8_t spi_transfer(uint8_t tx_data);



#endif