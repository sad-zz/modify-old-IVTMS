/**
 * @file console.c
 * @brief Serial console + flash-backed runtime configuration (USART1)
 *
 * تنظیمات در صفحه‌ی آخر flash داخلی STM32F103C8 (1KB در آدرس 0x0800FC00)
 * ذخیره می‌شوند. این صفحه خارج از محدوده‌ی کد است (کد ~40KB از 64KB).
 */

#include "console.h"
#include "config.h"
#include "variables.h"
#include "protocol.h"
#include "air780_tcp.h"
#include "stm32f1xx_hal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ─── Globals ────────────────────────────────────────────────────────── */
app_cfg_t g_cfg;

extern UART_HandleTypeDef huart1;          /* debug/console UART (main.c)   */

#define CFG_MAGIC       0x52415443UL       /* "RATC"                        */
#define CFG_FLASH_ADDR  0x0800FC00UL       /* page 63 of a 64KB F103        */
#define CFG_FLASH_PAGE  0x0800FC00UL

/* ─── tiny CRC32 (bitwise, no table) ─────────────────────────────────── */
static uint32_t crc32(const void *buf, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)buf;
    uint32_t crc = 0xFFFFFFFFUL;
    while (len--) {
        crc ^= *p++;
        for (int i = 0; i < 8; i++)
            crc = (crc >> 1) ^ (0xEDB88320UL & (~((crc & 1) - 1)));
    }
    return crc ^ 0xFFFFFFFFUL;
}

static uint32_t cfg_crc(const app_cfg_t *c)
{
    return crc32(c, (uint32_t)((const uint8_t *)&c->crc - (const uint8_t *)c));
}

/* ─── UART output helpers ────────────────────────────────────────────── */
static void cputc(char ch)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 50);
}
static void cputs(const char *s)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)s, (uint16_t)strlen(s), 200);
}
static void cnl(void) { cputs("\r\n"); }

/* ─── Configuration store ────────────────────────────────────────────── */
void config_defaults(void)
{
    static const uint8_t def_ip[4] = SERVER_IP;
    memset(&g_cfg, 0, sizeof(g_cfg));
    g_cfg.magic = CFG_MAGIC;
    strncpy(g_cfg.system_id, SYSTEM_ID, sizeof(g_cfg.system_id) - 1);
    memcpy(g_cfg.server_ip, def_ip, 4);
    g_cfg.server_port = SERVER_PORT;
    strncpy(g_cfg.apn, AIR780_APN, sizeof(g_cfg.apn) - 1);
    g_cfg.crc = cfg_crc(&g_cfg);
}

void config_load(void)
{
    const app_cfg_t *f = (const app_cfg_t *)CFG_FLASH_ADDR;
    if (f->magic == CFG_MAGIC && cfg_crc(f) == f->crc) {
        memcpy(&g_cfg, f, sizeof(g_cfg));
        /* make sure strings are terminated */
        g_cfg.system_id[sizeof(g_cfg.system_id) - 1] = '\0';
        g_cfg.apn[sizeof(g_cfg.apn) - 1] = '\0';
    } else {
        config_defaults();
    }
}

int config_save(void)
{
    g_cfg.magic = CFG_MAGIC;
    g_cfg.crc   = cfg_crc(&g_cfg);

    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef ei = {0};
    ei.TypeErase   = FLASH_TYPEERASE_PAGES;
    ei.PageAddress = CFG_FLASH_PAGE;
    ei.NbPages     = 1;
    uint32_t pageErr = 0;
    if (HAL_FLASHEx_Erase(&ei, &pageErr) != HAL_OK) {
        HAL_FLASH_Lock();
        return -1;
    }

    const uint16_t *src = (const uint16_t *)&g_cfg;
    const uint32_t  n   = (sizeof(g_cfg) + 1) / 2;
    for (uint32_t i = 0; i < n; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,
                              CFG_FLASH_ADDR + i * 2u, src[i]) != HAL_OK) {
            HAL_FLASH_Lock();
            return -2;
        }
    }
    HAL_FLASH_Lock();

    /* verify */
    if (memcmp((const void *)CFG_FLASH_ADDR, &g_cfg, sizeof(g_cfg)) != 0)
        return -3;
    return 0;
}

/* ─── Command implementations ────────────────────────────────────────── */
static void print_ip(const uint8_t ip[4])
{
    char b[20];
    snprintf(b, sizeof(b), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    cputs(b);
}

static void cmd_show(void)
{
    char b[80];
    cputs("Config:\r\n");
    snprintf(b, sizeof(b), "  id   = %s\r\n", g_cfg.system_id);     cputs(b);
    cputs("  ip   = "); print_ip(g_cfg.server_ip); cnl();
    snprintf(b, sizeof(b), "  port = %u\r\n", g_cfg.server_port);   cputs(b);
    snprintf(b, sizeof(b), "  apn  = %s\r\n", g_cfg.apn);           cputs(b);
    snprintf(b, sizeof(b), "  (firmware defaults: id=%s port=%u apn=%s)\r\n",
             SYSTEM_ID, SERVER_PORT, AIR780_APN);                   cputs(b);
}

static void cmd_status(void)
{
    char b[80];
    cputs("Status:\r\n");
    snprintf(b, sizeof(b), "  fw       = %s %s\r\n", SYSTEM_MODEL, FW_VERSION); cputs(b);
    snprintf(b, sizeof(b), "  uptime   = %lu s\r\n", (unsigned long)(HAL_GetTick()/1000)); cputs(b);
    snprintf(b, sizeof(b), "  modem    = %s\r\n", tcp_is_connected() ? "TCP connected" :
             (is_error(VMN_ERR) ? "init failed" : "not connected")); cputs(b);
    cputs("  server   = "); print_ip(g_cfg.server_ip);
    snprintf(b, sizeof(b), ":%u\r\n", g_cfg.server_port); cputs(b);
    snprintf(b, sizeof(b), "  onloop   = %d %d %d %d\r\n",
             onloop[0], onloop[1], onloop[2], onloop[3]); cputs(b);
    snprintf(b, sizeof(b), "  loop_en  = %d %d %d %d\r\n",
             loop[0], loop[1], loop[2], loop[3]); cputs(b);
    snprintf(b, sizeof(b), "  vbat_adc = %ld   solar_adc = %ld\r\n",
             (long)vbat, (long)solar); cputs(b);
    snprintf(b, sizeof(b), "  errors   = 0x%04X\r\n", (unsigned)error_byte); cputs(b);
    snprintf(b, sizeof(b), "  time     = %s\r\n", datetimesec[0] ? datetimesec : "(not set)"); cputs(b);
    snprintf(b, sizeof(b), "  totals L1: A=%lu B=%lu C=%lu D=%lu E=%lu X=%lu\r\n",
             (unsigned long)totalv1a,(unsigned long)totalv1b,(unsigned long)totalv1c,
             (unsigned long)totalv1d,(unsigned long)totalv1e,(unsigned long)totalv1x); cputs(b);
    snprintf(b, sizeof(b), "  totals L2: A=%lu B=%lu C=%lu D=%lu E=%lu X=%lu\r\n",
             (unsigned long)totalv2a,(unsigned long)totalv2b,(unsigned long)totalv2c,
             (unsigned long)totalv2d,(unsigned long)totalv2e,(unsigned long)totalv2x); cputs(b);
}

static void cmd_loops(void)
{
    char b[80];
    cputs("Loop  freq(us)   cal(us)   dev(x1e4)  on\r\n");
    for (int i = 0; i < 4; i++) {
        snprintf(b, sizeof(b), " %d    %8lu  %8lu  %8d   %d\r\n",
                 i, (unsigned long)freq_mean[i], (unsigned long)caldata[i],
                 dev[i], onloop[i]);
        cputs(b);
    }
}

static void cmd_help(void)
{
    cputs(
      "Commands:\r\n"
      "  help              this list\r\n"
      "  show              show stored config\r\n"
      "  status            live status (modem, loops, battery, errors)\r\n"
      "  loops             live loop frequency / calibration values\r\n"
      "  modem             run modem diagnostics (AT, SIM, signal, registration)\r\n"
      "  at <AT-command>   send a raw AT command to the modem, show reply\r\n"
      "  set ip A.B.C.D    set server IP\r\n"
      "  set port N        set TCP port\r\n"
      "  set apn STR       set SIM APN\r\n"
      "  set id STR        set device ID (max 15 chars)\r\n"
      "  save              write config to flash (persists across reboot)\r\n"
      "  default           reset config to firmware defaults (RAM; use save)\r\n"
      "  reboot            software reset\r\n");
}

static void modem_query(const char *cmd)
{
    static char resp[256];
    cputs(cmd); cputs(" ->\r\n");
    int r = air780_at_raw(cmd, resp, sizeof(resp), 5000);
    if (resp[0]) cputs(resp);
    if (r < 0)   cputs("  <no response - timeout>\r\n");
    cnl();
}

static void cmd_modem(void)
{
    cputs("--- modem diagnostics ---\r\n");
    modem_query("AT");
    modem_query("ATI");
    modem_query("AT+CPIN?");
    modem_query("AT+CIMI");
    modem_query("AT+CSQ");
    modem_query("AT+CEREG?");
    modem_query("AT+COPS?");
    modem_query("AT+CGDCONT?");
    cputs("-------------------------\r\n");
    cputs("note: if 'AT' shows no response -> power(3.8-4.2V/2A), EN pin, TXD<->RXD crossover.\r\n");
    cputs("      if 'AT' OK but CSQ low / CEREG not ,1 or ,5 -> antenna, signal, SIM, APN.\r\n");
}

/* parse "A.B.C.D" → ip[4]; returns 0 on success */
static int parse_ip(const char *s, uint8_t ip[4])
{
    unsigned a, b, c, d;
    if (sscanf(s, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) return -1;
    if (a > 255 || b > 255 || c > 255 || d > 255) return -1;
    ip[0]=(uint8_t)a; ip[1]=(uint8_t)b; ip[2]=(uint8_t)c; ip[3]=(uint8_t)d;
    return 0;
}

static void to_lower(char *s) { for (; *s; s++) if (*s>='A'&&*s<='Z') *s += 32; }

static void exec_line(char *line)
{
    /* skip leading whitespace */
    while (*line == ' ' || *line == '\t') line++;

    /* "at <cmd>" → raw AT command passthrough to the modem */
    if ((line[0] == 'a' || line[0] == 'A') && (line[1] == 't' || line[1] == 'T') &&
        (line[2] == ' ' || line[2] == '\t')) {
        const char *cmd = line + 3;
        while (*cmd == ' ' || *cmd == '\t') cmd++;
        if (!*cmd) { cputs("usage: at <AT-command>   e.g.  at AT+CSQ\r\n"); return; }
        static char resp[256];
        int r = air780_at_raw(cmd, resp, sizeof(resp), 5000);
        if (resp[0]) cputs(resp);
        cputs(r == 0 ? "[OK]\r\n" : r == 1 ? "[ERROR]\r\n" : "[timeout - no response from modem]\r\n");
        return;
    }

    /* split into argv (max 4 tokens) */
    char *argv[4]; int argc = 0;
    char *p = line;
    while (*p && argc < 4) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = '\0';
    }
    if (argc == 0) return;

    char cmd[12];
    strncpy(cmd, argv[0], sizeof(cmd) - 1); cmd[sizeof(cmd)-1] = '\0';
    to_lower(cmd);

    if (!strcmp(cmd, "help") || !strcmp(cmd, "?"))       { cmd_help();   return; }
    if (!strcmp(cmd, "show"))                            { cmd_show();   return; }
    if (!strcmp(cmd, "status") || !strcmp(cmd, "stat"))  { cmd_status(); return; }
    if (!strcmp(cmd, "loops") || !strcmp(cmd, "loop"))   { cmd_loops();  return; }
    if (!strcmp(cmd, "modem") || !strcmp(cmd, "mdm"))    { cmd_modem();  return; }
    if (!strcmp(cmd, "reboot") || !strcmp(cmd, "reset")) {
        cputs("rebooting...\r\n"); HAL_Delay(100); NVIC_SystemReset();
    }
    if (!strcmp(cmd, "default") || !strcmp(cmd, "defaults")) {
        config_defaults(); cputs("config reset to defaults (RAM). type 'save' to persist.\r\n");
        cmd_show(); return;
    }
    if (!strcmp(cmd, "save")) {
        int r = config_save();
        if (r == 0) cputs("saved to flash. reboot to fully apply (modem/APN).\r\n");
        else { char b[40]; snprintf(b, sizeof(b), "save FAILED (err %d)\r\n", r); cputs(b); }
        return;
    }
    if (!strcmp(cmd, "set")) {
        if (argc < 3) { cputs("usage: set <ip|port|apn|id> <value>\r\n"); return; }
        char key[8]; strncpy(key, argv[1], sizeof(key)-1); key[sizeof(key)-1]='\0'; to_lower(key);
        if (!strcmp(key, "ip")) {
            uint8_t ip[4];
            if (parse_ip(argv[2], ip) == 0) { memcpy(g_cfg.server_ip, ip, 4);
                cputs("ip = "); print_ip(g_cfg.server_ip); cputs("  (type 'save')\r\n"); }
            else cputs("bad IP. format: set ip 5.159.49.246\r\n");
            return;
        }
        if (!strcmp(key, "port")) {
            long v = strtol(argv[2], NULL, 10);
            if (v > 0 && v < 65536) { g_cfg.server_port = (uint16_t)v;
                char b[32]; snprintf(b, sizeof(b), "port = %u  (type 'save')\r\n", g_cfg.server_port); cputs(b); }
            else cputs("bad port (1..65535)\r\n");
            return;
        }
        if (!strcmp(key, "apn")) {
            strncpy(g_cfg.apn, argv[2], sizeof(g_cfg.apn)-1); g_cfg.apn[sizeof(g_cfg.apn)-1]='\0';
            char b[96]; snprintf(b, sizeof(b), "apn = %s  (type 'save', then 'reboot')\r\n", g_cfg.apn); cputs(b);
            return;
        }
        if (!strcmp(key, "id")) {
            strncpy(g_cfg.system_id, argv[2], sizeof(g_cfg.system_id)-1);
            g_cfg.system_id[sizeof(g_cfg.system_id)-1]='\0';
            char b[40]; snprintf(b, sizeof(b), "id = %s  (type 'save')\r\n", g_cfg.system_id); cputs(b);
            return;
        }
        cputs("unknown key. use: ip | port | apn | id\r\n");
        return;
    }
    cputs("unknown command. type 'help'\r\n");
}

/* ─── Console line buffer / poll ─────────────────────────────────────── */
#define CLINE_SZ 96
static char    cline[CLINE_SZ];
static uint8_t clen = 0;

void console_init(void)
{
    cputs("\r\n=== RATCX1-STM32 console ===\r\n");
    cputs("type 'help' for commands, 'show' for config\r\n");
    cmd_show();
    cputs("> ");
}

void console_task(void)
{
    /* clear overrun if any */
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_ORE))
        __HAL_UART_CLEAR_OREFLAG(&huart1);

    while (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE)) {
        char c = (char)(huart1.Instance->DR & 0xFF);

        if (c == '\r' || c == '\n') {
            cnl();
            cline[clen] = '\0';
            if (clen > 0) exec_line(cline);
            clen = 0;
            cputs("> ");
        } else if (c == 0x08 || c == 0x7F) {       /* backspace / DEL */
            if (clen > 0) { clen--; cputs("\b \b"); }
        } else if (c >= 0x20 && c < 0x7F) {
            if (clen < CLINE_SZ - 1) { cline[clen++] = c; cputc(c); }  /* echo */
        }
        if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_ORE))
            __HAL_UART_CLEAR_OREFLAG(&huart1);
    }
}
