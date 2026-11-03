//*******************************************************
// Copyright (c) TetherLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.html
//*******************************************************
// ESP32-C3 and S3 native USB CDC as a port (HWCDC, built with ARDUINO_USB_MODE=1 and no CDC on boot)
//*******************************************************
#ifndef ESPLIB_USB_H
#define ESPLIB_USB_H


void usb_init(void)
{
    USBSerial.setTxTimeoutMs(0); // no host, or one not reading: the bytes are dropped, the link never waits
    USBSerial.begin();
}


IRAM_ATTR void usb_putbuf(uint8_t* const buf, uint16_t len)
{
    USBSerial.write(buf, len);
}


IRAM_ATTR uint8_t usb_tx_full(void)
{
    return (USBSerial.availableForWrite() == 0) ? 1 : 0;
}


IRAM_ATTR uint8_t usb_rx_available(void)
{
    return (USBSerial.available() > 0) ? 1 : 0;
}


IRAM_ATTR uint16_t usb_rx_bytesavailable(void)
{
    int available = USBSerial.available();
    return (available > 0) ? available : 0;
}


IRAM_ATTR char usb_getc(void)
{
    return (char)USBSerial.read();
}


IRAM_ATTR void usb_flush(void)
{
    while (USBSerial.available() > 0) USBSerial.read();
}


#endif // ESPLIB_USB_H
