/**
 * @file protocol.c
 * @brief RATCX1 TCP protocol handler
 *
 * جایگزین state machine GPRS/SIM900 در فریمور اصلی.
 * به جای AT commands ماژول GSM، از Air780 4G LTE استفاده می‌شود.
 * Air780 همان API عمومی tcp_connect/tcp_send/tcp_recv/tcp_is_connected را
 * ارائه می‌دهد بنابراین این فایل بدون تغییر منطقی کار می‌کند.
 *
 * Protocol messages (same as original – server is unchanged):
 *   → 8000 + datetime(21) + sysId(8) + model + version + "READY"   (handshake)
 *   ← 0012 + "YYYY.MM.DD-HH:MM:SS.0"                               (time sync)
 *   → 8012 + datetime(21) + sysId(8)                                (ACK)
 *   ← 0197 + "YYMMDDHHmm"                                           (data request)
 *   → 8821 + datetime(21) + intervalData(262) + CRLF                (data response)
 */

#include "protocol.h"
#include "air780_tcp.h"
#include "variables.h"
#include "interval.h"
#include "storage.h"
#include "config.h"
#include "console.h"

#include "stm32f1xx_hal.h"
#include <string.h>
#include <stdio.h>

/* ─── State machine ──────────────────────────────────────────────────── */
typedef enum {
    PROTO_DISCONNECTED = 0,
    PROTO_CONNECTING,
    PROTO_HANDSHAKE_SENT,
    PROTO_CONNECTED,           /* handshake ACKed by server, data can flow */
} ProtoState_t;

static ProtoState_t state = PROTO_DISCONNECTED;

static uint8_t rx_buf[TCP_RX_BUF];

/* ─── Helpers ────────────────────────────────────────────────────────── */

/** Format datetimesec string "YYYY.MM.DD-HH:MM:SS.t" from current_time */
static void update_datetimesec(void)
{
    /* year (20xx) */
    datetimesec[0] = '2';
    datetimesec[1] = '0';
    datetimesec[2] = '0' + current_time.year / 10;
    datetimesec[3] = '0' + current_time.year % 10;
    datetimesec[4] = '.';
    datetimesec[5] = '0' + current_time.month / 10;
    datetimesec[6] = '0' + current_time.month % 10;
    datetimesec[7] = '.';
    datetimesec[8] = '0' + current_time.day / 10;
    datetimesec[9] = '0' + current_time.day % 10;
    datetimesec[10] = '-';
    datetimesec[11] = '0' + current_time.hour / 10;
    datetimesec[12] = '0' + current_time.hour % 10;
    datetimesec[13] = ':';
    datetimesec[14] = '0' + current_time.minute / 10;
    datetimesec[15] = '0' + current_time.minute % 10;
    datetimesec[16] = ':';
    datetimesec[17] = '0' + current_time.second / 10;
    datetimesec[18] = '0' + current_time.second % 10;
    datetimesec[19] = '.';
    datetimesec[20] = '0' + (uint8_t)(milisec / 100);
    datetimesec[21] = '\0';
}

/** Send 8000 handshake: "8000" + datetime(21) + sysId(8) + model + version + "READY" */
static void send_handshake(void)
{
    char buf[128];
    int  len;
    update_datetimesec();
    len = snprintf(buf, sizeof(buf), "8000%s%s%s%sREADY",
                   datetimesec, g_cfg.system_id, SYSTEM_MODEL, FW_VERSION);
    tcp_send((uint8_t *)buf, (uint16_t)len);
}

/** Send 8012 time-sync ACK: "8012" + datetime(21) + sysId(8) */
static void send_time_ack(void)
{
    char buf[64];
    int  len;
    update_datetimesec();
    len = snprintf(buf, sizeof(buf), "8012%s%s", datetimesec, g_cfg.system_id);
    tcp_send((uint8_t *)buf, (uint16_t)len);
}

/** Send 8821 interval response: "8821" + datetime(21) + intervalData(264) */
static void send_interval_response(void)
{
    char header[32];
    int  hlen;
    update_datetimesec();
    hlen = snprintf(header, sizeof(header), "8821%s", datetimesec);
    tcp_send((uint8_t *)header, (uint16_t)hlen);
    /* interval_data already has CRLF at [262-263] */
    tcp_send((uint8_t *)interval_data, 264);
}

/**
 * Parse a "0197YYMMDDHHmm" request and respond with 8821.
 * If the requested timestamp matches the last stored interval, send it.
 * Otherwise try to load from W25Q80 flash; if not found, send zeroed data.
 */
static void handle_data_request(const char *msg, uint16_t len)
{
    if (len < 14) return;   /* "0197" + 10-char timestamp */

    /* Points to YYMMDDHHmm (10 chars) */
    const char *req_ts = msg + 4;
    char ts10[11];
    memcpy(ts10, req_ts, 10);
    ts10[10] = '\0';

    /* 1. Check if interval_data in RAM already has the right timestamp */
    int match = (interval_data[8]  == req_ts[0] &&
                 interval_data[9]  == req_ts[1] &&
                 interval_data[10] == req_ts[2] &&
                 interval_data[11] == req_ts[3] &&
                 interval_data[12] == req_ts[4] &&
                 interval_data[13] == req_ts[5] &&
                 interval_data[14] == req_ts[6] &&
                 interval_data[15] == req_ts[7] &&
                 interval_data[16] == req_ts[8] &&
                 interval_data[17] == req_ts[9]);

    if (!match) {
        /* 2. Try to load from any available storage backend (SD then W25Q80) */
        if (!storage_load_interval(ts10, (uint8_t *)interval_data)) {
            /* 3. Not found in flash – send zeroed interval with requested timestamp */
            reset_interval_data();
            interval_data[8]  = req_ts[0];
            interval_data[9]  = req_ts[1];
            interval_data[10] = req_ts[2];
            interval_data[11] = req_ts[3];
            interval_data[12] = req_ts[4];
            interval_data[13] = req_ts[5];
            interval_data[14] = req_ts[6];
            interval_data[15] = req_ts[7];
            interval_data[16] = req_ts[8];
            interval_data[17] = req_ts[9];
        }
    }

    send_interval_response();
}

/**
 * Parse a "0012YYYY.MM.DD-HH:MM:SS.t" time sync command and set current_time.
 * Format: "0012" + "YYYY.MM.DD-HH:MM:SS.t"  (total 25 chars)
 */
static void handle_time_sync(const char *msg, uint16_t len)
{
    if (len < 25) return;

    const char *dt = msg + 4;  /* skip "0012" */
    /* Format: "YYYY.MM.DD-HH:MM:SS.t" */
    current_time.year   = (uint8_t)((dt[2] - '0') * 10 + (dt[3] - '0'));
    current_time.month  = (uint8_t)((dt[5] - '0') * 10 + (dt[6] - '0'));
    current_time.day    = (uint8_t)((dt[8] - '0') * 10 + (dt[9] - '0'));
    current_time.hour   = (uint8_t)((dt[11]- '0') * 10 + (dt[12]- '0'));
    current_time.minute = (uint8_t)((dt[14]- '0') * 10 + (dt[15]- '0'));
    current_time.second = (uint8_t)((dt[17]- '0') * 10 + (dt[18]- '0'));

    /* Validate – guard against garbage data */
    if (current_time.month  > 12) current_time.month  = 1;
    if (current_time.day    > 31) current_time.day    = 1;
    if (current_time.hour   > 23) current_time.hour   = 0;
    if (current_time.minute > 59) current_time.minute = 0;
    if (current_time.second > 59) current_time.second = 0;

    send_time_ack();
}

/* ─── Public API ─────────────────────────────────────────────────────── */

void protocol_init(void)
{
    state = PROTO_DISCONNECTED;
    memset(rx_buf, 0, sizeof(rx_buf));
}

void protocol_reconnect(void)
{
    tcp_disconnect();
    state = PROTO_DISCONNECTED;
}

uint8_t protocol_is_connected(void)
{
    return (state == PROTO_CONNECTED) && tcp_is_connected();
}

/**
 * Main protocol state machine – call from main loop as fast as possible.
 *
 * State transitions:
 *  DISCONNECTED → CONNECTING    : on tcp_connect() attempt
 *  CONNECTING   → HANDSHAKE_SENT: on successful TCP connection
 *  HANDSHAKE_SENT→ CONNECTED    : server replies with 0012 (time sync) which
 *                                  implies handshake was received OK
 *  CONNECTED    → DISCONNECTED  : on TCP disconnect detected
 *
 *  In CONNECTED state: receive 0197 requests and 0012 syncs, respond as needed.
 */
void protocol_task(void)
{
    int bytes;

    /* ── Check TCP health ─────────────────────────────────────────────── */
    if (state != PROTO_DISCONNECTED && !tcp_is_connected()) {
        state = PROTO_DISCONNECTED;
        return;
    }

    switch (state)
    {
    /* ─────────────────────────────────────────── DISCONNECTED ───────── */
    case PROTO_DISCONNECTED:
        if (tcp_connect(g_cfg.server_ip, g_cfg.server_port) == 0) {
            state = PROTO_CONNECTING;
        } else {
            HAL_Delay(5000);   /* wait 5 s before retry */
        }
        break;

    /* ─────────────────────────────────────────── CONNECTING ─────────── */
    case PROTO_CONNECTING:
        if (tcp_is_connected()) {
            send_handshake();
            state = PROTO_HANDSHAKE_SENT;
        }
        break;

    /* ─────────────────────────────────────────── HANDSHAKE_SENT ─────── */
    case PROTO_HANDSHAKE_SENT:
        bytes = tcp_recv(rx_buf, sizeof(rx_buf) - 1);
        if (bytes < 0) {
            state = PROTO_DISCONNECTED;
        } else if (bytes >= 4) {
            rx_buf[bytes] = '\0';
            if (rx_buf[0] == '0' && rx_buf[1] == '0' &&
                rx_buf[2] == '1' && rx_buf[3] == '2') {
                /* Server sent time sync – process it and move to CONNECTED */
                handle_time_sync((char *)rx_buf, (uint16_t)bytes);
                state = PROTO_CONNECTED;
            }
        }
        break;

    /* ─────────────────────────────────────────── CONNECTED ──────────── */
    case PROTO_CONNECTED:
        bytes = tcp_recv(rx_buf, sizeof(rx_buf) - 1);
        if (bytes < 0) {
            state = PROTO_DISCONNECTED;
            return;
        }
        if (bytes >= 4) {
            rx_buf[bytes] = '\0';
            uint8_t *p = rx_buf;
            /* A single TCP packet may contain multiple messages */
            while (bytes >= 4) {
                if (p[0]=='0' && p[1]=='0' && p[2]=='1' && p[3]=='2') {
                    /* 0012 time sync (25 bytes) */
                    handle_time_sync((char *)p, (uint16_t)bytes);
                    if (bytes >= 25) { p += 25; bytes -= 25; }
                    else break;
                } else if (p[0]=='0' && p[1]=='1' && p[2]=='9' && p[3]=='7') {
                    /* 0197 data request (14 bytes) */
                    handle_data_request((char *)p, (uint16_t)bytes);
                    if (bytes >= 14) { p += 14; bytes -= 14; }
                    else break;
                } else {
                    break;  /* unknown message – discard */
                }
            }
        }
        break;
    }
}
