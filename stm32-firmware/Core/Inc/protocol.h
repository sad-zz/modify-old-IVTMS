/**
 * @file protocol.h
 * @brief RATCX1 TCP protocol handler
 *
 * پروتکل TCP دستگاه RATCX1 - ارتباط با سرور TC Manager
 *
 * Message flow:
 *   Device → Server:  8000 + datetime(21) + sysId(8) + model + version + "READY"
 *   Server → Device:  0012 + "YYYY.MM.DD-HH:MM:SS.0"  (time sync)
 *   Device → Server:  8012 + datetime(21) + sysId(8)   (ACK)
 *   Server → Device:  0197 + "YYMMDDHHmm"             (request interval)
 *   Device → Server:  8821 + datetime(21) + intervalData(262) + CR + LF
 *
 * This module replaces the SIM900/GPRS state machine in the original firmware.
 * TCP connection is handled by the Air780 4G LTE module (see air780_tcp.h).
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

/** Initialise the protocol layer (call once after air780_init) */
void protocol_init(void);

/** Main protocol task – call from main loop as fast as possible.
 *  Manages connection, handshake, time sync and data sending. */
void protocol_task(void);

/** Force re-connection (e.g. after connection_state lost) */
void protocol_reconnect(void);

/** Returns 1 if TCP connection is established and handshake done */
uint8_t protocol_is_connected(void);

#endif /* PROTOCOL_H */
