/**
 * arduino_air780e_test.ino  [نسخه 2 – Hardware Serial]
 * ─────────────────────────────────────────────────────────────────────────
 * تست Air780E با Arduino Uno – حل مشکل baud 115200
 *
 * ─── مشکل نسخه قبلی ───────────────────────────────────────────────────
 *  SoftwareSerial روی Uno فقط تا ~38400 پایدار است.
 *  Air780E پیش‌فرض 115200 دارد → garbage در serial monitor.
 *
 * ─── راه‌حل: Bridge Mode ───────────────────────────────────────────────
 *  از hardware Serial (پین‌های 0 و 1) برای Air780 استفاده می‌کنیم.
 *  آردوینو به‌عنوان یک USB-to-TTL bridge عمل می‌کند:
 *    PC (Serial Monitor) ↔ USB ↔ Arduino ↔ Serial(0/1) ↔ Air780E
 *
 * ─── اتصال سخت‌افزاری ─────────────────────────────────────────────────
 *
 *  Arduino Uno        Air780E
 *  Pin 0 (RX)    ←   TXD
 *  Pin 1 (TX)    →   RXD
 *  GND           →   GND
 *  3.3V          →   VCC  (یا منبع 3.7-4.2V جداگانه)
 *
 *  اختیاری:
 *  Pin 7         →   PWRKEY
 *  Pin 8         ←   STATUS
 *
 * ─── مهم: آپلود کردن ──────────────────────────────────────────────────
 *  قبل از آپلود، سیم‌های پین 0 و 1 را جدا کنید!
 *  بعد از آپلود دوباره وصل کنید.
 *
 * ─── روش استفاده ─────────────────────────────────────────────────────
 *  1. Serial Monitor را روی 115200 baud باز کنید
 *  2. "Line ending: Both NL & CR" را انتخاب کنید
 *  3. هر AT command را مستقیم تایپ کنید:
 *       AT           → OK
 *       AT+CIMI      → IMSI سیم کارت
 *       AT+CSQ       → سطح سیگنال
 *       AT+CEREG?    → ثبت شبکه LTE
 *  4. یا کاراکترهای منو:
 *       ! → تست خودکار کامل (AT + SIM + Network + GPRS + TCP)
 *       ? → راهنما
 *       @ → وضعیت
 */

/* ─── تنظیمات ─────────────────────────────────────────────────────────── */
#define AIR780_BAUD   115200UL    /* بaud پیش‌فرض Air780E */

#define PWRKEY_PIN    7           /* -1 اگر PWRKEY وصل نیست */
#define STATUS_PIN    8           /* -1 اگر STATUS وصل نیست */

#define SERVER_IP     "192.168.1.100"
#define SERVER_PORT   "2022"
#define APN           "mcinet"    /* APN: mcinet / mci / rtl / mtnirancell */

/* ═══════════════════════════════════════════════════════════════════════ */

static char  cmd_buf[120];
static uint8_t cmd_len = 0;
static bool  tcp_connected = false;

/* ─────────────────────────────────────────────────────────────────────── */
void setup()
{
    Serial.begin(AIR780_BAUD);
    while (!Serial);

    if (PWRKEY_PIN >= 0) { pinMode(PWRKEY_PIN, OUTPUT); digitalWrite(PWRKEY_PIN, LOW); }
    if (STATUS_PIN >= 0)   pinMode(STATUS_PIN, INPUT);

    delay(500);

    Serial.println(F("\r\n=== Air780E Test – Hardware Serial Bridge ==="));
    Serial.println(F("هر AT command را تایپ کنید، یا:"));
    Serial.println(F("  !  → تست خودکار کامل"));
    Serial.println(F("  ?  → راهنما"));
    Serial.println(F("  @  → وضعیت"));
    Serial.println(F("مطمئن شوید: Line ending = Both NL & CR"));
    Serial.println();
}

/* ─────────────────────────────────────────────────────────────────────── */
void loop()
{
    /* ── هر چه از Air780 می‌آید مستقیم به Serial Monitor برو ── */
    while (Serial.available()) {
        char c = (char)Serial.read();

        /* ── جمع کردن کاراکترها تا Enter ── */
        if (c == '\n' || c == '\r') {
            if (cmd_len == 0) continue;
            cmd_buf[cmd_len] = '\0';

            /* بررسی کاراکترهای منو (یک کاراکتر) */
            if (cmd_len == 1) {
                switch (cmd_buf[0]) {
                    case '!': run_full_test();   cmd_len = 0; return;
                    case '?': print_help();       cmd_len = 0; return;
                    case '@': print_status();     cmd_len = 0; return;
                    case 'P': case 'p': do_pwrkey(); cmd_len = 0; return;
                }
            }

            /* ارسال مستقیم AT command به Air780 */
            Serial.print(F("→ "));
            Serial.println(cmd_buf);
            Serial.println(cmd_buf);   /* پین 1 (TX) → Air780 */
            cmd_len = 0;

        } else if (c != '\r') {
            if (cmd_len < sizeof(cmd_buf) - 1)
                cmd_buf[cmd_len++] = c;
        }
    }
}

/* ─── at_cmd: ارسال AT و انتظار برای پاسخ ──────────────────────────── */
bool at_cmd(const char *cmd, const char *expect, uint16_t timeout_ms)
{
    /* خالی کردن بافر ورودی */
    while (Serial.available()) Serial.read();
    delay(20);

    if (cmd && cmd[0]) {
        Serial.print(F("AT> "));
        Serial.println(cmd);
        Serial.println(cmd);
    }

    if (!expect) return true;

    unsigned long start = millis();
    char line[128];
    uint8_t llen = 0;
    bool found = false;

    while ((millis() - start) < timeout_ms) {
        while (Serial.available()) {
            char c = (char)Serial.read();
            Serial.write(c);                     /* echo به Monitor */
            if (c == '\n') {
                line[llen] = '\0';
                if (strstr(line, expect)) { found = true; }
                if (strstr(line, "CLOSED")) tcp_connected = false;
                if (strstr(line, "CONNECT OK")) tcp_connected = true;
                llen = 0;
            } else if (c != '\r') {
                if (llen < sizeof(line) - 1) line[llen++] = c;
            }
        }
        if (found) break;
        delay(5);
    }

    if (!found) {
        Serial.println(F("  ✗ [timeout]"));
    }
    Serial.println();
    return found;
}

/* ─── at_wait_line: منتظر هر پاسخی بمان ─────────────────────────────── */
void at_drain(uint16_t timeout_ms)
{
    unsigned long start = millis();
    while ((millis() - start) < timeout_ms) {
        while (Serial.available()) {
            Serial.write(Serial.read());
            start = millis();   /* ریست timeout هر بار که داده می‌آید */
        }
        delay(2);
    }
}

/* ─── تست خودکار کامل ─────────────────────────────────────────────────── */
void run_full_test()
{
    Serial.println(F("════════ تست خودکار Air780E ════════"));

    /* --- مرحله 1: ارتباط پایه --- */
    Serial.println(F("\n[1] ارتباط پایه"));
    if (!at_cmd("AT", "OK", 2000)) {
        Serial.println(F("✗ ماژول پاسخ نمی‌دهد!"));
        Serial.println(F("  بررسی کنید: سیم TXD/RXD، ولتاژ، PWRKEY"));
        return;
    }
    Serial.println(F("✓ ماژول پاسخ داد"));

    at_cmd("ATE0", "OK", 1000);           /* غیرفعال کردن echo */
    at_cmd("AT+CGMR", "OK", 2000);        /* نسخه firmware */

    /* --- مرحله 2: سیم کارت --- */
    Serial.println(F("\n[2] سیم کارت"));
    if (!at_cmd("AT+CIMI", "OK", 3000)) {
        Serial.println(F("✗ سیم کارت شناسایی نشد – بررسی کنید"));
        return;
    }
    Serial.println(F("✓ سیم کارت OK"));

    /* --- مرحله 3: سیگنال --- */
    Serial.println(F("\n[3] سطح سیگنال"));
    at_cmd("AT+CSQ", "OK", 2000);
    /* +CSQ: xx,0 → xx=0..31 (99=بدون سیگنال) */
    Serial.println(F("  (xx>10 قابل قبول، 99=بدون سیگنال)"));

    /* --- مرحله 4: ثبت شبکه LTE --- */
    Serial.println(F("\n[4] ثبت شبکه LTE (تا 60 ثانیه)"));
    at_cmd("AT+CEREG=1", "OK", 1000);

    bool registered = false;
    unsigned long start = millis();
    while ((millis() - start) < 60000) {
        if (at_cmd("AT+CEREG?", "0,1", 2000) ||
            at_cmd("AT+CEREG?", "0,5", 2000)) {
            registered = true;
            break;
        }
        Serial.println(F("  در حال ثبت... صبر کنید"));
        delay(3000);
    }
    if (!registered) {
        Serial.println(F("✗ ثبت شبکه ناموفق"));
        Serial.println(F("  بررسی کنید: آنتن، سیم کارت 4G، پوشش منطقه"));
        return;
    }
    Serial.println(F("✓ ثبت شبکه موفق"));
    at_cmd("AT+COPS?", "OK", 3000);       /* نام اپراتور */

    /* --- مرحله 5: GPRS/Network open --- */
    Serial.println(F("\n[5] باز کردن شبکه (NETOPEN)"));
    {
        char apn_cmd[60];
        snprintf(apn_cmd, sizeof(apn_cmd), "AT+CGDCONT=1,\"IP\",\"%s\"", APN);
        at_cmd(apn_cmd, "OK", 3000);
    }
    if (!at_cmd("AT+NETOPEN", "NETOPEN", 30000)) {
        Serial.println(F("✗ NETOPEN ناموفق – APN را بررسی کنید"));
        return;
    }
    Serial.println(F("✓ شبکه باز شد"));
    at_cmd("AT+IPADDR", "OK", 5000);      /* IP دریافتی */

    /* --- مرحله 6: TCP Connect --- */
    Serial.println(F("\n[6] اتصال TCP به سرور"));
    {
        char tcp_cmd[80];
        snprintf(tcp_cmd, sizeof(tcp_cmd),
                 "AT+CIPOPEN=0,\"TCP\",\"%s\",%s", SERVER_IP, SERVER_PORT);
        if (at_cmd(tcp_cmd, "CIPOPEN: 0,0", 15000)) {
            tcp_connected = true;
            Serial.println(F("✓ TCP متصل شد!"));
        } else {
            Serial.println(F("✗ TCP ناموفق – سرور را بررسی کنید"));
        }
    }

    Serial.println(F("\n════════ پایان تست ════════"));
    print_status();
}

/* ─────────────────────────────────────────────────────────────────────── */
void do_pwrkey()
{
    if (PWRKEY_PIN < 0) { Serial.println(F("PWRKEY_PIN=-1")); return; }
    Serial.println(F("پالس PWRKEY (600ms HIGH)..."));
    digitalWrite(PWRKEY_PIN, HIGH);
    delay(600);
    digitalWrite(PWRKEY_PIN, LOW);
    Serial.println(F("منتظر boot... (5s)"));
    delay(5000);
    at_cmd("AT", "OK", 3000);
}

void print_status()
{
    Serial.println(F("─── وضعیت ───────────────────────"));
    if (STATUS_PIN >= 0) {
        Serial.print(F("  STATUS: "));
        Serial.println(digitalRead(STATUS_PIN) ? F("HIGH (روشن)") : F("LOW (خاموش)"));
    }
    Serial.print(F("  TCP: "));
    Serial.println(tcp_connected ? F("CONNECTED ✓") : F("DISCONNECTED"));
    Serial.print(F("  سرور: "));
    Serial.print(SERVER_IP); Serial.print(F(":")); Serial.println(SERVER_PORT);
    Serial.print(F("  APN: ")); Serial.println(APN);
}

void print_help()
{
    Serial.println(F("════════ راهنما ════════════════"));
    Serial.println(F("  !  → تست خودکار کامل"));
    Serial.println(F("  @  → وضعیت"));
    Serial.println(F("  p  → پالس PWRKEY (روشن/خاموش)"));
    Serial.println(F("  ?  → این راهنما"));
    Serial.println(F("─── AT commands مستقیم ─────────"));
    Serial.println(F("  AT           → بررسی ارتباط"));
    Serial.println(F("  AT+CIMI      → IMSI سیم کارت"));
    Serial.println(F("  AT+CSQ       → سطح سیگنال"));
    Serial.println(F("  AT+CEREG?    → ثبت شبکه LTE"));
    Serial.println(F("  AT+COPS?     → اپراتور"));
    Serial.println(F("  AT+CGDCONT=1,\"IP\",\"mcinet\"  → APN"));
    Serial.println(F("  AT+NETOPEN   → باز کردن شبکه"));
    Serial.println(F("  AT+IPADDR    → آدرس IP"));
    Serial.println(F("════════════════════════════════"));
}
