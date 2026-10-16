//*******************************************************
// Copyright (c) MLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.de.html
// OlliW @ www.olliw.eu
//*******************************************************
// LEDs
//*******************************************************
#ifndef LEDS_H
#define LEDS_H
#pragma once


#include "hal/hal.h"


#ifdef DEVICE_HAS_NO_LED

class tLEDs
{
  public:
    void Init(void) {}
    void Tick_ms(bool connected) {}
    void SetToBind(void) {}
    void SetToParamStore(void) {}
    void InitPassthrough(void) {}
    void TickPassthrough_ms(void) {}
};

#else


// TetherLRS: one rule on every board, Tx and Rx alike. Slow blink = looking for the other radio, steady = connected,
// fast blink = binding. Colour LEDs are green in normal use and blue while binding; red is left to the fault codes.
class tLEDs
{
  public:
    void Init(void)
    {
        blink = 0;
        is_in_bind = false;
        on = false;
        shown = 0xFF;

        for (uint8_t i = 0; i < 7; i++) { on = !on; show(); delay_ms(50); }
    }

    void Tick_ms(bool connected)
    {
        DECc(blink, SYSTICK_DELAY_MS(is_in_bind ? 100 : 500));

        if (connected && !is_in_bind) {
            on = true;
        } else if (!blink) {
            on = !on;
        }
        show();
    }

    void SetToBind(void)
    {
        is_in_bind = true;
        on = true;
        blink = 0;
        show();
    }

    void SetToParamStore(void)
    {
        on = true;
        show();
    }

    void InitPassthrough(void)
    {
        led_red_off();
#ifndef DEVICE_HAS_SINGLE_LED
        led_green_on();
#endif
    }

    void TickPassthrough_ms(void)
    {
        DECc(blink, SYSTICK_DELAY_MS(100));

#ifdef DEVICE_HAS_SINGLE_LED
        if (!blink) { led_red_toggle(); }
#elif defined DEVICE_HAS_SINGLE_LED_RGB
        if (!blink) { led_purple_toggle(); }
#else
        if (!blink) { led_green_toggle(); led_red_toggle(); }
#endif
    }

  private:
    uint16_t blink;
    bool is_in_bind;
    bool on;
    uint8_t shown; // what the LED shows now, so an RGB LED is written only on a change

    void show(void)
    {
        uint8_t want = (on ? 1 : 0) | (is_in_bind ? 2 : 0);
        if (want == shown) return;
        shown = want;
#ifdef DEVICE_HAS_SINGLE_LED
        if (on) { led_red_on(); } else { led_red_off(); }
#elif defined DEVICE_HAS_SINGLE_LED_RGB
        if (!on) { led_green_off(); } else if (is_in_bind) { led_blue_on(); } else { led_green_on(); }
#else
        led_red_off();
        if (on) { led_green_on(); } else { led_green_off(); }
#endif
    }
};


#endif // DEVICE_HAS_NO_LED

#endif // LEDS_H
