/*
 * Copyright (C) 2017 XRADIO TECHNOLOGY CO., LTD. All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the
 *       distribution.
 *    3. Neither the name of XRADIO TECHNOLOGY CO., LTD. nor the names of
 *       its contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DRV_XR829_BT_PRIV_H_
#define DRV_XR829_BT_PRIV_H_

#define BT_UART_NORMAL_BAUDRATE   (115200)
#define BT_UART_DOWNLOAD_BAUDRATE (1500000)

#define BT_LOAD_ADDR              (0x0)
#define BT_JUMP_ADDR              (0x0)
#define BT_STARTUP_RESET_FLAG     (1)
#define BT_UPDATE_HCIRATE_FLAG    (1)
#define BT_BDADDR_FLAG            (1)
#define BT_DUMP_DEBUG             (0)

#define SWAP16(d) (((d & 0xff) << 8) | ((d & 0xff00) >> 8))
#define SWAP32(d) (((d & 0xff) << 24) | ((d & 0xff00) << 8)  \
    | ((d & 0xff0000) >> 8) | ((d & 0xff000000) >> 24))

#define FILL_HEADER_MAGIC(h) do { \
  (h)->magic[0] = 'B'; \
  (h)->magic[1] = 'R'; \
  (h)->magic[2] = 'O'; \
  (h)->magic[3] = 'M'; \
} while(0)

#define HEADER_MAGIC_VALID(h) ( \
    (h)->magic[0] == 'B' && \
    (h)->magic[1] == 'R' && \
    (h)->magic[2] == 'O' && \
    (h)->magic[3] == 'M' )

#define FILL_HEADER_CHECKSUM(h, cs) do { \
  (h)->checksum = cs; \
} while(0)

#define UART_BUF_RXCLEAR      (1<<0)
#define UART_BUF_TXCLEAR      (1<<1)

#define SZ_512                (0x00000200U)
#define SZ_1K                 (0x00000400U)
#define SZ_2K                 (0x00000800U)
#define SZ_4K                 (0x00001000U)
#define SZ_8K                 (0x00002000U)
#define SZ_16K                (0x00004000U)
#define SZ_32K                (0x00008000U)
#define SZ_64K                (0x00010000U)
#define SZ_128K               (0x00020000U)
#define SZ_256K               (0x00040000U)
#define SZ_512K               (0x00080000U)
#define SZ_1M                 (0x00100000U)
#define SZ_2M                 (0x00200000U)
#define SZ_4M                 (0x00400000U)
#define SZ_8M                 (0x00800000U)
#define SZ_16M                (0x01000000U)
#define SZ_32M                (0x02000000U)
#define SZ_64M                (0x04000000U)
#define SZ_128M               (0x08000000U)
#define SZ_256M               (0x10000000U)
#define SZ_512M               (0x20000000U)
#define SZ_1G                 (0x40000000U)
#define SZ_2G                 (0x80000000U)
#define SZ_4G                 (0x0100000000ULL)
#define SZ_8G                 (0x0200000000ULL)
#define SZ_16G                (0x0400000000ULL)
#define SZ_32G                (0x0800000000ULL)
#define SZ_64G                (0x1000000000ULL)

#define CMD_REVESION          (0x0000) /* 0.0.0.0 */
#define CMD_SYNC_WORD         (0x55)
#define CMD_ID(group, key)    (((group) << 3) | (key))

/*----------------------------*/
/*   COMMANDS FORM PC TO MCU  */
/*----------------------------*/
#define CMD_ID_MEMRW          (0x00)
#define CMD_ID_SEQRQ          (0x01)
#define CMD_ID_SYSCTL         (0x02)
#define CMD_ID_FLASH          (0x03)
/* memory access commands */
#define CMD_ID_READ1          CMD_ID(CMD_ID_MEMRW, 0)
#define CMD_ID_WRITE1         CMD_ID(CMD_ID_MEMRW, 1)
#define CMD_ID_READ2          CMD_ID(CMD_ID_MEMRW, 2)
#define CMD_ID_WRITE2         CMD_ID(CMD_ID_MEMRW, 3)
#define CMD_ID_READ4          CMD_ID(CMD_ID_MEMRW, 4)
#define CMD_ID_WRITE4         CMD_ID(CMD_ID_MEMRW, 5)
#define CMD_ID_READ8          CMD_ID(CMD_ID_MEMRW, 6)
#define CMD_ID_WRITE8         CMD_ID(CMD_ID_MEMRW, 7)

#define CMD_ID_SEQRD          CMD_ID(CMD_ID_SEQRQ, 0)
#define CMD_ID_SEQWR          CMD_ID(CMD_ID_SEQRQ, 1)
/* uart commands */
#define CMD_ID_SETUART        CMD_ID(CMD_ID_SYSCTL, 0)
#define CMD_ID_SETJTAG        CMD_ID(CMD_ID_SYSCTL, 1)
#define CMD_ID_REBOOT         CMD_ID(CMD_ID_SYSCTL, 2)
#define CMD_ID_SETPC          CMD_ID(CMD_ID_SYSCTL, 3)
#define CMD_ID_SETCKCS        CMD_ID(CMD_ID_SYSCTL, 4)
/* flash operation commands */
#define CMD_ID_FLASH_GETINFO  CMD_ID(CMD_ID_FLASH, 0)
#define CMD_ID_FLASH_ERASE    CMD_ID(CMD_ID_FLASH, 1)
#define CMD_ID_FLASH_READ     CMD_ID(CMD_ID_FLASH, 2)
#define CMD_ID_FLASH_WRITE    CMD_ID(CMD_ID_FLASH, 3)

/*----------------------------*/
/*   COMMANDS FORM MCU TO PC  */
/*----------------------------*/
/* message output */
#define CMD_ID_SENDMSG        CMD_ID(0, 0)

#pragma pack(1)
/* command header
 *
 *    byte 0    byte 1    byte 2   byte 3     byte 4    byte 5    byte 6 -7          byte 8-11
 *  ___________________________________________________________________________________________________
 * |         |         |         |         |         |         |                   |                   |
 * |   'B'   |   'R'   |   'O'   |   'M'   |  Flags  |Reserved | Checksum          | Playload Length   |
 * |_________|_________|_________|_________|_________|_________|__________ ________|___________________|
 */
typedef struct {
  unsigned char magic[4]; /* magic "BROM" */
#define CMD_BROM_MAGIC    "BROM"
  unsigned char flags;
#define CMD_HFLAG_ERROR   (0x1U << 0)
#define CMD_HFLAG_ACK     (0x1U << 1)
#define CMD_HFLAG_CHECK   (0x1U << 2)
#define CMD_HFLAG_RETRY   (0x1U << 3)
#define CMD_HFLAG_EXE     (0x1U << 4)
  unsigned char version:4;
  unsigned char reserved:4;
  unsigned short checksum;
  unsigned int payload_len;
} __attribute__((packed)) cmd_header_t;
#define MB_CMD_HEADER_SIZE (sizeof(cmd_header_t))

typedef struct {
  cmd_header_t h;
  unsigned char cmdid;
} __attribute__((packed)) cmd_header_id_t;
#define MB_HEADER_TO_ID(h) (((cmd_header_id_t*)(h))->cmdid)

/* acknownledge structure */
typedef struct {
  cmd_header_t h;
  unsigned char err;
} __attribute__((packed)) cmd_ack_t;

/* memory read/write command structure */
#define CMD_RW_DATA_POS (MB_CMD_HEADER_SIZE + 5)
#define CMD_RW_DATA_LEN(id) (1 << ((id >> 1) & 0x3))
typedef struct {
  cmd_header_t h;
  unsigned char cmdid;
  unsigned int addr;
} __attribute__((packed)) cmd_rw_t;

/* sequence read/write command structure */
#define CMD_SEQRW_DATA_POS (MB_CMD_HEADER_SIZE + 5)
typedef struct {
  cmd_header_t h;
  unsigned char cmdid;
  unsigned int addr;
  unsigned int dlen;
  unsigned short dcs;
} __attribute__((packed)) cmd_seq_wr_t;

typedef struct {
  cmd_header_t h;
  unsigned char cmdid;
  unsigned int addr;
  unsigned int dlen;
} __attribute__((packed)) cmd_seq_rd_t;

/* io change command structure */
#define CMD_SYS_DATA_POS (MB_CMD_HEADER_SIZE + 1)
typedef struct {
  cmd_header_t h;
  unsigned char cmdid;
  unsigned int val;
} __attribute__((packed)) cmd_sys_t;

typedef struct {
  cmd_header_t h;
  unsigned char cmdid;
  unsigned int lcr;
} __attribute__((packed)) cmd_sys_setuart_t;

typedef struct {
  cmd_header_t h;
  unsigned char cmdid;
  unsigned char mode;
} __attribute__((packed)) cmd_sys_setjtag_t;

typedef struct {
  cmd_header_t h;
  unsigned char cmdid;
  unsigned char mode;
} __attribute__((packed)) cmd_sys_setcs_t;

/* flash command structure */
typedef struct {
  cmd_header_t h;
  unsigned char cmdid;
  unsigned char erase_cmd;
  unsigned int addr;
} __attribute__((packed)) cmd_flash_er_t;

typedef struct {
  cmd_header_t h;
  unsigned char cmdid;
  unsigned int sector;
  unsigned int num;
  unsigned short dcs;
} __attribute__((packed)) cmd_flash_wr_t;

typedef struct {
  cmd_header_t h;
  unsigned char cmdid;
  unsigned int sector;
  unsigned int num;
} __attribute__((packed)) cmd_flash_rd_t;

#pragma pack()

/* response error type */
#define MB_ERR_UNKNOWNCMD (1)
#define MB_ERR_TIMEOUT    (2)
#define MB_ERR_CHECKSUM   (3)
#define MB_ERR_INVALID    (4)
#define MB_ERR_NOMEM      (5)

#endif /* DRV_XR829_BT_PRIV_H_ */
