#ifndef __SD_H__
#define __SD_H__

#include <stdint.h>
#include <stddef.h>

int sd_init();
int sd_send_cmd(uint8_t cmd, uint32_t arg, uint8_t crc, uint8_t *r1, uint8_t *resp_tail, size_t resp_tail_len);
int sd_read_block(uint32_t lba, uint8_t* buf, size_t block_count);
#endif