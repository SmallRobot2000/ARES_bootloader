//Simple SPI driver for Xilinix QSPI controller in standard master mode

#include "spi.h"


//Write only
#define  XSPI_RESET 0x40
    #define XSPI_RST_CONST 0x0000000a

//Read Write
#define XSPI_CONTROL 0x60
    //Loop mode
    #define XSPI_CONTROL_LOOP_SHIFT          0 
    //SPI enable core
    #define XSPI_CONTROL_SPE_SHIFT           1
    //SPI master mode
    #define XSPI_CONTROL_MASTER_SHIFT        2
    //CPOL clock polarity -> 0 - SCK IDLE low, 1 - SCK IDLE high
    #define XSPI_CONTROL_CPOL_SHIFT          3
    //CPHA clock phase -> 0 - data valid on first SCK edge, 1 - data valid on second SCK edge
    #define XSPI_CONTROL_CPHA_SHIFT          4
    //TX FIFO reset
    #define XSPI_CONTROL_TX_RST_SHIFT        5
    //RX FIFO reset
    #define XSPI_CONTROL_RX_RST_SHIFT        6
    //Manual slave select logic
    #define XSPI_CONTROL_MAN_SS_SHIFT        7
    //Master transaction inhibit
    #define XSPI_CONTROL_MTI_SHIFT           8
    //LSB first
    #define XSPI_CONTROL_LSB_SHIFT           9

//Read only
#define XSPI_STATUS 0x64
    #define XSPI_STATUS_RX_EMPTY_MASK        0x001   
    #define XSPI_STATUS_RX_FULL_MASK         0x002
    #define XSPI_STATUS_TX_EMPTY_MASK        0x004
    #define XSPI_STATUS_TX_FULL_MASK         0x008
    //This flag is set if the SS signal goes active while the SPI deviceis configured as a master.
    #define XSPI_STATUS_MODF_MASK            0x010
    //This flag is asserted when the core is configured in slavemode.
    #define XSPI_STATUS_SLAVE_SEL_MASK       0x020
    //The CPOL and CPHA are set to 01 or 10
    #define XSPI_STATUS_CPOL_CPHA_ERR_MASK   0x040
    //This bit is set when the core is configured with dual orquad SPI mode and the master is set to 0 in the controlregister 
    #define XSPI_STATUS_SLAVE_ERR_MASK       0x080
    //This bit is set when the core is configured to transferthe SPI transactions in either dual or quad SPI mode andLSB first bit is set in the control register
    #define XSPI_STATUS_MSB_ERR_MASK         0x100
    //When the SPI command, address, and data bits are setto be transferred in other than standard SPI protocol modeand this bit is set in control register (SPICR)
    #define XSPI_STATUS_LOOP_ERR_MASK        0x200
    //When the core is configured in dual/quad SPI modeand the first entry in the SPI DTR FIFO (after reset) do notmatch with the supported command list for the particularmemory, this bit is set.
    #define XSPI_STATUS_CMD_ERR_MASK         0x400

//Write only 
#define XSPI_TX_DATA 0x68
    #define XSPI_TX_DATA_SHIFT               0
    #define XSPI_TX_DATA_MASK                0x0f

//Read only 
#define XSPI_RX_DATA 0x6C
    #define XSPI_RX_DATA_SHIFT               0
    #define XSPI_RX_DATA_MASK                0x0f


#define XSPI_SLAVE_SEL 0x70


#define XSPI_TX_OCCUPANCY 0x74
    #define XSPI_TX_OCCUPANCY_MASK           0x0f

#define XSPI_RX_OCCUPANCY 0x78
    #define XSPI_RX_OCCUPANCY_MASK           0x0f

//Global interrupt enable
#define XSPI_GIE 0x1C
    #define XSPI_GIE_SHIFT                   31
    #define XSPI_GIE_MASK                    0x80000000

//No interrupts for now, TODO: Add int support

//Doesnt work - timing
#define GPIO_SCK_SEL_DATA 0x00
#define GPIO_SCK_SEL_TRI 0x04
static volatile uint32_t *m_spi;

//-----------------------------------------------------------------
// spi_init: Initialise SPI peripheral
//-----------------------------------------------------------------
void spi_init(uint32_t base_addr_xspi)           
{
    uint32_t cfg = 0;
    m_spi = (volatile uint32_t *)base_addr_xspi;

    m_spi[XSPI_RESET/4] = XSPI_RST_CONST; //Soft reset the core
    cfg += (1 << XSPI_CONTROL_SPE_SHIFT);
    cfg += (1 << XSPI_CONTROL_MASTER_SHIFT);
    //CPOL = 0 and CPHA = 0 -> mode 0
    cfg += (1 << XSPI_CONTROL_MAN_SS_SHIFT); //Manual SS logic
    //MSB first no master inhibit
    m_spi[XSPI_CONTROL/4] = cfg;



}

//-----------------------------------------------------------------
// spi_select: Select SPI device slave_num
//-----------------------------------------------------------------
void spi_select(uint8_t slave_num)
{
    uint32_t cfg = 0;
    cfg = ~((1 << slave_num));
    m_spi[XSPI_SLAVE_SEL/4] = cfg;

}
//-----------------------------------------------------------------
// spi_deselect: Deselect all SPI devices
//-----------------------------------------------------------------
void spi_deselect()
{
    m_spi[XSPI_SLAVE_SEL/4] = 0xFFFFFFFF;
}

uint8_t spi_transfer(uint8_t tx_data)
{
    uint8_t rx_data;
    //Wait for TX and RX to not be full
    while(!(m_spi[XSPI_STATUS/4] & XSPI_STATUS_TX_EMPTY_MASK) || !(m_spi[XSPI_STATUS/4] & XSPI_STATUS_RX_EMPTY_MASK));

    //Transfer TX data
    m_spi[XSPI_TX_DATA/4] = (uint32_t)tx_data;
    //Wait for RX
    while((m_spi[XSPI_STATUS/4] & XSPI_STATUS_RX_EMPTY_MASK));
    rx_data = (uint8_t)m_spi[XSPI_RX_DATA/4];

    
    return rx_data;
}

