//*******************************************************
// TetherLRS
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.de.html
//*******************************************************
// ESP32 display on SPI, behind the I2C calls gdisp makes
//*******************************************************
// DEVICE_HAS_SPI_DISPLAY: an SSD1306 128x64 OLED on SPI (ExpressLRS screen_type 2), the same commands and page
//   writes as on I2C, the D/C pin telling commands from data
// DEVICE_HAS_TFT_DISPLAY: an ST7735 80x160 TFT (screen_type 4), set up as ExpressLRS does (IPS, BGR, 26/1 offsets,
//   landscape); gdisp's 128x64 picture is drawn in its middle, the SSD1306 commands gdisp sends are dropped
// The display has the HSPI controller, the radio VSPI. As esp-i2c.h, frames go out from a Core 0 task.
// The hal defines DISP_SPI_SCK_IO, DISP_SPI_MOSI_IO, DISP_DC_IO, and DISP_CS_IO, DISP_RST_IO, DISP_BL_IO if wired.
#ifndef ESPLIB_DISPLAY_SPI_H
#define ESPLIB_DISPLAY_SPI_H

#include "esp-peripherals.h"
#include <SPI.h>

#ifdef __cplusplus
extern "C" { // gdisp.c calls the i2c_ functions
#endif


#ifdef DEVICE_HAS_TFT_DISPLAY
  #define DISP_SPI_HZ  20000000L // ST7735 up to 27 MHz, HSPI through the GPIO matrix to some 26
#else
  #define DISP_SPI_HZ  8000000L
#endif

SPIClass disp_spi(HSPI);

struct {
    const uint8_t* buf;
    uint16_t len;
    volatile bool busy;
    TaskHandle_t task_handle;
} disp_async;


static void disp_write(bool cmd, const uint8_t* buf, uint16_t len)
{
    disp_spi.beginTransaction(SPISettings(DISP_SPI_HZ, MSBFIRST, SPI_MODE0));
#ifdef DISP_CS_IO
    gpio_low(DISP_CS_IO);
#endif
    if (cmd) { gpio_low(DISP_DC_IO); } else { gpio_high(DISP_DC_IO); }
    disp_spi.writeBytes(buf, len);
#ifdef DISP_CS_IO
    gpio_high(DISP_CS_IO);
#endif
    disp_spi.endTransaction();
}


static void disp_cmd(uint8_t cmd, const uint8_t* data, uint16_t len)
{
    disp_write(true, &cmd, 1);
    if (len) disp_write(false, data, len);
}


//-------------------------------------------------------
// the I2C calls of gdisp
//-------------------------------------------------------

IRAM_ATTR void i2c_setdeviceadr(uint8_t dev_adr) {}


// reg_adr is gdisp's SSD1306_CMD (0x00) or SSD1306_DATA (0x40)
IRAM_ATTR HAL_StatusTypeDef i2c_put_blocked(uint8_t reg_adr, uint8_t* buf, uint16_t len)
{
#ifndef DEVICE_HAS_TFT_DISPLAY
    disp_write(reg_adr == 0x00, buf, len);
#endif
    return HAL_OK;
}


// async transfer by the Core 0 task, passes the pointer (no copy): the buffer must stay until done
IRAM_ATTR HAL_StatusTypeDef i2c_put(uint8_t reg_adr, uint8_t* buf, uint16_t len)
{
    if (disp_async.busy) return HAL_ERROR;

    disp_async.buf = buf;
    disp_async.len = len;
    disp_async.busy = true;
    xTaskNotifyGive(disp_async.task_handle);
    return HAL_OK;
}


IRAM_ATTR HAL_StatusTypeDef i2c_device_ready(void)
{
    return (disp_async.busy) ? HAL_BUSY : HAL_OK;
}


#ifdef DEVICE_HAS_TFT_DISPLAY

#define TFT_X0  (16 + 1) // the 128x64 picture in the middle of 160x80, plus ExpressLRS's offsets in landscape
#define TFT_Y0  (8 + 26)

static void tft_frame(const uint8_t* buf) // gdisp's buffer: 8 pages of 128 columns, a byte holds 8 rows
{
    uint8_t win[4];
    win[0] = 0; win[1] = TFT_X0; win[2] = 0; win[3] = TFT_X0 + 127;
    disp_cmd(0x2A, win, 4); // CASET
    win[1] = TFT_Y0; win[3] = TFT_Y0 + 63;
    disp_cmd(0x2B, win, 4); // RASET
    disp_cmd(0x2C, nullptr, 0); // RAMWR

    static uint8_t line[128 * 2]; // one row in RGB565, white on black
    for (uint8_t y = 0; y < 64; y++) {
        const uint8_t* page = buf + (y >> 3) * 128;
        uint8_t bit = 1 << (y & 7);
        for (uint8_t x = 0; x < 128; x++) {
            uint8_t c = (page[x] & bit) ? 0xFF : 0x00;
            line[2 * x] = c;
            line[2 * x + 1] = c;
        }
        disp_write(false, line, sizeof(line));
    }
}

#endif


// frame transfer on Core 0, so that Core 1 (radio) is not held up
void disp_task(void* param)
{
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

#ifdef DEVICE_HAS_TFT_DISPLAY
        tft_frame(disp_async.buf);
#else
        // page addressing, as esp-i2c.h does
        const uint8_t* buf = disp_async.buf;
        uint16_t remaining = disp_async.len;
        for (uint8_t page = 0; remaining > 0; page++) {
            uint16_t chunk = (remaining < 128) ? remaining : 128;
            uint8_t cmd[3] = { 0x00, 0x10, (uint8_t)(0xB0 + page) }; // column 0, page
            disp_write(true, cmd, 3);
            disp_write(false, buf, chunk);
            buf += chunk;
            remaining -= chunk;
        }
#endif

        disp_async.busy = false;
    }
}


//-------------------------------------------------------
// INIT routines
//-------------------------------------------------------

void i2c_init(void)
{
    gpio_init(DISP_DC_IO, IO_MODE_OUTPUT_PP_HIGH);
#ifdef DISP_CS_IO
    gpio_init(DISP_CS_IO, IO_MODE_OUTPUT_PP_HIGH);
#endif
    disp_spi.begin(DISP_SPI_SCK_IO, -1, DISP_SPI_MOSI_IO, -1);

#ifdef DISP_RST_IO
    gpio_init(DISP_RST_IO, IO_MODE_OUTPUT_PP_HIGH);
    delay(10);
    gpio_low(DISP_RST_IO);
    delay(50);
    gpio_high(DISP_RST_IO);
    delay(50);
#endif

#ifdef DEVICE_HAS_TFT_DISPLAY
  #ifndef DISP_RST_IO
    disp_cmd(0x01, nullptr, 0); // SWRESET
    delay(50);
  #endif
    // as Arduino_GFX's ST7735 for ExpressLRS: sleep out, 16 bit colour, gamma, normal mode, on, landscape (rotation 1,
    // BGR), inverted for the IPS panel
    disp_cmd(0x11, nullptr, 0); // SLPOUT
    delay(120);
    const uint8_t colmod = 0x05;
    disp_cmd(0x3A, &colmod, 1);
    static const uint8_t gamma_p[16] = { 0x09, 0x16, 0x09, 0x20, 0x21, 0x1B, 0x13, 0x19, 0x17, 0x15, 0x1E, 0x2B, 0x04, 0x05, 0x02, 0x0E };
    static const uint8_t gamma_n[16] = { 0x0B, 0x14, 0x08, 0x1E, 0x22, 0x1D, 0x18, 0x1E, 0x1B, 0x1A, 0x24, 0x2B, 0x06, 0x06, 0x02, 0x0F };
    disp_cmd(0xE0, gamma_p, 16);
    disp_cmd(0xE1, gamma_n, 16);
    delay(10);
    disp_cmd(0x13, nullptr, 0); // NORON
    delay(10);
    disp_cmd(0x29, nullptr, 0); // DISPON
    const uint8_t madctl = 0x80 | 0x20 | 0x08; // MY, MV, BGR
    disp_cmd(0x36, &madctl, 1);
    disp_cmd(0x21, nullptr, 0); // INVON

    // the whole panel black once, the frames cover only the middle
    uint8_t win[4] = { 0, 1, 0, 1 + 159 };
    disp_cmd(0x2A, win, 4);
    win[1] = 26; win[3] = 26 + 79;
    disp_cmd(0x2B, win, 4);
    disp_cmd(0x2C, nullptr, 0);
    static uint8_t black[160 * 2];
    memset(black, 0, sizeof(black));
    for (uint8_t y = 0; y < 80; y++) disp_write(false, black, sizeof(black));
#endif

#ifdef DISP_BL_IO
    gpio_init(DISP_BL_IO, IO_MODE_OUTPUT_PP_HIGH); // backlight on
#endif

    disp_async.busy = false;
    disp_async.task_handle = NULL;

    xTaskCreatePinnedToCore(disp_task, "DISP", 2048, NULL, 1, &disp_async.task_handle, 0); // Core 0, Priority 1
}


#ifdef __cplusplus
}
#endif
#endif // ESPLIB_DISPLAY_SPI_H
