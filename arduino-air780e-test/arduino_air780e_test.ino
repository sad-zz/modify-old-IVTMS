/**
 * arduino_air780e_test.ino  [نسخه 3 – Minimal Bridge]
 * ─────────────────────────────────────────────────────────────────────────
 *
 * ─── سیم‌کشی ────────────────────────────────────────────────────────────
 *
 *  Air780E          Arduino Uno
 *  ─────────────────────────────
 *  VCC (3.3V) ←─── منبع خارجی 3.3V  (نه از آردوینو! حداقل 500mA)
 *  GND        ←─── GND آردوینو  (GND مشترک حتما!)
 *  TXD        ───► پین 0 (RX)   *سیم قبل آپلود جدا کن*
 *  RXD        ◄─── پین 1 (TX)   *سیم قبل آپلود جدا کن*
 *  EN         ←─── 3.3V  (حتما HIGH باشد)
 *  ON         ←─── پین 6  (PWRKEY – برای روشن کردن)
 *  LED        ───► پین 7  (STATUS – وضعیت power)
 *
 *  اگر منبع خارجی ندارید:
 *    باتری LiPo 3.7V مستقیم به BAT وصل کنید + GND مشترک
 *
 * ─── آپلود ───────────────────────────────────────────────────────────────
 *  1. سیم پین 0 و 1 را جدا کنید
 *  2. Upload کنید
 *  3. سیم‌ها را دوباره وصل کنید
 *
 * ─── استفاده ─────────────────────────────────────────────────────────────
 *  Serial Monitor → 115200 baud → "Both NL & CR"
 *  هر چه تایپ کنید مستقیم به Air780E می‌رود
 *  هر چه Air780E بفرستد در Serial Monitor نشان داده می‌شود
 *
 *  چند ثانیه بعد از وصل کردن باید ببینید:
 *    [PWRKEY pulse sent]
 *    RDY
 *    +CPIN: READY
 *    +CEREG: ...
 *  اگر این‌ها آمد → ماژول زنده است → AT+CSQ را تست کنید
 */

#define PWRKEY_PIN   6   /* ON pin روی ماژول */
#define STATUS_PIN   7   /* LED pin روی ماژول */

void setup()
{
    Serial.begin(115200);

    pinMode(PWRKEY_PIN, OUTPUT);
    digitalWrite(PWRKEY_PIN, HIGH);   /* idle HIGH برای Air780E */

    pinMode(STATUS_PIN, INPUT);

    delay(500);
    Serial.println(F("=== Air780E Bridge ==="));

    /* بررسی وضعیت power */
    bool is_on = (digitalRead(STATUS_PIN) == HIGH);
    Serial.print(F("STATUS pin: "));
    Serial.println(is_on ? F("HIGH (احتمالا روشن)") : F("LOW (خاموش یا STATUS وصل نیست)"));

    /* اگر خاموش بود یا STATUS وصل نبود → پالس PWRKEY بده */
    if (!is_on) {
        Serial.println(F("[PWRKEY pulse: LOW 600ms]"));
        digitalWrite(PWRKEY_PIN, LOW);
        delay(600);
        digitalWrite(PWRKEY_PIN, HIGH);
        Serial.println(F("[منتظر boot ماژول... 8 ثانیه]"));
        delay(8000);
    }

    Serial.println(F("آماده – هر AT command بنویسید:"));
    Serial.println(F("  AT         <- تست اتصال"));
    Serial.println(F("  AT+CSQ     <- سطح سیگنال"));
    Serial.println(F("  AT+CIMI    <- سیم کارت"));
    Serial.println(F("  AT+CEREG?  <- ثبت شبکه LTE"));
    Serial.println(F("-------------------------------"));
}

void loop()
{
    /* Bridge کامل – هر بایت در هر جهت منتقل می‌شود */
    /* Air780 → Serial Monitor: توسط hardware انجام می‌شود (پین 0 به USB) */
    /* Serial Monitor → Air780: توسط hardware انجام می‌شود (USB به پین 1) */
    /* هیچ کدی لازم نیست! */
}
