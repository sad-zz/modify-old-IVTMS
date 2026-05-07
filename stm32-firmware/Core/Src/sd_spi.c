/**
 * @file sd_spi.c
 * @brief SD/MMC card driver – SPI mode block-level I/O
 *
 * مرجع پروتکل: SD Specification Part-1 Physical Layer v3.01
 *              SimplifiedSpec_PartA2_Phys_v3_01.pdf §7
 *
 * این فایل مستقل است: فقط به HAL SPI و GPIO نیاز دارد.
 *
 * نکات پیاده‌سازی:
 *   - timeout های conservative (1s) تا روی کارت‌های کند (microSD ارزان) هم کار کند.
 *   - تشخیص خودکار SDv1 / SDv2 / SDHC.
 *   - برای SDSC آدرس بایتی، برای SDHC آدرس بلاک‌اساس استفاده می‌شود.
 *   - بعد از init، سرعت SPI افزایش پیدا می‌کند (در main.c با
 *     sd_set_high_speed() یا با تنظیم BR در hspi2).
 */

#include "sd_spi.h"
#include "stm32f1xx_hal.h"
#include <string.h>

/* ─── Extern HAL handle (defined in main.c) ──────────────────────────── */
extern SPI_HandleTypeDef hspi2;

/* ─── /CS GPIO ───────────────────────────────────────────────────────── */
#define SD_CS_PORT       GPIOB
#define SD_CS_PIN        GPIO_PIN_12

/* ─── SD command set (SPI mode) ──────────────────────────────────────── */
#define CMD0    (0)         /* GO_IDLE_STATE                             */
#define CMD1    (1)         /* SEND_OP_COND (MMC)                        */
#define ACMD41  (0x80 + 41) /* SD_SEND_OP_COND (SDC)                     */
#define CMD8    (8)         /* SEND_IF_COND                              */
#define CMD9    (9)         /* SEND_CSD                                  */
#define CMD10   (10)        /* SEND_CID                                  */
#define CMD12   (12)        /* STOP_TRANSMISSION                         */
#define CMD16   (16)        /* SET_BLOCKLEN                              */
#define CMD17   (17)        /* READ_SINGLE_BLOCK                         */
#define CMD18   (18)        /* READ_MULTIPLE_BLOCK                       */
#define CMD23   (23)        /* SET_BLOCK_COUNT (MMC)                     */
#define ACMD23  (0x80 + 23) /* SET_WR_BLK_ERASE_COUNT (SDC)              */
#define CMD24   (24)        /* WRITE_BLOCK                               */
#define CMD25   (25)        /* WRITE_MULTIPLE_BLOCK                      */
#define CMD55   (55)        /* APP_CMD                                   */
#define CMD58   (58)        /* READ_OCR                                  */

/* ─── Data tokens ────────────────────────────────────────────────────── */
#define TOKEN_SINGLE_READ_WRITE 0xFEU
#define TOKEN_MULTI_WRITE       0xFCU
#define TOKEN_STOP_TRAN         0xFDU

/* ─── Module state ───────────────────────────────────────────────────── */
static uint8_t  card_type;
static uint32_t card_sectors;

/* ─── Low-level SPI / CS helpers ─────────────────────────────────────── */
static inline void cs_low(void)
{
    HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_RESET);
}

static inline void cs_high(void)
{
    HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET);
}

static uint8_t spi_xfer(uint8_t b)
{
    uint8_t rx = 0xFF;
    HAL_SPI_TransmitReceive(&hspi2, &b, &rx, 1, 200);
    return rx;
}

static void spi_send_dummy(uint16_t n)
{
    uint8_t ff = 0xFF;
    while (n--) {
        HAL_SPI_Transmit(&hspi2, &ff, 1, 200);
    }
}

/* ─── Wait for the card to be ready (DO line returns 0xFF) ───────────── */
static int wait_ready(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    uint8_t  d;
    do {
        d = spi_xfer(0xFF);
        if (d == 0xFF) return 0;
    } while ((HAL_GetTick() - start) < timeout_ms);
    return -1;
}

/* ─── Deselect and pulse 8 clocks so the card releases DO ────────────── */
static void deselect(void)
{
    cs_high();
    spi_xfer(0xFF);
}

/* ─── Select; on failure the caller must deselect again ──────────────── */
static int select_card(void)
{
    cs_low();
    spi_xfer(0xFF);
    if (wait_ready(500) == 0) return 0;
    deselect();
    return -1;
}

/* ─── Send a command, return R1 response byte ────────────────────────── */
static uint8_t send_cmd(uint8_t cmd, uint32_t arg)
{
    uint8_t  n, res;

    /* ACMD<n> = CMD55 + CMD<n> */
    if (cmd & 0x80) {
        cmd &= 0x7F;
        res = send_cmd(CMD55, 0);
        if (res > 1) return res;
    }

    /* Re-select */
    if (cmd != CMD12) {
        deselect();
        if (select_card() < 0) return 0xFF;
    }

    /* Build and send the 6-byte command frame */
    uint8_t buf[6];
    buf[0] = 0x40 | cmd;
    buf[1] = (uint8_t)(arg >> 24);
    buf[2] = (uint8_t)(arg >> 16);
    buf[3] = (uint8_t)(arg >> 8);
    buf[4] = (uint8_t)(arg);
    /* CRC is required for CMD0 (0x95) and CMD8 (0x87); ignored otherwise. */
    if      (cmd == CMD0) buf[5] = 0x95;
    else if (cmd == CMD8) buf[5] = 0x87;
    else                  buf[5] = 0x01;
    HAL_SPI_Transmit(&hspi2, buf, 6, 200);

    /* CMD12: skip one stuff byte */
    if (cmd == CMD12) spi_xfer(0xFF);

    /* Wait up to 10 bytes for a valid R1 response */
    n = 10;
    do {
        res = spi_xfer(0xFF);
    } while ((res & 0x80) && --n);

    return res;
}

/* ─── Read a data packet (TOKEN + N bytes + 2-byte dummy CRC) ────────── */
static int read_data_packet(uint8_t *buf, uint16_t len)
{
    uint8_t  tok;
    uint32_t start = HAL_GetTick();
    do {
        tok = spi_xfer(0xFF);
        if ((HAL_GetTick() - start) > 500) return -1;
    } while (tok == 0xFF);

    if (tok != TOKEN_SINGLE_READ_WRITE) return -1;

    /* Read len bytes */
    while (len--) *buf++ = spi_xfer(0xFF);

    /* Discard 2-byte CRC */
    spi_xfer(0xFF);
    spi_xfer(0xFF);
    return 0;
}

/* ─── Write a data packet (TOKEN + N bytes + dummy CRC) ──────────────── */
static int write_data_packet(const uint8_t *buf, uint8_t token)
{
    if (wait_ready(500) < 0) return -1;

    spi_xfer(token);

    if (token != TOKEN_STOP_TRAN) {
        for (uint16_t i = 0; i < 512; i++) spi_xfer(buf[i]);
        spi_xfer(0xFF);   /* dummy CRC */
        spi_xfer(0xFF);

        uint8_t resp = spi_xfer(0xFF);
        if ((resp & 0x1F) != 0x05) return -1;  /* not "data accepted"  */
    }
    return 0;
}

/* ─── sd_init ────────────────────────────────────────────────────────── */
int sd_init(void)
{
    uint8_t  n, cmd, type, ocr[4];
    uint32_t start;

    card_type    = SD_TYPE_NONE;
    card_sectors = 0;

    /* 80 dummy clocks with /CS high to wake the card up */
    cs_high();
    spi_send_dummy(10);

    /* Enter idle state */
    if (send_cmd(CMD0, 0) != 0x01) {
        deselect();
        return -1;
    }

    type = 0;
    if (send_cmd(CMD8, 0x1AA) == 0x01) {
        /* SDv2: read 32-bit r7 (R1 + 4 bytes) */
        for (n = 0; n < 4; n++) ocr[n] = spi_xfer(0xFF);
        if (ocr[2] == 0x01 && ocr[3] == 0xAA) {
            /* The card supports VDD 2.7-3.6V */
            start = HAL_GetTick();
            while ((HAL_GetTick() - start) < 1000 && send_cmd(ACMD41, 0x40000000));

            if ((HAL_GetTick() - start) < 1000 && send_cmd(CMD58, 0) == 0) {
                for (n = 0; n < 4; n++) ocr[n] = spi_xfer(0xFF);
                /* Bit 30 = CCS: 1=SDHC/SDXC, 0=SDSC */
                type = (ocr[0] & 0x40) ? (SD_TYPE_SDV2 | SD_TYPE_BLOCK)
                                       :  SD_TYPE_SDV2;
            }
        }
    } else {
        /* SDv1 or MMC – try ACMD41 first */
        if (send_cmd(ACMD41, 0) <= 1) {
            type = SD_TYPE_SDV1;
            cmd  = ACMD41;
        } else {
            type = SD_TYPE_MMC;
            cmd  = CMD1;
        }
        start = HAL_GetTick();
        while ((HAL_GetTick() - start) < 1000 && send_cmd(cmd, 0));
        if ((HAL_GetTick() - start) >= 1000 || send_cmd(CMD16, 512) != 0) {
            type = 0;
        }
    }
    card_type = type;
    deselect();

    if (type == 0) return -1;

    /* Read CSD to compute total sector count */
    uint8_t csd[16];
    if (select_card() < 0) return -1;
    if (send_cmd(CMD9, 0) == 0 && read_data_packet(csd, 16) == 0) {
        if ((csd[0] >> 6) == 1) {
            /* CSD v2.0 (SDHC/SDXC): C_SIZE in bits [69:48] of csd[7..9] */
            uint32_t c_size = ((uint32_t)(csd[7] & 0x3F) << 16) |
                              ((uint32_t)csd[8] << 8) |
                              csd[9];
            card_sectors = (c_size + 1) * 1024UL;   /* in 512-byte units */
        } else {
            /* CSD v1.0 (SDSC) */
            uint32_t c_size      = (((uint32_t)(csd[6] & 0x03)) << 10) |
                                   (((uint32_t)csd[7]) << 2) |
                                   (((uint32_t)csd[8]) >> 6);
            uint8_t  c_size_mult = (uint8_t)(((csd[9] & 0x03) << 1) |
                                             ((csd[10] & 0x80) >> 7));
            uint8_t  read_bl_len = csd[5] & 0x0F;
            uint32_t blocknr     = (c_size + 1) * (1UL << (c_size_mult + 2));
            uint32_t block_len   = 1UL << read_bl_len;
            card_sectors = blocknr * block_len / 512UL;
        }
    }
    deselect();
    return 0;
}

/* ─── sd_deinit ──────────────────────────────────────────────────────── */
void sd_deinit(void)
{
    deselect();
    card_type    = SD_TYPE_NONE;
    card_sectors = 0;
}

/* ─── sd_get_type / sd_get_sector_count ──────────────────────────────── */
uint8_t  sd_get_type(void)         { return card_type;    }
uint32_t sd_get_sector_count(void) { return card_sectors; }

/* ─── sd_read ────────────────────────────────────────────────────────── */
int sd_read(uint32_t lba, uint8_t *buf, uint32_t count)
{
    if (!card_type || count == 0) return -1;

    /* SDSC uses byte addressing */
    if (!(card_type & SD_TYPE_BLOCK)) lba *= 512;

    if (count == 1) {
        if (send_cmd(CMD17, lba) == 0 && read_data_packet(buf, 512) == 0)
            count = 0;
    } else {
        if (send_cmd(CMD18, lba) == 0) {
            do {
                if (read_data_packet(buf, 512) != 0) break;
                buf += 512;
            } while (--count);
            send_cmd(CMD12, 0);
        }
    }
    deselect();
    return (count == 0) ? 0 : -1;
}

/* ─── sd_write ───────────────────────────────────────────────────────── */
int sd_write(uint32_t lba, const uint8_t *buf, uint32_t count)
{
    if (!card_type || count == 0) return -1;

    if (!(card_type & SD_TYPE_BLOCK)) lba *= 512;

    if (count == 1) {
        if (send_cmd(CMD24, lba) == 0 &&
            write_data_packet(buf, TOKEN_SINGLE_READ_WRITE) == 0)
            count = 0;
    } else {
        if (card_type & SD_TYPE_SDV2) send_cmd(ACMD23, count);
        if (send_cmd(CMD25, lba) == 0) {
            do {
                if (write_data_packet(buf, TOKEN_MULTI_WRITE) != 0) break;
                buf += 512;
            } while (--count);
            /* Stop-tran token; ignore the response byte */
            spi_xfer(TOKEN_STOP_TRAN);
            wait_ready(500);
        }
    }
    deselect();
    return (count == 0) ? 0 : -1;
}

/* ─── sd_sync ────────────────────────────────────────────────────────── */
int sd_sync(void)
{
    /* Selecting + waiting for ready is enough: the card flushes its
     * internal write buffer before raising DO. */
    if (select_card() < 0) return -1;
    deselect();
    return 0;
}
