//*******************************************************
// Copyright (c) TetherLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.html
//*******************************************************
// Rx Ser Out2: a second TX carrying the link's data, for a device beside the one on the serial port
//*******************************************************
#ifndef RX_SERIAL_OUT2_H
#define RX_SERIAL_OUT2_H
#pragma once

#include "../common/setup_types.h"

#ifdef ESP32 // the GPIO matrix puts a UART TX on any pin, also the serial port's RX pin, which then reads nothing
#include "soc/gpio_sig_map.h"
#include "soc/gpio_pins.h"
#include "esp_rom_gpio.h"
#ifndef UARTB_USE_RX_IO
  #define UARTB_USE_RX_IO  3 // the ESP32's UART0 pin
#endif
#endif


class tRxSerialOut2
{
  public:
    void Init(uint8_t _mode, uint32_t baud)
    {
        mode = RX_SERIAL_OUT2_OFF;
#ifdef ESP32
        int8_t tx_pin = -1;
        if (_mode == RX_SERIAL_OUT2_TX_ON_RX_PIN) tx_pin = UARTB_USE_RX_IO;
#ifdef USE_SERIAL_OUT2
        if (_mode == RX_SERIAL_OUT2_SERIAL1) tx_pin = UART_USE_TX_IO;
#endif
        if (tx_pin < 0) return;
        Serial1.setTxBufferSize(512);
        Serial1.begin(baud, SERIAL_8N1, -1, tx_pin);
        if (_mode == RX_SERIAL_OUT2_TX_ON_RX_PIN) {
            // the pin drives now: UART0 would hear its own bytes, so its RX gets a steady idle instead
            esp_rom_gpio_connect_in_signal(GPIO_MATRIX_CONST_ONE_INPUT, U0RXD_IN_IDX, false);
        }
        mode = _mode;
#elif defined USE_SERIAL_OUT2 // STM32 with a second UART pad
        if (_mode != RX_SERIAL_OUT2_SERIAL1) return;
#ifdef SERIAL_OUT2_INIT_GPIO // an enable or an inverter control in front of the pad
        serial_out2_init_gpio();
#endif
        uart_init();
        uart_setbaudrate(baud);
        mode = _mode;
#else
        (void)_mode; (void)baud;
#endif
    }

    void putbuf(uint8_t* const buf, uint16_t len)
    {
        if (mode == RX_SERIAL_OUT2_OFF) return;
#ifdef ESP32
        Serial1.write(buf, len);
#elif defined USE_SERIAL_OUT2
        uart_putbuf(buf, len);
#else
        (void)buf; (void)len;
#endif
    }

  private:
    uint8_t mode;
};


#endif // RX_SERIAL_OUT2_H
