void send_atc(const char *s)
{
   while(*s) 
   {              // send command string
       if(*s!=10 && *s!=13 && *s!=' ')
       {
           if(debug_gsm) UART1_Write(*s);
           UART2_Write(*s);
           delay_us(15);
       }
       s++;
      //delay_us(10);
      
   }
}
void send_out(const char *s)
{
   while(*s)
   {              // send command string
       if(*s!=10 && *s!=13)
       {
           //#ifndef ZTCSLAVE
           UART1_Write(*s);
           //#endif
           //UART2_Write(*s);
           delay_us(15);
       }
       s++;
      //delay_us(10);

   }
}
void set_gprs_timer(unsigned short timer)
{
    gprs_timer=0;
    gprs_end_timer=timer+1;
    gprs_timer_en=1;
}
void sim900_restart()
{
    sim_reset=1;
}

// Send an SMS message to sms_number_1.
// Must be called only when UART2 is in command mode (connection_state==0).
void send_sms_reply(char *msg)
{
    short timeout;

    if(sms_number_1[0] == 0) return;    // No number configured

    sending_ready = 0;
    uart2_data_received = 0;
    clear_uart2();

    // Issue AT+CMGS command
    UART2_Write('A');
    delay_ms(10);
    send_atc("T+CMGS=\"");
    send_atc(sms_number_1);
    UART2_Write('"');
    UART2_Write(13);

    // Wait up to 5 seconds for the '>' prompt
    for(timeout = 0; timeout < 500 && !sending_ready; timeout++)
    {
        delay_ms(10);
        Clrwdt();
    }

    if(sending_ready)
    {
        send_atc(msg);
        delay_ms(10);
        UART2_Write(0x1A);  // Ctrl+Z: commit the message

        // Wait up to 8 seconds for OK/ERROR response
        for(timeout = 0; timeout < 800 && !uart2_data_received; timeout++)
        {
            delay_ms(10);
            Clrwdt();
        }
    }

    uart2_data_received = 0;
    clear_uart2();
}