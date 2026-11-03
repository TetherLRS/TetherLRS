//*******************************************************
// Copyright (c) MLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.de.html
//*******************************************************
// hal
//********************************************************

/*
  Info on pin usage

  TX,RX (main pads on top&bottom):  serial
  TX2 (corner pad on top):          out (CRSF/SBUS)

  top:      side with button, LED, antenna, and PA chips
  bottom:   side with ESP chip
*/


//-------------------------------------------------------
// ESP32, ELRS RadioMaster XR4 RX
//-------------------------------------------------------

#include "rx-hal-generic-lr1121-td-esp32.h"


#define DEVICE_HAS_SERIAL_OUT2


//-- UARTS
// UARTB = serial port
// UART = Ser Out2 port, a second TX for the link's data
// UARTF = debug port

#define UART_USE_SERIAL1 
#define UART_BAUD                 115200   // Rx Ser Baudrate2 sets it
#define UART_USE_TX_IO            IO_P18   // tx2 pad on the receiver
#define UART_USE_RX_IO            -1       // no rx pin needed
#define UART_TXBUFSIZE            256


//-- LR11xx DIO switch control

#define SX_USE_RFSW_CTRL {15, 0, 12, 8, 8, 6, 0, 5}
