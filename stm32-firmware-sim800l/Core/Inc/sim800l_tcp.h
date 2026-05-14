/**
 * @file sim800l_tcp.h
 * @brief SIM800L GPRS – USART3 driver + AT-command TCP state machine
 *
 * ─── اتصال سخت‌افزاری ─────────────────────────────────────────────────
 *
 *  STM32F103   SIM800L
 *  PB10 (TX3) → RXD
 *  PB11 (RX3) ← TXD
 *  PA8        → PWRKEY  (pulse LOW ≥1s to toggle power on/off)
 *  PA11       ← STATUS  (HIGH = module powered on)  [اختیاری]
 *  GND        → GND
 *  4.0–4.2V   → VCC     (مهم: SIM800L با 3.3V کار نمی‌کند – به LiPo یا رگولاتور 4V نیاز دارد)
 *
 * ─── تفاوت‌های کلیدی با Air780 ────────────────────────────────────────
 *  | ویژگی            | Air780                  | SIM800L                  |
 *  |-------------------|-------------------------|--------------------------|
 *  | ثبت شبکه         | AT+CEREG?               | AT+CREG?                 |
 *  | تنظیم APN        | AT+CGDCONT              | AT+CSTT                  |
 *  | فعال‌سازی GPRS   | AT+NETOPEN              | AT+CIICR                 |
 *  | باز کردن TCP     | AT+CIPOPEN=0,"TCP",...  | AT+CIPSTART="TCP",...    |
 *  | ارسال داده       | AT+CIPSEND=0,len        | AT+CIPSEND=len           |
 *  | تایید ارسال      | +CIPSEND: 0,len         | SEND OK                  |
 *  | دریافت داده      | +IPD: 0,len\r\n[bytes]  | +IPD,len:data (inline)   |
 *  | بسته شدن         | +CIPEVENT: 0,CLOSED     | CLOSED                   |
 *  | نرخ باد پیش‌فرض  | 115200                  | 9600 (auto-baud)         |
 *  | PWRKEY           | پالس HIGH ≥600ms         | پالس LOW ≥1s             |
 *
 * ─── API (امضای یکسان با air780_tcp.h) ────────────────────────────────
 * همان public API تا protocol.c بدون تغییر کار کند.
 */

#ifndef SIM800L_TCP_H
#define SIM800L_TCP_H

#include <stdint.h>

#define SIM800L_UART_BAUD   9600UL   /* نرخ پیش‌فرض SIM800L (auto-baud) */

/**
 * @brief راه‌اندازی SIM800L: power on، ثبت شبکه، فعال‌سازی GPRS.
 * @return 0 موفق، مقدار منفی در صورت خطا.
 */
int  sim800l_init(void);

/**
 * @brief برقراری اتصال TCP به سرور.
 * @param server_ip   آرایه 4 بایتی آدرس IP
 * @param server_port  شماره پورت TCP
 * @return 0 موفق، مقدار منفی در صورت خطا.
 */
int  tcp_connect(uint8_t *server_ip, uint16_t server_port);

/** @brief قطع اتصال TCP. */
void tcp_disconnect(void);

/**
 * @brief ارسال داده روی اتصال TCP باز.
 * @return تعداد بایت ارسالی (==len) یا <0 در صورت خطا.
 */
int  tcp_send(const uint8_t *buf, uint16_t len);

/**
 * @brief خواندن غیر blocking از بافر دریافتی.
 * @return تعداد بایت کپی شده (0=بدون داده)، <0=اتصال قطع شده.
 */
int  tcp_recv(uint8_t *buf, uint16_t len);

/** @brief برگرداندن 1 اگر TCP برقرار باشد. */
uint8_t tcp_is_connected(void);

/** @brief تسک پس‌زمینه – در هر بار loop اجرا شود. */
void sim800l_task(void);

/** @brief ISR داخلی USART3 – از USART3_IRQHandler صدا بزنید. */
void sim800l_uart_irq(void);

#endif /* SIM800L_TCP_H */
