/**
 * arduino_air780e_test.ino
 * ─────────────────────────────────────────────────────────────────────────
 * تست Air780E با Arduino Uno – از طریق Serial Monitor
 *
 * ─── اتصال سخت‌افزاری ─────────────────────────────────────────────────
 *
 *  Arduino Uno        Air780E
 *  Pin 10 (RX_soft)  ← TXD    (سیم از TXD ماژول به پین 10 آردوینو)
 *  Pin 11 (TX_soft)  → RXD    (سیم از پین 11 آردوینو به RXD ماژول)
 *  GND               → GND
 *  3.3V / 4.2V       → VCC    (Air780E را از 5V مستقیم آردوینو تغذیه نکنید)
 *
 *  اختیاری:
 *  Pin 7  → PWRKEY  (برای کنترل نرم‌افزاری power)
 *  Pin 8  ← STATUS  (خواندن وضعیت power ماژول)
 *
 * ─── نکته مهم سرعت باد ──────────────────────────────────────────────
 *  Air780E پیش‌فرض 115200 دارد. SoftwareSerial روی Uno فقط تا ~57600
 *  پایدار است. دو راه‌حل:
 *
 *  راه 1 (پیشنهادی): بار اول فقط "SET_BAUD" را فعال کنید (پایین ببینید)
 *                    تا baud ماژول به 9600 تنظیم شود. بعد غیرفعال کنید.
 *
 *  راه 2: از Arduino Mega یا Leonardo استفاده کنید که Serial1 سخت‌افزاری
 *          دارد و از 115200 پشتیبانی می‌کند.
 *
 * ─── روش استفاده ─────────────────────────────────────────────────────
 *  1. کد را آپلود کنید
 *  2. Serial Monitor را روی 115200 baud باز کنید
 *  3. منوی گزینه‌ها:
 *       s  → وضعیت اتصال
 *       a  → تست AT پایه
 *       n  → ثبت شبکه
 *       g  → تست GPRS (open network)
 *       t  → تست TCP connect
 *       d  → قطع TCP
 *       h  → راهنما
 *  4. یا هر AT command را مستقیم تایپ کنید (مثل: AT+CIMI)
 */

#include <SoftwareSerial.h>

/* ─── تنظیمات ─────────────────────────────────────────────────────────── */
#define AIR780_BAUD   9600      /* بعد از SET_BAUD این مقدار را استفاده کنید */
#define SET_BAUD      0         /* 1 = اول baud را تنظیم کن (فقط یک‌بار) */

#define PWRKEY_PIN    7         /* -1 اگر نمی‌خواهید power را کنترل کنید */
#define STATUS_PIN    8         /* -1 اگر STATUS وصل نیست */

#define SERVER_IP     "192.168.1.100"
#define SERVER_PORT   "2022"
#define APN           "mcinet"  /* APN اپراتور: mcinet / mci / rtl / irancell */

/* ─── SoftwareSerial ─────────────────────────────────────────────────── */
SoftwareSerial air780(10, 11);  /* RX=10, TX=11 */

/* ─── متغیرهای داخلی ────────────────────────────────────────────────── */
static bool tcp_connected = false;
static char cmd_buf[80];
static uint8_t cmd_len = 0;

/* ═══════════════════════════════════════════════════════════════════════ */

void setup()
{
    Serial.begin(115200);
    while (!Serial);

    if (PWRKEY_PIN >= 0) {
        pinMode(PWRKEY_PIN, OUTPUT);
        digitalWrite(PWRKEY_PIN, LOW);  /* idle LOW برای Air780 */
    }
    if (STATUS_PIN >= 0)
        pinMode(STATUS_PIN, INPUT);

    Serial.println(F("=== Air780E Test Tool ==="));
    Serial.println(F("تایپ کنید h برای راهنما"));

#if SET_BAUD
    /* ── یک‌بار اجرا کنید تا baud ماژول را به 9600 تغییر دهید ── */
    Serial.println(F(">>> تنظیم baud از 115200 به 9600..."));
    air780.begin(115200);
    delay(500);
    air780.println(F("AT+IPR=9600"));
    delay(500);
    air780.begin(9600);
    delay(500);
    air780.println(F("AT&W"));  /* ذخیره تنظیمات */
    delay(500);
    Serial.println(F(">>> baud تنظیم شد. SET_BAUD را به 0 تغییر دهید و دوباره آپلود کنید."));
#else
    air780.begin(AIR780_BAUD);
#endif

    delay(1000);
    print_help();
}

void loop()
{
    /* ── دریافت از Air780 و نمایش در Serial Monitor ── */
    while (air780.available()) {
        char c = air780.read();
        Serial.write(c);
    }

    /* ── دریافت از Serial Monitor ── */
    while (Serial.available()) {
        char c = (char)Serial.read();

        /* اگر یک کاراکتر تنها بود (منو) */
        if (c == '\n' || c == '\r') {
            if (cmd_len == 1) {
                handle_menu(cmd_buf[0]);
            } else if (cmd_len > 1) {
                /* ارسال مستقیم AT command به ماژول */
                cmd_buf[cmd_len] = '\0';
                Serial.print(F(">> "));
                Serial.println(cmd_buf);
                air780.println(cmd_buf);
            }
            cmd_len = 0;
        } else {
            Serial.write(c);  /* echo */
            if (cmd_len < sizeof(cmd_buf) - 1)
                cmd_buf[cmd_len++] = c;
        }
    }
}

/* ─── منو ────────────────────────────────────────────────────────────── */
void handle_menu(char key)
{
    switch (key) {
    case 'h': case 'H': print_help(); break;
    case 's': case 'S': cmd_status(); break;
    case 'a': case 'A': cmd_at_basic(); break;
    case 'n': case 'N': cmd_net_reg(); break;
    case 'g': case 'G': cmd_gprs_open(); break;
    case 't': case 'T': cmd_tcp_connect(); break;
    case 'd': case 'D': cmd_tcp_disconnect(); break;
    case 'p': case 'P': cmd_power_on(); break;
    case 'i': case 'I': cmd_send_test_data(); break;
    default:
        Serial.print(F("گزینه ناشناخته: "));
        Serial.println(key);
    }
}

/* ─── دستیار ارسال AT و خواندن پاسخ ─────────────────────────────────── */
bool at_cmd(const char *cmd, const char *expect, uint16_t timeout_ms)
{
    /* خالی کردن بافر */
    while (air780.available()) air780.read();

    Serial.print(F("AT> "));
    Serial.println(cmd);
    air780.println(cmd);

    unsigned long start = millis();
    char resp[128];
    uint8_t rlen = 0;
    bool found = false;

    while ((millis() - start) < timeout_ms) {
        while (air780.available()) {
            char c = air780.read();
            Serial.write(c);
            if (c == '\n') {
                resp[rlen] = '\0';
                if (expect && strstr(resp, expect)) found = true;
                rlen = 0;
            } else if (c != '\r') {
                if (rlen < sizeof(resp) - 1)
                    resp[rlen++] = c;
            }
        }
        if (found) break;
        delay(5);
    }

    if (!found && expect) {
        Serial.println(F("  [timeout یا پاسخ دریافت نشد]"));
    }
    Serial.println();
    return found;
}

/* ─── دستورات منو ────────────────────────────────────────────────────── */

void cmd_status()
{
    Serial.println(F("─── وضعیت ─────────────────────"));
    if (STATUS_PIN >= 0) {
        Serial.print(F("STATUS pin: "));
        Serial.println(digitalRead(STATUS_PIN) ? F("HIGH (روشن)") : F("LOW (خاموش)"));
    }
    Serial.print(F("TCP: "));
    Serial.println(tcp_connected ? F("CONNECTED") : F("DISCONNECTED"));
}

void cmd_at_basic()
{
    Serial.println(F("─── تست AT پایه ────────────────"));
    at_cmd("AT",       "OK",    2000);
    at_cmd("ATE0",     "OK",    1000);  /* غیرفعال کردن echo */
    at_cmd("AT+CIMI",  "OK",    3000);  /* IMSI سیم کارت */
    at_cmd("AT+CSQ",   "OK",    2000);  /* سطح سیگنال: +CSQ: xx,0  (xx>5 قابل قبول) */
    at_cmd("AT+CGMR",  "OK",    2000);  /* نسخه firmware ماژول */
}

void cmd_net_reg()
{
    Serial.println(F("─── ثبت شبکه (4G/LTE) ─────────"));
    Serial.println(F("انتظار برای ثبت... (حداکثر 60 ثانیه)"));

    at_cmd("AT+CREG=1",   "OK", 1000);  /* فعال کردن گزارش ثبت شبکه */
    at_cmd("AT+CEREG=1",  "OK", 1000);  /* برای LTE */

    unsigned long start = millis();
    bool registered = false;
    while ((millis() - start) < 60000) {
        /* AT+CEREG? → +CEREG: 0,1 یا 0,5 */
        if (at_cmd("AT+CEREG?", ",1", 2000) || at_cmd("AT+CEREG?", ",5", 2000)) {
            registered = true;
            break;
        }
        /* AT+CREG? برای 2G fallback */
        if (at_cmd("AT+CREG?", ",1", 2000) || at_cmd("AT+CREG?", ",5", 2000)) {
            registered = true;
            break;
        }
        Serial.println(F("هنوز ثبت نشده... صبر کنید"));
        delay(3000);
    }

    if (registered)
        Serial.println(F("✓ ثبت شبکه موفق"));
    else
        Serial.println(F("✗ ثبت شبکه ناموفق – سیم کارت را بررسی کنید"));

    /* اطلاعات اضافی */
    at_cmd("AT+COPS?",  "OK", 3000);  /* اپراتور */
    at_cmd("AT+CSQ",    "OK", 2000);  /* سطح سیگنال */
}

void cmd_gprs_open()
{
    Serial.println(F("─── باز کردن شبکه (NETOPEN) ───"));
    char cmd[60];

    /* APN */
    snprintf(cmd, sizeof(cmd), "AT+CGDCONT=1,\"IP\",\"%s\"", APN);
    at_cmd(cmd, "OK", 3000);

    /* NETOPEN */
    at_cmd("AT+NETOPEN", "NETOPEN", 30000);

    /* IP */
    at_cmd("AT+IPADDR", "OK", 5000);
}

void cmd_tcp_connect()
{
    Serial.println(F("─── تست TCP Connect ─────────────"));
    char cmd[80];
    snprintf(cmd, sizeof(cmd),
             "AT+CIPOPEN=0,\"TCP\",\"%s\",%s", SERVER_IP, SERVER_PORT);

    tcp_connected = false;
    if (at_cmd(cmd, "CIPOPEN: 0,0", 15000)) {
        tcp_connected = true;
        Serial.println(F("✓ TCP متصل شد"));
    } else {
        Serial.println(F("✗ TCP ناموفق"));
    }
}

void cmd_tcp_disconnect()
{
    Serial.println(F("─── قطع TCP ─────────────────────"));
    at_cmd("AT+CIPCLOSE=0", "OK", 5000);
    tcp_connected = false;
}

void cmd_power_on()
{
    if (PWRKEY_PIN < 0) {
        Serial.println(F("PWRKEY_PIN تنظیم نشده"));
        return;
    }
    Serial.println(F("─── روشن/خاموش کردن Air780E ───"));
    Serial.println(F("پالس PWRKEY HIGH به مدت 600ms..."));
    digitalWrite(PWRKEY_PIN, HIGH);
    delay(600);
    digitalWrite(PWRKEY_PIN, LOW);
    Serial.println(F("انتظار برای راه‌اندازی... (5 ثانیه)"));
    delay(5000);
    at_cmd("AT", "OK", 3000);
}

void cmd_send_test_data()
{
    if (!tcp_connected) {
        Serial.println(F("اول TCP را وصل کنید (دستور t)"));
        return;
    }
    /* ارسال یک پیام تست کوچک */
    const char *msg = "PING";
    uint8_t len = strlen(msg);
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=0,%u", len);

    Serial.println(F("─── ارسال داده تست ──────────────"));
    air780.println(cmd);
    delay(500);
    /* انتظار برای '>' */
    unsigned long start = millis();
    bool got_prompt = false;
    while ((millis() - start) < 3000) {
        if (air780.available()) {
            char c = air780.read();
            Serial.write(c);
            if (c == '>') { got_prompt = true; break; }
        }
    }
    if (got_prompt) {
        air780.print(msg);
        Serial.println(F("داده ارسال شد"));
        /* انتظار برای SEND OK */
        at_cmd("", "SEND OK", 5000);
    } else {
        Serial.println(F("پرامپت '>' دریافت نشد"));
    }
}

void print_help()
{
    Serial.println(F(""));
    Serial.println(F("═══════ راهنمای دستورات ════════"));
    Serial.println(F("  a  → تست AT پایه (AT, CIMI, CSQ)"));
    Serial.println(F("  n  → ثبت شبکه (CEREG/CREG)"));
    Serial.println(F("  g  → باز کردن GPRS (NETOPEN)"));
    Serial.println(F("  t  → TCP Connect به سرور"));
    Serial.println(F("  i  → ارسال داده تست"));
    Serial.println(F("  d  → قطع TCP"));
    Serial.println(F("  s  → وضعیت"));
    Serial.println(F("  p  → روشن/خاموش (نیاز به PWRKEY)"));
    Serial.println(F("  h  → این راهنما"));
    Serial.println(F("  یا AT command مستقیم تایپ کنید"));
    Serial.println(F("  مثال: AT+CSQ"));
    Serial.println(F("════════════════════════════════"));
    Serial.print(F("سرور: "));
    Serial.print(SERVER_IP);
    Serial.print(F(":"));
    Serial.println(SERVER_PORT);
    Serial.print(F("APN: "));
    Serial.println(APN);
    Serial.println();
}
