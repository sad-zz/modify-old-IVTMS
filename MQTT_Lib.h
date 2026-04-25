// MQTT_Lib.h
// Lightweight MQTT v3.1.1 client for dsPIC30F4013 + SIM900 (GPRS).
//
// Implements a "publish burst" approach:
//   1. Briefly bring up a new TCP connection to the configured MQTT broker.
//   2. Send MQTT CONNECT + SUBSCRIBE to the cmd topic.
//   3. PUBLISH temp, humidity, vbat, error.
//   4. Wait a few seconds for an incoming PUBLISH on the cmd topic.
//   5. Process any received command (same handler as SMS commands).
//   6. Send MQTT DISCONNECT and CIPSHUT.
//
// Called once per interval period from gprs_state==0, before the main
// GPRS reconnect to the custom server.  The caller's CIPSHUT runs
// immediately afterwards so the existing protocol is unaffected.
//
// EEPROM layout (new, 4-byte aligned):
//   0x7FFCD8 : mqtt_ip1
//   0x7FFCDC : mqtt_ip2
//   0x7FFCE0 : mqtt_ip3
//   0x7FFCE4 : mqtt_ip4
//   0x7FFCE8 : mqtt_port
//   0x7FFCEC : mqtt_en  (0 = disabled, 1 = enabled)
//
// MQTT packet type constants
#define MQTT_CTRL_CONNECT    0x10
#define MQTT_CTRL_PUBLISH    0x30
#define MQTT_CTRL_SUBSCRIBE  0x82
#define MQTT_CTRL_DISCONNECT 0xE0

// -----------------------------------------------------------------------
// Internal helpers
// -----------------------------------------------------------------------

// Build topic string in mqtt_topic[]: "ivtms/<system_id>/<sub>"
void mqtt_build_topic(const char *sub)
{
    short i, j;
    i = 0;
    mqtt_topic[i++] = 'i';
    mqtt_topic[i++] = 'v';
    mqtt_topic[i++] = 't';
    mqtt_topic[i++] = 'm';
    mqtt_topic[i++] = 's';
    mqtt_topic[i++] = '/';
    for(j = 0; system_id[j] && i < 21; j++)
        mqtt_topic[i++] = system_id[j];
    mqtt_topic[i++] = '/';
    for(j = 0; sub[j] && i < 23; j++)
        mqtt_topic[i++] = sub[j];
    mqtt_topic[i] = 0;
}

// Wait for the '>' prompt that SIM900 sends after AT+CIPSEND=N.
// Returns 1 on success, 0 on timeout (~3 s).
short mqtt_wait_prompt()
{
    short t;
    sending_ready = 0;
    for(t = 0; t < 300 && !sending_ready; t++)
    {
        delay_ms(10);
        Clrwdt();
    }
    return sending_ready;
}

// Wait up to n*100 ms for uart2_data_received to be set.
// Returns 1 if received, 0 on timeout.
short mqtt_wait_rx(short n)
{
    short t;
    for(t = 0; t < n && !uart2_data_received; t++)
    {
        delay_ms(100);
        Clrwdt();
    }
    return uart2_data_received;
}

// Build MQTT CONNECT packet into mqtt_buf[].
// client_id: null-terminated string (max 23 chars).
// keepalive: seconds.
// Returns total packet length.
short mqtt_build_connect(const char *client_id, unsigned short keepalive)
{
    short id_len, rem_len, i, p;
    id_len  = strlen(client_id);
    rem_len = 10 + 2 + id_len;   // fixed var-header (10) + len field (2) + id

    p = 0;
    mqtt_buf[p++] = (char)MQTT_CTRL_CONNECT;
    mqtt_buf[p++] = (char)rem_len;           // remaining length (always < 128 here)
    // Variable header
    mqtt_buf[p++] = 0x00; mqtt_buf[p++] = 0x04;   // protocol name length = 4
    mqtt_buf[p++] = 'M';  mqtt_buf[p++] = 'Q';
    mqtt_buf[p++] = 'T';  mqtt_buf[p++] = 'T';
    mqtt_buf[p++] = 0x04;                          // protocol level: MQTT 3.1.1
    mqtt_buf[p++] = 0x02;                          // connect flags: Clean Session
    mqtt_buf[p++] = (char)(keepalive >> 8);
    mqtt_buf[p++] = (char)(keepalive & 0xFF);
    // Payload: client ID
    mqtt_buf[p++] = 0x00;
    mqtt_buf[p++] = (char)id_len;
    for(i = 0; i < id_len; i++) mqtt_buf[p++] = client_id[i];

    return p;
}

// Build MQTT PUBLISH (QoS 0) packet into mqtt_buf[].
// topic and payload are null-terminated strings.
// Returns total packet length, or 0 if the packet would exceed mqtt_buf[].
short mqtt_build_publish(const char *topic, const char *payload)
{
    short t_len, p_len, rem_len, i, p;
    t_len   = strlen(topic);
    p_len   = strlen(payload);
    rem_len = 2 + t_len + p_len;  // topic-len field (2) + topic + payload
    // Guard: 2-byte fixed header + rem_len must fit in mqtt_buf[]
    if (2 + rem_len > 95) return 0;

    p = 0;
    mqtt_buf[p++] = (char)MQTT_CTRL_PUBLISH;
    mqtt_buf[p++] = (char)rem_len;
    mqtt_buf[p++] = (char)(t_len >> 8);
    mqtt_buf[p++] = (char)(t_len & 0xFF);
    for(i = 0; i < t_len; i++)  mqtt_buf[p++] = topic[i];
    for(i = 0; i < p_len; i++)  mqtt_buf[p++] = payload[i];

    return p;
}

// Build MQTT SUBSCRIBE packet into mqtt_buf[] (single topic, QoS 0).
// packet_id: nonzero unsigned short.
// Returns total packet length, or 0 if the packet would exceed mqtt_buf[].
short mqtt_build_subscribe(const char *topic, unsigned short packet_id)
{
    short t_len, rem_len, i, p;
    t_len   = strlen(topic);
    rem_len = 2 + 2 + t_len + 1;  // pkt-id (2) + topic-len (2) + topic + QoS (1)
    // Guard: 2-byte fixed header + rem_len must fit in mqtt_buf[]
    if (2 + rem_len > 95) return 0;

    p = 0;
    mqtt_buf[p++] = (char)MQTT_CTRL_SUBSCRIBE;
    mqtt_buf[p++] = (char)rem_len;
    mqtt_buf[p++] = (char)(packet_id >> 8);
    mqtt_buf[p++] = (char)(packet_id & 0xFF);
    mqtt_buf[p++] = (char)(t_len >> 8);
    mqtt_buf[p++] = (char)(t_len & 0xFF);
    for(i = 0; i < t_len; i++) mqtt_buf[p++] = topic[i];
    mqtt_buf[p++] = 0x00;  // QoS 0

    return p;
}

// Send mqtt_buf[0..len-1] via SIM900 AT+CIPSEND.
// Formats: AT+CIPSEND=N\r, waits for '>' prompt, sends bytes,
// then waits up to 5 s for SEND OK from SIM900.
// Returns 1 on success, 0 on failure.
short mqtt_send_buf(short len)
{
    short i;

    clear_uart2();
    UART2_Write('A');
    delay_ms(10);
    send_atc(gsm_cipsend);         // "T+CIPSEND="
    inttostr(len, tmp7);
    send_atc(tmp7);
    UART2_Write(13);               // \r

    if(!mqtt_wait_prompt()) return 0;

    // Send binary packet bytes
    for(i = 0; i < len; i++)
    {
        UART2_Write(mqtt_buf[i]);
        delay_us(15);
    }

    // Wait for "SEND OK" (up to 5 s)
    uart2_data_received = 0;
    mqtt_wait_rx(50);
    return 1;
}

// -----------------------------------------------------------------------
// Main publish burst function
// -----------------------------------------------------------------------
// Called once per interval period.  Performs a complete MQTT cycle:
//   CIPSHUT → GPRS up → TCP to broker → CONNECT → SUBSCRIBE → PUBLISH×4
//   → wait for incoming cmd → DISCONNECT → CIPSHUT.
// Uses global variables: mqtt_ip*, mqtt_port, mqtt_en, system_id,
//   dht_temp, dht_hum, dht_valid, vbat, error_byte, apndata, sms_body,
//   sms_body_ptr, sms_cmd_received.
void mqtt_publish_burst()
{
    short pkt_len, t, ci, connected, found;
    char  cid[18];       // "IVTMS" + system_id (13 chars max)
    char  payload[12];   // sensor value as decimal string

    if(!mqtt_en) return;
    // Require a valid (non-zero) broker IP
    if(mqtt_ip1 == 0 && mqtt_ip2 == 0 && mqtt_ip3 == 0 && mqtt_ip4 == 0) return;

    // Build client ID: "IVTMS" + system_id (up to 13 chars)
    cid[0] = 'I'; cid[1] = 'V'; cid[2] = 'T'; cid[3] = 'M'; cid[4] = 'S';
    for(t = 0; t < 13 && system_id[t]; t++) cid[5 + t] = system_id[t];
    cid[5 + t] = 0;

    // -----------------------------------------------------------------
    // Step 1: CIPSHUT — drop any existing TCP connection
    // -----------------------------------------------------------------
    clear_uart2();
    UART2_Write('A');
    delay_ms(10);
    send_atc(gsm_cipshut);   // "T+CIPSHUT"
    UART2_Write(13);
    uart2_data_received = 0;
    mqtt_wait_rx(30);        // up to 3 s
    Clrwdt();

    // -----------------------------------------------------------------
    // Step 2: AT+CSTT — set APN
    // -----------------------------------------------------------------
    clear_uart2();
    send_atc(gsm_cstt[apndata]);   // "AT+CSTT=..."
    UART2_Write(13);
    uart2_data_received = 0;
    mqtt_wait_rx(20);              // up to 2 s
    if(!(uart2_data_pointer > 1 &&
         uart2_data[uart2_data_pointer - 2] == 'O' &&
         uart2_data[uart2_data_pointer - 1] == 'K'))
        goto mqtt_abort;
    Clrwdt();

    // -----------------------------------------------------------------
    // Step 3: AT+CIICR — bring up GPRS
    // -----------------------------------------------------------------
    clear_uart2();
    send_atc(gsm_ciicr);    // "AT+CIICR"
    UART2_Write(13);
    uart2_data_received = 0;
    mqtt_wait_rx(150);      // up to 15 s
    if(!(uart2_data_pointer > 1 &&
         uart2_data[uart2_data_pointer - 2] == 'O' &&
         uart2_data[uart2_data_pointer - 1] == 'K'))
        goto mqtt_abort;
    Clrwdt();

    // -----------------------------------------------------------------
    // Step 4: AT+CIFSR — get local IP (verifies GPRS is up)
    // -----------------------------------------------------------------
    clear_uart2();
    send_atc(gsm_cifsr);    // "AT+CIFSR"
    UART2_Write(13);
    uart2_data_received = 0;
    mqtt_wait_rx(20);
    if(uart2_data_pointer < 7) goto mqtt_abort;   // need at least x.x.x.x
    Clrwdt();

    // -----------------------------------------------------------------
    // Step 5: AT+CIPSTART — open TCP connection to MQTT broker
    // -----------------------------------------------------------------
    clear_uart2();
    send_atc(gsm_cipstart);         // "AT+CIPSTART=\"TCP\",\""
    inttostr(mqtt_ip1, tmp7); send_atc(tmp7);
    UART2_Write('.'); delay_us(5);
    inttostr(mqtt_ip2, tmp7); send_atc(tmp7);
    UART2_Write('.'); delay_us(5);
    inttostr(mqtt_ip3, tmp7); send_atc(tmp7);
    UART2_Write('.'); delay_us(5);
    inttostr(mqtt_ip4, tmp7); send_atc(tmp7);
    send_atc("\",");
    inttostr(mqtt_port, tmp7); send_atc(tmp7);
    UART2_Write(13);

    // Wait up to 10 s for "CONNECT OK" pattern
    // SIM900 sends: "\r\nOK\r\n\r\nCONNECT OK\r\n"
    // All bytes accumulate in uart2_data (pointer never resets on CR).
    // Pattern: uart2_data[pointer-4]='T', [pointer-2]='O', [pointer-1]='K'
    connected = 0;
    for(t = 0; t < 100 && !connected; t++)
    {
        delay_ms(100);
        Clrwdt();
        if(uart2_data_pointer > 3 &&
           uart2_data[uart2_data_pointer - 4] == 'T' &&
           uart2_data[uart2_data_pointer - 2] == 'O' &&
           uart2_data[uart2_data_pointer - 1] == 'K')
            connected = 1;
    }
    if(!connected) goto mqtt_abort;

    // -----------------------------------------------------------------
    // Step 6: MQTT CONNECT
    // -----------------------------------------------------------------
    pkt_len = mqtt_build_connect(cid, 60);
    if(!mqtt_send_buf(pkt_len)) goto mqtt_abort;

    // Wait up to 5 s for CONNACK: byte sequence 0x20 0x02 ?? 0x00
    // SIM900 delivers received TCP data as "+IPD,N:<data>" without a
    // trailing CR, so we poll uart2_data[] content directly.
    found = 0;
    for(t = 0; t < 50 && !found; t++)
    {
        delay_ms(100);
        Clrwdt();
        for(ci = 0; ci + 3 < uart2_data_pointer && !found; ci++)
        {
            if((unsigned char)uart2_data[ci]     == 0x20 &&
               (unsigned char)uart2_data[ci + 1] == 0x02 &&
               (unsigned char)uart2_data[ci + 3] == 0x00)   // return code 0 = accepted
                found = 1;
        }
    }
    if(!found) goto mqtt_abort;

    // -----------------------------------------------------------------
    // Step 7: MQTT SUBSCRIBE to cmd topic (QoS 0, packet ID = 1)
    // -----------------------------------------------------------------
    mqtt_build_topic("cmd");
    pkt_len = mqtt_build_subscribe(mqtt_topic, 1);
    if(!mqtt_send_buf(pkt_len)) goto mqtt_abort;

    // Brief wait for SUBACK (2 s) — not strictly required for QoS 0 publish
    uart2_data_received = 0;
    mqtt_wait_rx(20);
    Clrwdt();

    // -----------------------------------------------------------------
    // Step 8: PUBLISH temperature (if DHT11 read is valid)
    // -----------------------------------------------------------------
    if(dht_valid)
    {
        mqtt_build_topic("temp");
        bytetostr((unsigned short)dht_temp, payload);
        pkt_len = mqtt_build_publish(mqtt_topic, payload);
        mqtt_send_buf(pkt_len);
        Clrwdt();

        // -----------------------------------------------------------------
        // Step 9: PUBLISH humidity
        // -----------------------------------------------------------------
        mqtt_build_topic("hum");
        bytetostr((unsigned short)dht_hum, payload);
        pkt_len = mqtt_build_publish(mqtt_topic, payload);
        mqtt_send_buf(pkt_len);
        Clrwdt();
    }

    // Step 10: PUBLISH battery voltage.
    // ADC raw value (0-1023) is scaled by the board's voltage divider:
    //   Vbat (0.1 V units) = vbat * 0.388509 + 7
    // (Divider: R_top / (R_top + R_bot) ≈ 0.388509; +7 offset for diode drop)
    mqtt_build_topic("vbat");
    longtostr((long)(vbat * 0.388509 + 7), payload);
    pkt_len = mqtt_build_publish(mqtt_topic, payload);
    mqtt_send_buf(pkt_len);
    Clrwdt();

    // -----------------------------------------------------------------
    // Step 11: PUBLISH error bitmask (decimal)
    // -----------------------------------------------------------------
    mqtt_build_topic("error");
    inttostr((int)error_byte, payload);
    pkt_len = mqtt_build_publish(mqtt_topic, payload);
    mqtt_send_buf(pkt_len);
    Clrwdt();

    // -----------------------------------------------------------------
    // Step 12: Wait 3 s for a PUBLISH from the broker on the cmd topic.
    //          SIM900 delivers it as "+IPD,N:\x30..." in uart2_data.
    //          Extract the payload and hand it to the SMS command handler.
    // -----------------------------------------------------------------
    clear_uart2();
    uart2_data_received = 0;
    for(t = 0; t < 30; t++)
    {
        delay_ms(100);
        Clrwdt();
        // Search for MQTT PUBLISH fixed-header byte (0x30 = QoS 0, no flags)
        for(ci = 0; ci + 4 < uart2_data_pointer; ci++)
        {
            if((unsigned char)uart2_data[ci] == (unsigned char)MQTT_CTRL_PUBLISH)
            {
                // Remaining length at [ci+1], topic length at [ci+2..ci+3]
                short rem_len   = (unsigned char)uart2_data[ci + 1];
                short t_len     = ((unsigned char)uart2_data[ci + 2] << 8) |
                                   (unsigned char)uart2_data[ci + 3];
                short pay_start = ci + 2 + 2 + t_len;
                short pay_len   = rem_len - 2 - t_len;
                short k;

                // Validate bounds before accessing payload
                if(t_len >= 0 && pay_len > 0 &&
                   pay_start >= 0 && pay_start + pay_len <= uart2_data_pointer)
                {
                    for(k = 0; k < pay_len && k < 49; k++)
                        sms_body[k] = uart2_data[pay_start + k];
                    sms_body[k] = 0;
                    sms_body_ptr   = k;
                    if(k >= 4) sms_cmd_received = 1;   // processed in main loop
                }
                t = 30;   // exit outer loop
                break;
            }
        }
    }

mqtt_abort:
    // -----------------------------------------------------------------
    // Step 13: MQTT DISCONNECT (2-byte packet: 0xE0 0x00)
    // -----------------------------------------------------------------
    mqtt_buf[0] = (char)MQTT_CTRL_DISCONNECT;
    mqtt_buf[1] = 0x00;
    clear_uart2();
    UART2_Write('A');
    delay_ms(10);
    send_atc(gsm_cipsend);   // "T+CIPSEND="
    send_atc("2");
    UART2_Write(13);
    if(mqtt_wait_prompt())
    {
        UART2_Write((char)MQTT_CTRL_DISCONNECT);
        delay_us(15);
        UART2_Write(0x00);
        uart2_data_received = 0;
        mqtt_wait_rx(10);
    }

    // -----------------------------------------------------------------
    // Step 14: CIPSHUT — clean up TCP connection
    // -----------------------------------------------------------------
    clear_uart2();
    UART2_Write('A');
    delay_ms(10);
    send_atc(gsm_cipshut);   // "T+CIPSHUT"
    UART2_Write(13);
    uart2_data_received = 0;
    mqtt_wait_rx(30);
    Clrwdt();
}
