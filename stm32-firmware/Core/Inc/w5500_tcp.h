/**
 * @file w5500_tcp.h
 * @brief W5500 Ethernet chip – SPI driver and TCP socket wrapper
 *
 * جایگزین ماژول SIM900 GPRS و پروتکل AT commands در فریمور اصلی
 *
 * Hardware connections (STM32F103C8T6 Blue Pill):
 *   PA5 → W5500 SCLK  (SPI1_SCK)
 *   PA6 → W5500 MISO  (SPI1_MISO)
 *   PA7 → W5500 MOSI  (SPI1_MOSI)
 *   PA4 → W5500 /CS   (GPIO output, active-low)
 *   PB2 → W5500 /RST  (GPIO output, active-low)
 *   PB3 → W5500 INT   (GPIO input, optional – can poll instead)
 *
 * Depends on WIZnet ioLibrary_Driver (https://github.com/Wiznet/ioLibrary_Driver)
 * Copy these files from ioLibrary_Driver into Drivers/W5500/:
 *   Ethernet/wizchip_conf.h   Ethernet/wizchip_conf.c
 *   Ethernet/W5500/w5500.h    Ethernet/W5500/w5500.c
 *   Internet/DHCP/dhcp.h      Internet/DHCP/dhcp.c  (optional)
 * Socket API is provided by socket.h / socket.c in the library.
 */

#ifndef W5500_TCP_H
#define W5500_TCP_H

#include <stdint.h>

/* ─── W5500 Network Configuration ──────────────────────────────────── */
/* MAC address – must be unique on your network                          */
#define W5500_MAC   {0x00, 0x08, 0xDC, 0xAB, 0xCD, 0xEF}

/* Static IP configuration (edit to match your network)                  */
#define W5500_IP    {192, 168, 1, 200}
#define W5500_MASK  {255, 255, 255, 0}
#define W5500_GW    {192, 168, 1, 1}
#define W5500_DNS   {8, 8, 8, 8}

/* Socket number used for server connection (0-7)                        */
#define TCP_SOCKET  0

/* Receive/transmit buffer size for the socket (bytes)                   */
#define TCP_RX_BUF  512
#define TCP_TX_BUF  512

/* ─── Public API ─────────────────────────────────────────────────────── */

/**
 * @brief Initialise SPI1, GPIO pins, W5500 chip and apply IP configuration.
 * @return 0 on success, non-zero on error.
 */
int  w5500_init(void);

/**
 * @brief Open a TCP connection to the TC Manager server.
 * @param server_ip   4-byte IP address array
 * @param server_port destination port (2022)
 * @return 0 on success (SOCK_ESTABLISHED), non-zero on failure.
 */
int  tcp_connect(uint8_t *server_ip, uint16_t server_port);

/**
 * @brief Close the current TCP connection.
 */
void tcp_disconnect(void);

/**
 * @brief Send data over the open TCP connection.
 * @param buf   pointer to data
 * @param len   number of bytes
 * @return bytes actually sent, or <0 on error.
 */
int  tcp_send(const uint8_t *buf, uint16_t len);

/**
 * @brief Receive data from the TCP connection (non-blocking).
 * @param buf   destination buffer
 * @param len   maximum bytes to read
 * @return bytes received (0 = no data), or <0 on connection lost.
 */
int  tcp_recv(uint8_t *buf, uint16_t len);

/**
 * @brief Check whether the TCP connection is still alive.
 * @return 1 = connected, 0 = disconnected.
 */
uint8_t tcp_is_connected(void);

/* ─── SPI callbacks required by WIZnet ioLibrary ─────────────────────── */
/* These are implemented in w5500_spi.c and registered with wizchip_init() */
void  w5500_cs_select(void);
void  w5500_cs_deselect(void);
uint8_t w5500_spi_read(void);
void  w5500_spi_write(uint8_t byte);

#endif /* W5500_TCP_H */
