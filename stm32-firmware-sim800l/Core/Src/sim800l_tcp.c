/**
 * @file sim800l_tcp.c
 * @brief SIM800L GPRS – USART3 driver + AT-command TCP state machine
 *
 * ─── جریان AT commands ────────────────────────────────────────────────
 *
 *  Power on:
 *    PWRKEY LOW 1200ms → HIGH → wait STATUS HIGH (or wait for "Call Ready")
 *    AT            → OK
 *    ATE0          → OK  (disable echo)
 *    AT+CPIN?      → +CPIN: READY
 *    AT+CREG?      → +CREG: 0,1  (یا 0,5 برای رومینگ)
 *
 *  GPRS bearer:
 *    AT+CSTT="APN","","" → OK
 *    AT+CIICR            → OK  (activate bearer – ممکن است 5-10 ثانیه طول بکشد)
 *    AT+CIFSR            → x.x.x.x  (دریافت IP تأیید می‌کند GPRS فعال شد)
 *
 *  Connect:
 *    AT+CIPSTART="TCP","ip","port"
 *    → CONNECT OK
 *
 *  Send (len bytes):
 *    AT+CIPSEND=len
 *    → '>'
 *    [send len bytes]
 *    → SEND OK
 *
 *  Receive (URC):
 *    +IPD,len:data   (data همان خط، بعد از دونقطه)
 *
 *  Disconnect URC:
 *    CLOSED
 */

#include "sim800l_tcp.h"
#include "config.h"
#include "stm32f1xx_hal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

extern UART_HandleTypeDef huart3;

/* ─── GPIO ───────────────────────────────────────────────────────────── */
/* پین‌های یکسان با Air780 – فقط منطق PWRKEY فرق دارد */
#define PWRKEY_PORT  GPIOA
#define PWRKEY_PIN   GPIO_PIN_8
#define STATUS_PORT  GPIOA
#define STATUS_PIN   GPIO_PIN_11

/* ─── Ring buffer ────────────────────────────────────────────────────── */
#define RING_SZ     512
static volatile uint8_t  ring_buf[RING_SZ];
static volatile uint16_t ring_head = 0;
static volatile uint16_t ring_tail = 0;

/* ─── Line buffer ────────────────────────────────────────────────────── */
#define LINE_SZ     160
static char    line_buf[LINE_SZ];
static uint8_t line_len = 0;

/* ─── RX data buffer ─────────────────────────────────────────────────── */
#define RX_DATA_SZ  TCP_RX_BUF
static uint8_t  rx_data[RX_DATA_SZ];
static uint16_t rx_data_len  = 0;
static uint16_t rx_data_head = 0;

/* ─── State ──────────────────────────────────────────────────────────── */
static volatile uint8_t  connected   = 0;
static volatile uint8_t  got_prompt  = 0;
static volatile uint8_t  send_ok     = 0;
static volatile uint8_t  connect_ok  = 0;

/* SIM800L +IPD,len:data – داده inline است پس فقط len را نگه می‌داریم */
static volatile uint16_t ipd_pending = 0;

/* ─── Ring buffer helpers ────────────────────────────────────────────── */
static inline uint16_t ring_count(void)
{
    return (uint16_t)((ring_head - ring_tail + RING_SZ) % RING_SZ);
}

static inline uint8_t ring_get(void)
{
    uint8_t b = ring_buf[ring_tail];
    ring_tail = (ring_tail + 1) % RING_SZ;
    return b;
}

/* ─── USART3 ISR ─────────────────────────────────────────────────────── */
void sim800l_uart_irq(void)
{
    if (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_RXNE)) {
        uint8_t b = (uint8_t)(huart3.Instance->DR & 0xFF);
        uint16_t next = (ring_head + 1) % RING_SZ;
        if (next != ring_tail) {
            ring_buf[ring_head] = b;
            ring_head = next;
        }
    }
    if (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_ORE))
        __HAL_UART_CLEAR_OREFLAG(&huart3);
}

/* ─── AT helpers ─────────────────────────────────────────────────────── */
static void at_write(const char *s)
{
    HAL_UART_Transmit(&huart3, (uint8_t *)s, (uint16_t)strlen(s), 500);
}

static void at_send(const char *cmd)
{
    at_write(cmd);
    at_write("\r\n");
}

/* ─── پردازش یک خط کامل دریافتی از SIM800L ───────────────────────────── */
static void process_line(const char *line)
{
    /* CONNECT OK – اتصال TCP برقرار شد */
    if (strstr(line, "CONNECT OK"))        { connected = 1; connect_ok = 1; return; }

    /* ALREADY CONNECT – قبلاً وصل بوده */
    if (strstr(line, "ALREADY CONNECT"))   { connected = 1; connect_ok = 1; return; }

    /* SEND OK – ارسال تایید شد */
    if (strstr(line, "SEND OK"))           { send_ok = 1; return; }

    /* CLOSED / CLOSE OK – اتصال قطع شد */
    if (strstr(line, "CLOSED") || strstr(line, "CLOSE OK"))
    {
        connected = 0;
        return;
    }

    /* +IPD,len:data – داده دریافتی (data اینجا inline است) */
    if (strncmp(line, "+IPD,", 5) == 0) {
        int len = 0;
        const char *colon = strchr(line + 5, ':');
        if (colon) {
            len = atoi(line + 5);
            /* کپی data بعد از دونقطه به rx_data */
            const char *data = colon + 1;
            uint16_t data_len = (uint16_t)strlen(data);
            if (data_len > (uint16_t)len) data_len = (uint16_t)len;
            uint16_t space = RX_DATA_SZ - rx_data_len;
            if (data_len > space) data_len = space;
            memcpy(rx_data + rx_data_len, data, data_len);
            rx_data_len += data_len;
            /* اگر داده از یک line بیشتر بود ipd_pending را تنظیم کن */
            if ((uint16_t)len > data_len)
                ipd_pending = (uint16_t)(len - data_len);
        }
        return;
    }
}

/* ─── sim800l_task ───────────────────────────────────────────────────── */
void sim800l_task(void)
{
    while (ring_count() > 0)
    {
        uint8_t b = ring_get();

        /* پرامپت ارسال */
        if (b == '>') { got_prompt = 1; continue; }

        /* بایت‌های باقی‌مانده یک +IPD که بعد از \n آمده */
        if (ipd_pending > 0) {
            if (rx_data_len < RX_DATA_SZ)
                rx_data[rx_data_len++] = b;
            ipd_pending--;
            continue;
        }

        /* ساخت line buffer */
        if (b == '\n') {
            if (line_len > 0) {
                line_buf[line_len] = '\0';
                process_line(line_buf);
                line_len = 0;
            }
        } else if (b == '\r') {
            /* نادیده گرفتن CR */
        } else {
            if (line_len < LINE_SZ - 1)
                line_buf[line_len++] = (char)b;
        }
    }
}

/* ─── at_wait_for ────────────────────────────────────────────────────── */
static int at_wait_for(const char *expect, uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    char tmp[LINE_SZ];
    uint8_t tlen = 0;

    while ((HAL_GetTick() - start) < timeout_ms)
    {
        while (ring_count() > 0)
        {
            uint8_t b = ring_get();
            if (b == '>') { got_prompt = 1; }
            if (b == '\n') {
                if (tlen > 0) {
                    tmp[tlen] = '\0';
                    process_line(tmp);
                    if (strstr(tmp, expect)) return 0;
                    tlen = 0;
                }
            } else if (b != '\r') {
                if (tlen < LINE_SZ - 1)
                    tmp[tlen++] = (char)b;
            }
        }
        HAL_Delay(5);
    }
    return -1;
}

/* ─── Power on ───────────────────────────────────────────────────────── */
/*
 * نکته سخت‌افزاری مهم:
 * SIM800L جریان peak تا 2A دارد. اگر از یک منبع مشترک با STM32 استفاده
 * شود، ولتاژ افت می‌کند و STM32 brownout reset می‌شود. حتماً یک
 * خازن 470µF تا 1000µF روی پایه VCC ماژول SIM800L نصب کنید.
 *
 * جریان init:
 *  1. صبر 500ms تا ولتاژ پس از reset تثبیت شود
 *  2. چک STATUS → اگر HIGH، ماژول قبلاً روشن است
 *  3. پالس LOW فقط یک‌بار؛ اگر بعد از 20 ثانیه STATUS هنوز LOW است،
 *     ادامه می‌دهیم و در AT handshake timeout می‌کنیم
 */
static void sim800l_power_on(void)
{
    /* صبر تا منبع تغذیه پس از reset تثبیت شود */
    HAL_Delay(500);

    /* اگر STATUS بالاست، ماژول قبلاً روشن است */
    if (HAL_GPIO_ReadPin(STATUS_PORT, STATUS_PIN) == GPIO_PIN_SET)
        return;

    /* پالس LOW روی PWRKEY حداقل 1 ثانیه – فقط یک‌بار تلاش می‌کنیم */
    HAL_GPIO_WritePin(PWRKEY_PORT, PWRKEY_PIN, GPIO_PIN_RESET);
    HAL_Delay(1200);
    HAL_GPIO_WritePin(PWRKEY_PORT, PWRKEY_PIN, GPIO_PIN_SET);

    /* انتظار برای STATUS – حداکثر 20 ثانیه
     * اگر منبع تغذیه ضعیف باشد و STATUS بالا نیاید، ادامه می‌دهیم
     * تا AT handshake تصمیم بگیرد */
    uint32_t start = HAL_GetTick();
    while (HAL_GPIO_ReadPin(STATUS_PORT, STATUS_PIN) == GPIO_PIN_RESET) {
        if ((HAL_GetTick() - start) > 20000) break;
        HAL_Delay(200);
    }

    /* زمان boot کامل ماژول */
    HAL_Delay(2000);
}

/* ─── sim800l_init ───────────────────────────────────────────────────── */
int sim800l_init(void)
{
    connected   = 0;
    got_prompt  = 0;
    send_ok     = 0;
    connect_ok  = 0;
    ring_head   = ring_tail = 0;
    line_len    = 0;
    rx_data_len = rx_data_head = 0;
    ipd_pending = 0;

    /* 1. روشن کردن ماژول */
    sim800l_power_on();

    /* 2. AT handshake – چندین بار تلاش می‌کنیم چون auto-baud نیاز دارد */
    {
        uint8_t ok = 0;
        for (int i = 0; i < 5; i++) {
            at_send("AT");
            if (at_wait_for("OK", 1000) == 0) { ok = 1; break; }
        }
        if (!ok) return -1;
    }

    /* 3. غیرفعال کردن echo */
    at_send("ATE0");
    at_wait_for("OK", 1000);

    /* 4. بررسی SIM */
    at_send("AT+CPIN?");
    if (at_wait_for("READY", 3000) != 0) return -2;

    /* 5. انتظار برای ثبت شبکه GSM (+CREG: 0,1 یا 0,5) */
    {
        uint8_t net_ok = 0;
        uint32_t start = HAL_GetTick();
        while ((HAL_GetTick() - start) < 60000) {
            char resp[80];
            uint8_t rlen = 0;
            at_send("AT+CREG?");
            uint32_t t2 = HAL_GetTick();
            while ((HAL_GetTick() - t2) < 2000) {
                while (ring_count() > 0 && rlen < (uint8_t)(sizeof(resp) - 1)) {
                    uint8_t b = ring_get();
                    if (b == '\n') {
                        resp[rlen] = '\0';
                        if (strstr(resp, "+CREG:") &&
                            (strstr(resp, ",1") || strstr(resp, ",5"))) {
                            net_ok = 1;
                        }
                        rlen = 0;
                    } else if (b != '\r') {
                        resp[rlen++] = b;
                    }
                }
                if (net_ok) break;
                HAL_Delay(10);
            }
            if (net_ok) break;
            HAL_Delay(2000);
        }
        if (!net_ok) return -3;
    }

    /* 6. تنظیم APN و فعال‌سازی bearer */
    {
        char cmd[80];
        snprintf(cmd, sizeof(cmd), "AT+CSTT=\"%s\",\"\",\"\"", SIM800L_APN);
        at_send(cmd);
        if (at_wait_for("OK", 5000) != 0) return -4;
    }

    /* 7. فعال‌سازی GPRS (AT+CIICR ممکن است تا 10 ثانیه طول بکشد) */
    at_send("AT+CIICR");
    if (at_wait_for("OK", 15000) != 0) return -5;

    /* 8. گرفتن IP (تأیید می‌کند GPRS فعال است) */
    at_send("AT+CIFSR");
    if (at_wait_for(".", 5000) != 0) return -6;   /* IP شامل نقطه است */

    return 0;
}

/* ─── tcp_connect ────────────────────────────────────────────────────── */
int tcp_connect(uint8_t *server_ip, uint16_t server_port)
{
    if (!server_ip) return -1;

    char cmd[80];
    snprintf(cmd, sizeof(cmd),
             "AT+CIPSTART=\"TCP\",\"%d.%d.%d.%d\",\"%u\"",
             server_ip[0], server_ip[1], server_ip[2], server_ip[3],
             (unsigned)server_port);

    connected  = 0;
    connect_ok = 0;
    at_send(cmd);

    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < 15000) {
        sim800l_task();
        if (connect_ok) return connected ? 0 : -1;
        /* خطای فوری */
        HAL_Delay(20);
    }
    return -1;
}

/* ─── tcp_disconnect ─────────────────────────────────────────────────── */
void tcp_disconnect(void)
{
    at_send("AT+CIPCLOSE");
    HAL_Delay(500);
    connected = 0;
}

/* ─── tcp_send ───────────────────────────────────────────────────────── */
int tcp_send(const uint8_t *buf, uint16_t len)
{
    if (!connected || len == 0) return -1;

    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u", (unsigned)len);
    got_prompt = 0;
    send_ok    = 0;
    at_send(cmd);

    /* انتظار برای '>' */
    uint32_t start = HAL_GetTick();
    while (!got_prompt && (HAL_GetTick() - start) < 3000) {
        sim800l_task();
        HAL_Delay(2);
    }
    if (!got_prompt) return -1;

    /* ارسال داده */
    if (HAL_UART_Transmit(&huart3, (uint8_t *)buf, len, 5000) != HAL_OK)
        return -1;

    /* انتظار برای SEND OK */
    start = HAL_GetTick();
    while (!send_ok && (HAL_GetTick() - start) < 5000) {
        sim800l_task();
        HAL_Delay(5);
    }
    if (!send_ok) return -1;

    return (int)len;
}

/* ─── tcp_recv ───────────────────────────────────────────────────────── */
int tcp_recv(uint8_t *buf, uint16_t len)
{
    sim800l_task();

    if (!connected) return -1;
    if (rx_data_head >= rx_data_len) {
        rx_data_len = rx_data_head = 0;
        return 0;
    }

    uint16_t avail = rx_data_len - rx_data_head;
    if (avail > len) avail = len;
    memcpy(buf, rx_data + rx_data_head, avail);
    rx_data_head += avail;
    if (rx_data_head >= rx_data_len)
        rx_data_len = rx_data_head = 0;

    return (int)avail;
}

/* ─── tcp_is_connected ───────────────────────────────────────────────── */
uint8_t tcp_is_connected(void)
{
    sim800l_task();
    return connected;
}
