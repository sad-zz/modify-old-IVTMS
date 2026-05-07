/**
 * @file w5500_tcp.c
 * @brief W5500 Ethernet – SPI driver + TCP socket wrapper
 *
 * این فایل SPI callbacks مورد نیاز WIZnet ioLibrary را پیاده‌سازی می‌کند
 * و یک API ساده TCP را در اختیار protocol.c می‌گذارد.
 *
 * ─────────────────────────────────────────────────────────────────────
 * Dependencies – download and add to your project:
 *   https://github.com/Wiznet/ioLibrary_Driver
 *
 *   Required files (copy to Drivers/W5500/):
 *     Ethernet/wizchip_conf.h
 *     Ethernet/wizchip_conf.c
 *     Ethernet/W5500/w5500.h
 *     Ethernet/W5500/w5500.c
 *     Internet/socket.h
 *     Internet/socket.c
 *
 *   Add these to your STM32CubeIDE / Makefile include paths:
 *     -IDrivers/W5500/Ethernet
 *     -IDrivers/W5500/Ethernet/W5500
 * ─────────────────────────────────────────────────────────────────────
 */

#include "w5500_tcp.h"
#include "stm32f1xx_hal.h"
#include <string.h>

/* ─── WIZnet library headers ─────────────────────────────────────────── */
/* Uncomment after copying the library files to Drivers/W5500/            */
/*
#include "wizchip_conf.h"
#include "socket.h"
*/

/* ─── Extern HAL handles (defined in main.c) ─────────────────────────── */
extern SPI_HandleTypeDef hspi1;

/* ─── GPIO pin definitions ───────────────────────────────────────────── */
#define W5500_CS_PORT   GPIOA
#define W5500_CS_PIN    GPIO_PIN_4
#define W5500_RST_PORT  GPIOB
#define W5500_RST_PIN   GPIO_PIN_2

/* ─── SPI callbacks (registered with wizchip_init) ──────────────────── */
void w5500_cs_select(void)
{
    HAL_GPIO_WritePin(W5500_CS_PORT, W5500_CS_PIN, GPIO_PIN_RESET);
}

void w5500_cs_deselect(void)
{
    HAL_GPIO_WritePin(W5500_CS_PORT, W5500_CS_PIN, GPIO_PIN_SET);
}

uint8_t w5500_spi_read(void)
{
    uint8_t rx = 0;
    HAL_SPI_Receive(&hspi1, &rx, 1, HAL_MAX_DELAY);
    return rx;
}

void w5500_spi_write(uint8_t byte)
{
    HAL_SPI_Transmit(&hspi1, &byte, 1, HAL_MAX_DELAY);
}

/* ─── W5500 hardware reset ───────────────────────────────────────────── */
static void w5500_hw_reset(void)
{
    HAL_GPIO_WritePin(W5500_RST_PORT, W5500_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(W5500_RST_PORT, W5500_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(150);   /* W5500 needs ~150ms after reset */
}

/* ─── w5500_init ─────────────────────────────────────────────────────── */
int w5500_init(void)
{
    /* Hardware reset */
    w5500_hw_reset();

    /*
     * ── WIZnet ioLibrary initialisation (uncomment after adding library) ──
     *
     * uint8_t mac[]  = W5500_MAC;
     * uint8_t ip[]   = W5500_IP;
     * uint8_t mask[] = W5500_MASK;
     * uint8_t gw[]   = W5500_GW;
     * uint8_t dns[]  = W5500_DNS;
     *
     * uint8_t txsize[8] = {2,2,2,2,2,2,2,2};  // 2KB per socket
     * uint8_t rxsize[8] = {2,2,2,2,2,2,2,2};
     *
     * // Register SPI callbacks
     * reg_wizchip_cs_cbfunc(w5500_cs_select, w5500_cs_deselect);
     * reg_wizchip_spi_cbfunc(w5500_spi_read, w5500_spi_write);
     *
     * // Initialise W5500 chip
     * if (ctlwizchip(CW_INIT_WIZCHIP, (void*)txsize) != 0) return -1;
     *
     * // Check chip version
     * uint8_t ver;
     * ctlwizchip(CW_GET_ID, (void*)&ver);
     *
     * // Apply network config
     * wiz_NetInfo netInfo = {
     *     .mac  = {mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]},
     *     .ip   = {ip[0],ip[1],ip[2],ip[3]},
     *     .sn   = {mask[0],mask[1],mask[2],mask[3]},
     *     .gw   = {gw[0],gw[1],gw[2],gw[3]},
     *     .dns  = {dns[0],dns[1],dns[2],dns[3]},
     *     .dhcp = NETINFO_STATIC
     * };
     * ctlnetwork(CN_SET_NETINFO, (void*)&netInfo);
     *
     * return 0;
     */

    return 0;  /* placeholder until library is added */
}

/* ─── tcp_connect ────────────────────────────────────────────────────── */
int tcp_connect(uint8_t *server_ip, uint16_t server_port)
{
    /*
     * Using WIZnet ioLibrary socket API:
     *
     * if (socket(TCP_SOCKET, Sn_MR_TCP, 0, 0) != TCP_SOCKET) return -1;
     *
     * uint32_t timeout_ms = 5000;
     * uint32_t start = HAL_GetTick();
     * int ret = connect(TCP_SOCKET, server_ip, server_port);
     * if (ret != SOCK_OK) {
     *     close(TCP_SOCKET);
     *     return -2;
     * }
     *
     * // Wait for ESTABLISHED
     * while (getSn_SR(TCP_SOCKET) != SOCK_ESTABLISHED) {
     *     if ((HAL_GetTick() - start) > timeout_ms) {
     *         close(TCP_SOCKET);
     *         return -3;
     *     }
     *     HAL_Delay(10);
     * }
     * return 0;
     */

    (void)server_ip;
    (void)server_port;
    return 0;  /* placeholder */
}

/* ─── tcp_disconnect ─────────────────────────────────────────────────── */
void tcp_disconnect(void)
{
    /*
     * disconnect(TCP_SOCKET);
     * close(TCP_SOCKET);
     */
}

/* ─── tcp_send ───────────────────────────────────────────────────────── */
int tcp_send(const uint8_t *buf, uint16_t len)
{
    /*
     * int32_t ret = send(TCP_SOCKET, (uint8_t *)buf, len);
     * return (ret > 0) ? (int)ret : -1;
     */
    (void)buf;
    (void)len;
    return (int)len;  /* placeholder */
}

/* ─── tcp_recv ───────────────────────────────────────────────────────── */
int tcp_recv(uint8_t *buf, uint16_t len)
{
    /*
     * uint16_t avail = getSn_RX_RSR(TCP_SOCKET);
     * if (avail == 0) return 0;
     * if (avail > len) avail = len;
     * int32_t ret = recv(TCP_SOCKET, buf, avail);
     * if (ret == SOCK_BUSY) return 0;
     * if (ret <= 0)         return -1;
     * return (int)ret;
     */
    (void)buf;
    (void)len;
    return 0;  /* placeholder */
}

/* ─── tcp_is_connected ───────────────────────────────────────────────── */
uint8_t tcp_is_connected(void)
{
    /*
     * return (getSn_SR(TCP_SOCKET) == SOCK_ESTABLISHED) ? 1 : 0;
     */
    return 0;  /* placeholder */
}
