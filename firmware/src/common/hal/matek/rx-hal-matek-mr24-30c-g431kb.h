//*******************************************************
// Copyright (c) MLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.de.html
//*******************************************************
// hal
//*******************************************************

//-------------------------------------------------------
// MATEKSYS mR24-30C STM32G431KB, as receiver
//-------------------------------------------------------

#define DEVICE_HAS_SERIAL_OUT2
#define DEVICE_HAS_FAN_ONOFF
#define DEVICE_HAS_DRONECAN_FD

#include "hal-matek-mr-g431kb-common.h"

// change these defaults
#undef SETUP_RX_SERIAL_PORT
#define SETUP_RX_SERIAL_PORT  1 // 0: serial, 1: can, 2: canfd


//-- Timers, Timing, EEPROM, and such stuff

#define CLOCK_TIMx                TIM2
#define CLOCK_IRQn                TIM2_IRQn
#define CLOCK_IRQHandler          TIM2_IRQHandler
//#define CLOCK_IRQ_PRIORITY        10


//-- UARTS
// UARTB = serial port
// UART = Ser Out2 port, a second TX for the link's data
// UARTF = debug port

#define UARTB_USE_UART1_PA9PA10 // serial
#define UARTB_BAUD                RX_SERIAL_BAUDRATE
#define UARTB_USE_TX
#define UARTB_TXBUFSIZE           RX_SERIAL_TXBUFSIZE // 1024 // 512
#define UARTB_USE_TX_ISR
#define UARTB_USE_RX
#define UARTB_RXBUFSIZE           RX_SERIAL_RXBUFSIZE // 1024 // 512

#define UART_USE_UART2_PB3PB4 // out pin
#define UART_BAUD                 115200 // Rx Ser Baudrate2 sets it
#define UART_USE_TX
#define UART_TXBUFSIZE            256 // 512
#define UART_USE_TX_ISR
//#define UART_USE_RX
//#define UART_RXBUFSIZE            512

#define UARTF_USE_LPUART1_PA2PA3 // debug
#define UARTF_BAUD                115200
#define UARTF_USE_TX
#define UARTF_TXBUFSIZE           1024 // for CAN debug
#define UARTF_USE_TX_ISR
//#define UARTF_USE_RX
//#define UARTF_RXBUFSIZE           512


//-- SX1: SX12xx & SPI

#define SX_USE_REGULATOR_MODE_DCDC


//-- Button


//-- LEDs


//-- Cooling Fan


//-- POWER

#define POWER_PA_MATEK_MR24_30
#include "../hal-power-pa.h"

