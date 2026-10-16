//*******************************************************
// Copyright (c) MLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.de.html
//*******************************************************
// hal
//********************************************************

//-------------------------------------------------------
// ESP32C3, DAKEFPV 900MHz MAX 1000mW Diversity RX ("915 Pro") as TX, LR1121
//-------------------------------------------------------
// The receiver used as a ground radio for the data link, SiK-style: serial port only, no JR bay and no RC input.
// Pins and RF overlay as in rx-hal-dakefpv-900-max-1w-esp32c3.h.

#define DEVICE_HAS_SINGLE_LED_RGB
#define DEVICE_HAS_SERIAL_ESCAPE // the board has one UART on its pads: serial, and com (CLI) after SiK's +++
#define DEVICE_HAS_NO_DEBUG


//-- UARTS
// UARTB = serial port
// UARTC (or USB) = com (CLI) port
// UARTD = serial2 port or wireless bridge port
// UART  = JR bay pin5
// UARTE = in port, SBus or whatever
// UARTF or SWUART = debug port

#define UARTB_USE_SERIAL // serial or com, the RX/TX pads
#define UARTB_BAUD                TX_SERIAL_BAUDRATE
#define UARTB_USE_TX_IO           IO_P21
#define UARTB_USE_RX_IO           IO_P20
#define UARTB_TXBUFSIZE           TX_SERIAL_TXBUFSIZE
#define UARTB_RXBUFSIZE           TX_SERIAL_RXBUFSIZE


//-- SX1: LR11xx & SPI

#define SPI_CS_IO                 IO_P7
#define SPI_MISO                  IO_P5
#define SPI_MOSI                  IO_P4
#define SPI_SCK                   IO_P6
#define SPI_FREQUENCY             16000000L  // 16 MHz max per datasheet
#define SX_RESET                  IO_P2
#define SX_DIO                    IO_P1
#define SX_BUSY                   IO_P3

#define SX_USE_RFSW_CTRL {31, 0, 4, 24, 24, 0, 0, 0}

#define SX_USE_REGULATOR_MODE_DCDC

// Antenna switch, driven by Common/antenna_switch.h (ELRS's single-radio diversity); antenna 0 is low, as in ELRS
#define DEVICE_HAS_ANTENNA_SWITCH
#define SX_ANT_CTRL               IO_P10

IRQHANDLER(void SX_DIO_EXTI_IRQHandler(void);)

void sx_init_gpio(void)
{
    gpio_init(SX_RESET, IO_MODE_OUTPUT_PP_LOW);
    gpio_init(SX_DIO, IO_MODE_INPUT_ANALOG);
    gpio_init(SX_BUSY, IO_MODE_INPUT_ANALOG);
    gpio_init(SX_ANT_CTRL, IO_MODE_OUTPUT_PP_LOW);
}

IRAM_ATTR bool sx_busy_read(void) { return (gpio_read_activehigh(SX_BUSY)) ? true : false; }

IRAM_ATTR void sx_antenna_switch_set(uint8_t antenna) { if (antenna) { gpio_high(SX_ANT_CTRL); } else { gpio_low(SX_ANT_CTRL); } }

IRAM_ATTR void sx_amp_transmit(void) {}
IRAM_ATTR void sx_amp_receive(void) {}

void sx_dio_init_exti_isroff(void) { detachInterrupt(SX_DIO); }
void sx_dio_enable_exti_isr(void) { attachInterrupt(SX_DIO, SX_DIO_EXTI_IRQHandler, RISING); }
IRAM_ATTR void sx_dio_exti_isr_clearflag(void) {}


//-- Button

#define BUTTON                    IO_P9

void button_init(void)
{
    gpio_init(BUTTON, IO_MODE_INPUT_PU);
}

IRAM_ATTR bool button_pressed(void)
{
    return gpio_read_activelow(BUTTON) ? true : false;
}


//-- LEDs

#define LED_RGB                   IO_P8
#define LED_RGB_PIXEL_NUM         1
#include "esp-hal-led-rgb.h"


//-- POWER

#include "../../setup_types.h" // needed for frequency band condition in rfpower calc

#define SX_USE_LP_PA             // the 900 MHz PA is fed from the LP PA output (ELRS radio_rfo_hf)

// sx_power from ELRS's power_values; the board has no 2.4 GHz front end, so the band does not matter
void lr11xx_rfpower_calc(const int8_t power_dbm, int8_t* sx_power, int8_t* actual_power_dbm, const uint8_t frequency_band)
{
    if (power_dbm >= POWER_30_DBM) {
        *sx_power = 2;
        *actual_power_dbm = 30;
    } else if (power_dbm >= POWER_27_DBM) {
        *sx_power = -2;
        *actual_power_dbm = 27;
    } else if (power_dbm >= POWER_24_DBM) {
        *sx_power = -6;
        *actual_power_dbm = 24;
    } else if (power_dbm >= POWER_20_DBM) {
        *sx_power = -10;
        *actual_power_dbm = 20;
    } else if (power_dbm >= POWER_17_DBM) {
        *sx_power = -13;
        *actual_power_dbm = 17;
    } else {
        *sx_power = -16;
        *actual_power_dbm = 14;
    }
}

#define RFPOWER_DEFAULT           0 // index into rfpower_list array

// 1 W left out until the board is confirmed to be the max-1w and not the 500 mW pro, which shares its RF overlay
const rfpower_t rfpower_list[] = {
    { .dbm = POWER_14_DBM, .mW = 25 },
    { .dbm = POWER_17_DBM, .mW = 50 },
    { .dbm = POWER_20_DBM, .mW = 100 },
    { .dbm = POWER_24_DBM, .mW = 250 },
    { .dbm = POWER_27_DBM, .mW = 500 },
};
