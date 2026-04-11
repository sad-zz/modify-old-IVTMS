// DHT11 Temperature and Humidity Sensor Library
// Data pin: RF2 (formerly DS1305 SPI SDI1 - now repurposed for DHT11)
// Hardware: connect DHT11 DATA pin to RF2 with a 4.7kOhm pull-up to VCC (3.3V)
// Bytes: Hum_int | Hum_dec | Temp_int | Temp_dec | Checksum

void dht11_read()
{
    short i, t, ok, bit_val;
    short b[5];

    ok = 1;
    b[0] = 0;
    b[1] = 0;
    b[2] = 0;
    b[3] = 0;
    b[4] = 0;

    // Disable interrupts for timing accuracy during 1-wire read
    T4IE_bit   = 0;
    U1RXIE_bit = 0;
    U2RXIE_bit = 0;

    // Send start signal: pull RF2 low for >=18ms, then release
    TRISF.RF2 = 0;          // Set RF2 as output
    LATF2_bit = 0;          // Pull low
    delay_ms(20);
    LATF2_bit = 1;          // Pull high
    delay_us(30);
    TRISF.RF2 = 1;          // Set RF2 as input (release bus)

    // Wait for DHT11 response low (~80us)
    t = 0;
    while(!RF2_bit && t < 150) { delay_us(1); t++; }
    if(t >= 150) ok = 0;

    // Wait for DHT11 response high (~80us)
    if(ok)
    {
        t = 0;
        while(RF2_bit && t < 150) { delay_us(1); t++; }
        if(t >= 150) ok = 0;
    }

    // Read 40 bits (5 bytes): humidity int, humidity dec, temp int, temp dec, checksum
    for(i = 0; i < 40 && ok; i++)
    {
        // Each bit starts with a low pulse (~50us); wait for it to go high
        t = 0;
        while(!RF2_bit && t < 100) { delay_us(1); t++; }
        if(t >= 100) { ok = 0; break; }

        // Sample at 40us: '0' = high for 26-28us, '1' = high for 70us
        delay_us(40);
        bit_val = RF2_bit;
        b[i / 8] = (b[i / 8] << 1) | bit_val;

        // Wait for bit high pulse to end before next bit
        t = 0;
        while(RF2_bit && t < 100) { delay_us(1); t++; }
    }

    // Verify checksum and store results
    if(ok && ((b[0] + b[1] + b[2] + b[3]) & 0xFF) == b[4])
    {
        dht_hum  = (char)b[0];
        dht_temp = (char)b[2];
        dht_valid = 1;
    }

    // Re-enable interrupts
    T4IE_bit   = 1;
    U1RXIE_bit = 1;
    U2RXIE_bit = 1;
}
