/**
 * @file air780_tcp.c
 * @brief Air780 4G LTE – USART3 driver + AT-command TCP state machine
 *
 * ─── معماری ───────────────────────────────────────────────────────────
 *  - USART3 ISR (PB10/PB11) بایت‌ها را به ring buffer می‌ریزد.
 *  - air780_task() که از حلقه اصلی فراخوانده می‌شود، خطوط کامل را از
 *    ring buffer می‌خواند و URCها را تجزیه می‌کند.
 *  - tcp_send() و tcp_connect() به‌صورت blocking (با timeout) کار می‌کنند.
 *  - داده‌های دریافتی (+IPD) در یک بافر جداگانه کپی می‌شوند که
 *    tcp_recv() آن را در اختیار protocol.c می‌گذارد.
 *
 * ─── AT command flow ──────────────────────────────────────────────────
 *  Power on:
 *    PWRKEY HIGH 600ms → LOW → wait for "RDY" URC
 *    AT+CIMI           → check IMSI (SIM present)
 *    AT+CEREG?         → wait +CEREG: 0,1 or 0,5
 *    AT+CGDCONT=1,"IP","APN"
 *    AT+NETOPEN        → +NETOPEN: 0
 *
 *  Connect:
 *    AT+CIPOPEN=0,"TCP","192.168.1.100",2022
 *    → +CIPOPEN: 0,0  (socket 0, error code 0 = OK)
 *
 *  Send (len bytes):
 *    AT+CIPSEND=0,len
 *    → '>'  (prompt)
 *    [send len bytes]
 *    → +CIPSEND: 0,len
 *
 *  Receive (URC):
 *    +IPD: 0,len\r\n
 *    [len bytes of data]
 *
 *  Disconnect URC:
 *    +CIPEVENT: 0,CLOSED
 */

#include "air780_tcp.h"
#include "config.h"
#include "stm32f1xx_hal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ─── Extern HAL handles (defined in main.c) ─────────────────────────── */
extern UART_HandleTypeDef huart3;

/* ─── GPIO pin defines ───────────────────────────────────────────────── */
/* PB12/PB13 آزاد شده‌اند برای SD card روی SPI2.
 * Air780 کنترل پاور حالا از پین‌های PA8/PA11 استفاده می‌کند. */
#define PWRKEY_PORT  GPIOA
#define PWRKEY_PIN   GPIO_PIN_8
#define STATUS_PORT  GPIOA
#define STATUS_PIN   GPIO_PIN_11

/* ─── Ring buffer for USART3 RX ─────────────────────────────────────── */
#define RING_SZ     512
static volatile uint8_t  ring_buf[RING_SZ];
static volatile uint16_t ring_head = 0;   /* written by ISR */
static volatile uint16_t ring_tail = 0;   /* read  by task  */

/* ─── Line buffer (assembled from ring buffer by task) ───────────────── */
#define LINE_SZ     160
static char line_buf[LINE_SZ];
static uint8_t line_len = 0;

/* ─── Receive data buffer (filled from +IPD URC) ────────────────────── */
#define RX_DATA_SZ  TCP_RX_BUF
static uint8_t  rx_data[RX_DATA_SZ];
static uint16_t rx_data_len  = 0;   /* bytes available for tcp_recv() */
static uint16_t rx_data_head = 0;   /* read pointer for tcp_recv()    */

/* ─── Internal state ─────────────────────────────────────────────────── */
static volatile uint8_t  connected  = 0;  /* 1 = TCP ESTABLISHED        */
static volatile uint8_t  net_open   = 0;  /* 1 = NETOPEN done           */
static volatile uint8_t  got_prompt = 0;  /* 1 = '>' received           */
/* ipd_pending: >0 means we are in the middle of receiving +IPD body */
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

/* ─── USART3 ISR – call from USART3_IRQHandler in main.c ────────────── */
void air780_uart_irq(void)
{
    if (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_RXNE))
    {
        uint8_t b = (uint8_t)(huart3.Instance->DR & 0xFF);
        uint16_t next = (ring_head + 1) % RING_SZ;
        if (next != ring_tail) {          /* don't overwrite if full */
            ring_buf[ring_head] = b;
            ring_head = next;
        }
    }
    /* Clear overrun error if set */
    if (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_ORE)) {
        __HAL_UART_CLEAR_OREFLAG(&huart3);
    }
}

/* ─── Send a raw string over USART3 ────────────────────────────────── */
static void at_write(const char *s)
{
    HAL_UART_Transmit(&huart3, (uint8_t *)s, (uint16_t)strlen(s), 500);
}

static void at_send(const char *cmd)
{
    at_write(cmd);
    at_write("\r\n");
}

/* ─── Process one complete line received from Air780 ────────────────── */
static void process_line(const char *line)
{
    if (strncmp(line, "+CIPOPEN:", 9) == 0) {
        /* +CIPOPEN: 0,0  → socket 0, error 0 = connected */
        int sock = 0, err = 0;
        if (sscanf(line + 9, " %d,%d", &sock, &err) == 2 && sock == 0 && err == 0)
            connected = 1;
        else
            connected = 0;
    }
    else if (strncmp(line, "+CIPEVENT:", 10) == 0) {
        /* +CIPEVENT: 0,CLOSED */
        if (strstr(line, "CLOSED"))
            connected = 0;
    }
    else if (strncmp(line, "+NETOPEN:", 9) == 0) {
        /* +NETOPEN: 0 = network opened successfully */
        int code = 1;
        sscanf(line + 9, " %d", &code);
        if (code == 0) net_open = 1;
    }
    else if (strncmp(line, "+IPD:", 5) == 0) {
        /* +IPD: 0,len  → incoming data follows on next read */
        int sock = 0, len = 0;
        sscanf(line + 5, " %d,%d", &sock, &len);
        if (sock == 0 && len > 0)
            ipd_pending = (uint16_t)len;
    }
    /* '>' is handled by the byte-level scanner in air780_task */
}

/* ─── air780_task ────────────────────────────────────────────────────── */
/**
 * Call from main loop. Drains ring buffer, builds lines, processes URCs.
 * Also handles raw IPD body bytes.
 */
void air780_task(void)
{
    while (ring_count() > 0)
    {
        uint8_t b = ring_get();

        /* ── Detect '>' send-data prompt ───────────────────────────── */
        if (b == '>') {
            got_prompt = 1;
            continue;
        }

        /* ── Accumulate IPD body bytes ──────────────────────────────── */
        if (ipd_pending > 0) {
            if (rx_data_len < RX_DATA_SZ) {
                rx_data[rx_data_len++] = b;
            }
            ipd_pending--;
            continue;
        }

        /* ── Build line buffer ──────────────────────────────────────── */
        if (b == '\n') {
            if (line_len > 0) {
                line_buf[line_len] = '\0';
                process_line(line_buf);
                line_len = 0;
            }
        } else if (b == '\r') {
            /* ignore CR – wait for LF */
        } else {
            if (line_len < LINE_SZ - 1)
                line_buf[line_len++] = (char)b;
        }
    }
}

/* ─── Wait helpers ───────────────────────────────────────────────────── */
/**
 * Wait for a specific substring in the next received line (blocking with timeout).
 * Keeps calling air780_task() to process incoming bytes while waiting.
 */
static int at_wait_for(const char *expect, uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    char matched_line[LINE_SZ];
    uint8_t tmp_len = 0;

    /* Reset line accumulator for this wait */
    uint16_t saved_tail = ring_tail;
    (void)saved_tail;

    while ((HAL_GetTick() - start) < timeout_ms)
    {
        /* Drain ring buffer into a temporary line detector */
        while (ring_count() > 0)
        {
            uint8_t b = ring_get();

            if (b == '>') { got_prompt = 1; }

            if (b == '\n') {
                if (tmp_len > 0) {
                    matched_line[tmp_len] = '\0';
                    /* Feed to URC processor too */
                    process_line(matched_line);
                    /* Check if this is what we're looking for */
                    if (strstr(matched_line, expect)) return 0;
                    tmp_len = 0;
                }
            } else if (b != '\r') {
                if (tmp_len < LINE_SZ - 1)
                    matched_line[tmp_len++] = (char)b;
            }
        }
        HAL_Delay(2);
    }
    return -1;   /* timeout */
}

/* ─── Power on / off ─────────────────────────────────────────────────── */
static void air780_power_on(void)
{
    /* If already on (STATUS=HIGH), nothing to do */
    if (HAL_GPIO_ReadPin(STATUS_PORT, STATUS_PIN) == GPIO_PIN_SET)
        return;

    /* Pulse PWRKEY HIGH for 600ms */
    HAL_GPIO_WritePin(PWRKEY_PORT, PWRKEY_PIN, GPIO_PIN_SET);
    HAL_Delay(600);
    HAL_GPIO_WritePin(PWRKEY_PORT, PWRKEY_PIN, GPIO_PIN_RESET);

    /* Wait up to 15 s for STATUS to go HIGH */
    uint32_t start = HAL_GetTick();
    while (HAL_GPIO_ReadPin(STATUS_PORT, STATUS_PIN) == GPIO_PIN_RESET) {
        if ((HAL_GetTick() - start) > 15000) return;
        HAL_Delay(100);
    }
    HAL_Delay(2000);   /* allow module to finish boot */
}

/* ─── air780_init ────────────────────────────────────────────────────── */
int air780_init(void)
{
    net_open  = 0;
    connected = 0;
    ring_head = ring_tail = 0;
    line_len  = 0;
    rx_data_len = rx_data_head = 0;
    ipd_pending = 0;

    /* 1. Power on */
    air780_power_on();

    /* 2. Basic AT handshake */
    at_send("AT");
    if (at_wait_for("OK", 3000) != 0) return -1;

    /* 3. Disable echo */
    at_send("ATE0");
    at_wait_for("OK", 1000);

    /* 4. Check SIM */
    at_send("AT+CIMI");
    if (at_wait_for("OK", 3000) != 0) return -2;

    /* 5. Wait for LTE registration (+CEREG: 0,1 or 0,5) */
    {
        char resp[64];
        uint32_t start = HAL_GetTick();
        while ((HAL_GetTick() - start) < 60000) {
            at_send("AT+CEREG?");
            /* Peek at response lines – if one contains ",1" or ",5" → registered */
            uint8_t rlen = 0;
            uint32_t t2 = HAL_GetTick();
            while ((HAL_GetTick() - t2) < 2000 && rlen < 63) {
                while (ring_count() > 0) {
                    uint8_t b = ring_get();
                    if (b == '\n') {
                        resp[rlen] = '\0';
                        if (strstr(resp, "+CEREG:") &&
                            (strstr(resp, ",1") || strstr(resp, ",5")))
                            goto registered;
                        rlen = 0;
                    } else if (b != '\r') {
                        resp[rlen++] = (char)b;
                    }
                }
            }
            HAL_Delay(2000);
        }
        return -3;   /* registration timeout */
    }
registered:;

    /* 6. Set APN */
    {
        char cmd[80];
        snprintf(cmd, sizeof(cmd), "AT+CGDCONT=1,\"IP\",\"%s\"", AIR780_APN);
        at_send(cmd);
        at_wait_for("OK", 3000);
    }

    /* 7. Open network */
    net_open = 0;
    at_send("AT+NETOPEN");
    if (at_wait_for("NETOPEN", 30000) != 0) {
        /* Already open? Try CIPOPEN directly */
    }
    /* Short delay for network stack to stabilise */
    HAL_Delay(1000);

    return 0;
}

/* ─── tcp_connect ────────────────────────────────────────────────────── */
int tcp_connect(uint8_t *server_ip, uint16_t server_port)
{
    if (!server_ip) return -1;

    char cmd[80];
    snprintf(cmd, sizeof(cmd),
             "AT+CIPOPEN=0,\"TCP\",\"%d.%d.%d.%d\",%u",
             server_ip[0], server_ip[1], server_ip[2], server_ip[3],
             (unsigned)server_port);

    connected = 0;
    at_send(cmd);

    /* Wait for +CIPOPEN: 0,0 */
    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < 10000) {
        air780_task();
        if (connected) return 0;
        HAL_Delay(20);
    }
    return -1;
}

/* ─── tcp_disconnect ─────────────────────────────────────────────────── */
void tcp_disconnect(void)
{
    at_send("AT+CIPCLOSE=0");
    HAL_Delay(500);
    connected = 0;
}

/* ─── tcp_send ───────────────────────────────────────────────────────── */
int tcp_send(const uint8_t *buf, uint16_t len)
{
    if (!connected || len == 0) return -1;

    /* Step 1: AT+CIPSEND=0,len */
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=0,%u", (unsigned)len);
    got_prompt = 0;
    at_send(cmd);

    /* Step 2: wait for '>' prompt */
    uint32_t start = HAL_GetTick();
    while (!got_prompt && (HAL_GetTick() - start) < 3000) {
        air780_task();
        HAL_Delay(2);
    }
    if (!got_prompt) return -1;

    /* Step 3: send raw data */
    if (HAL_UART_Transmit(&huart3, (uint8_t *)buf, len, 5000) != HAL_OK)
        return -1;

    /* Step 4: wait for +CIPSEND: 0,len (ACK from module) */
    if (at_wait_for("CIPSEND", 5000) != 0) return -1;

    return (int)len;
}

/* ─── tcp_recv ───────────────────────────────────────────────────────── */
int tcp_recv(uint8_t *buf, uint16_t len)
{
    air780_task();   /* drain ring buffer first */

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
    air780_task();
    return connected;
}
