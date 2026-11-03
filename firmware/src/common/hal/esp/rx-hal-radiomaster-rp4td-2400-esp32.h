//*******************************************************
// Copyright (c) MLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.de.html
//*******************************************************
// hal
//********************************************************

//-------------------------------------------------------
// ESP32, ELRS RadioMaster RP4TD 2400 RX
//-------------------------------------------------------

#include "rx-hal-generic-2400-td-pa-esp32.h"


#define DEVICE_HAS_SERIAL_OUT2


//-- UARTS
// UARTB = serial port
// UART = Ser Out2 port, a second TX for the link's data
// UARTF = debug port

#define UART_USE_SERIAL1 
#define UART_BAUD                 115200   // Rx Ser Baudrate2 sets it
#define UART_USE_TX_IO            IO_P18   // t pad on the receiver
#define UART_USE_RX_IO            -1       // no Rx pin needed
#define UART_TXBUFSIZE            256


