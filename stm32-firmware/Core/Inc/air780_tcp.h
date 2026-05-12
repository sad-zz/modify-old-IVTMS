/**
 * @file air780_tcp.h
 * @brief Air780 4G LTE Cat.1 module – AT-command TCP driver
 *
 * جایگزین W5500 Ethernet در فریمور RATCX1-STM32.
 * ماژول Air780 (سری EC618 شرکت LuatOS) از طریق USART3 کنترل می‌شود.
 *
 * ─── اتصال سخت‌افزاری ─────────────────────────────────────────────────
 *
 *  STM32F103   Air780
 *  PB10 (TX3) → RXD
 *  PB11 (RX3) ← TXD
 *  PA8        → PWRKEY  (HIGH pulse ≥500ms to power on, ≥2500ms to power off)
 *  PA11       ← STATUS  (HIGH = module powered on)
 *  GND        → GND
 *  3.3V / 4.2V → VCC    (Air780E can run on 3.3V–4.2V; check your module)
 *
 * (پین‌های قدیمی PB12/PB13 آزاد شدند برای SD card روی SPI2.)
 *
 * ─── پروتکل AT (سری EC618) ────────────────────────────────────────────
 *  AT+NETOPEN               → open network (response: +NETOPEN: 0)
 *  AT+CIPOPEN=0,"TCP",ip,port → open TCP (response: +CIPOPEN: 0,0)
 *  AT+CIPSEND=0,len         → send (waits for '>', then send bytes)
 *  +IPD: 0,len\r\n<data>    → URC for incoming data
 *  +CIPEVENT: 0,CLOSED      → URC for connection closed
 *  AT+CIPCLOSE=0            → close TCP
 *  AT+NETCLOSE              → close network
 *
 * ─── API ──────────────────────────────────────────────────────────────
 * Same public surface as the old w5500_tcp.h so protocol.c needs no change.
 */

#ifndef AIR780_TCP_H
#define AIR780_TCP_H

#include <stdint.h>

/* ─── GPIO Pins ──────────────────────────────────────────────────────── */
#define AIR780_UART_BAUD    115200UL  /* USART3 baud rate                 */

/* ─── Public API (identical signatures to old w5500_tcp.h) ─────────── */

/**
 * @brief Power on Air780, wait for registration and open network.
 * @return 0 on success, non-zero on timeout/error.
 */
int  air780_init(void);

/**
 * @brief Open a TCP connection to the TC Manager server.
 * @param server_ip   4-byte IP address (or pass dotted-string via AIR780_SERVER_HOST)
 * @param server_port TCP port number
 * @return 0 on success, non-zero on failure.
 */
int  tcp_connect(uint8_t *server_ip, uint16_t server_port);

/**
 * @brief Close the TCP connection (AT+CIPCLOSE).
 */
void tcp_disconnect(void);

/**
 * @brief Send data over the open TCP connection.
 * @param buf  data buffer
 * @param len  byte count
 * @return bytes sent (==len), or <0 on error.
 */
int  tcp_send(const uint8_t *buf, uint16_t len);

/**
 * @brief Non-blocking read from internal receive buffer.
 *        Must be called regularly from the main loop to drain URCs.
 * @param buf   destination buffer
 * @param len   maximum bytes to copy
 * @return bytes copied (0 = no data ready), <0 = connection lost.
 */
int  tcp_recv(uint8_t *buf, uint16_t len);

/**
 * @brief Return 1 if TCP socket is ESTABLISHED, 0 otherwise.
 */
uint8_t tcp_is_connected(void);

/**
 * @brief Background task – must be called from main loop as fast as possible.
 *        Drains UART3 RX FIFO, processes URCs (+IPD, +CIPEVENT etc.).
 */
void air780_task(void);

/**
 * @brief Send a raw AT command and capture the response (for the serial console).
 * @param cmd        AT command WITHOUT trailing CRLF (e.g. "AT+CSQ")
 * @param resp       buffer to receive the response text (CRLF-separated lines)
 * @param resp_sz    size of resp
 * @param timeout_ms how long to wait for "OK"/"ERROR"
 * @return 0 if "OK", 1 if "ERROR"/"+CME ERROR"/..., -1 on timeout
 */
int air780_at_raw(const char *cmd, char *resp, int resp_sz, uint32_t timeout_ms);

/* ─── Internal UART3 ISR – call from USART3_IRQHandler ─────────────── */
void air780_uart_irq(void);

#endif /* AIR780_TCP_H */
