//*******************************************************
// Copyright (c) MLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.de.html
//*******************************************************
// TetherLRS: dynamic RF power
//********************************************************
// The configured power is the ceiling (the region caps it further). Each end steps its own power from what the other
// end reports about our frames: its RSSI over the receiver sensitivity of the mode, and its LQ. Both ends transmit
// data, so both do it. Up fast, down slow: a weak or lost link goes back to the ceiling at once, a weak-ish one climbs
// a step every 250 ms, a strong one comes down a step after 2 s of staying strong. The 15 dB between the up and down
// thresholds is wider than any step of the power tables (3 to 4 dB), so it settles instead of hunting.
#ifndef DYNAMIC_POWER_H
#define DYNAMIC_POWER_H
#pragma once

#include <stdint.h>


#define DYNPOWER_MARGIN_LOW_DB      10 // below: straight to the ceiling
#define DYNPOWER_MARGIN_UP_DB       20 // below: one step up
#define DYNPOWER_MARGIN_DOWN_DB     35 // above, held: one step down
#define DYNPOWER_LQ_LOW             70
#define DYNPOWER_LQ_UP              90
#define DYNPOWER_LQ_DOWN            95
#define DYNPOWER_UP_MS              250
#define DYNPOWER_DOWN_HOLD_MS       2000


class tDynPower
{
  public:
    void Init(void)
    {
        idx = UINT8_MAX;
        t_last_up_ms = 0;
        t_strong_ms = 0;
        strong = false;
    }

    // once per frame cycle; peer_rssi_dbm >= 0 means the other end did not hear us (mLRS reports 127 then)
    uint8_t Step(uint32_t tnow_ms, bool connected, int8_t peer_rssi_dbm, uint8_t peer_lq, int8_t sensitivity_dbm,
                 uint8_t ceiling)
    {
        if (idx > ceiling) idx = ceiling;

        int16_t margin_db = (int16_t)peer_rssi_dbm - sensitivity_dbm;

        if (!connected || peer_rssi_dbm >= 0 || margin_db < DYNPOWER_MARGIN_LOW_DB || peer_lq < DYNPOWER_LQ_LOW) {
            idx = ceiling;
            strong = false;
            return idx;
        }

        if (margin_db < DYNPOWER_MARGIN_UP_DB || peer_lq < DYNPOWER_LQ_UP) {
            strong = false;
            if (idx < ceiling && (uint32_t)(tnow_ms - t_last_up_ms) >= DYNPOWER_UP_MS) {
                idx++;
                t_last_up_ms = tnow_ms;
            }
            return idx;
        }

        if (margin_db > DYNPOWER_MARGIN_DOWN_DB && peer_lq >= DYNPOWER_LQ_DOWN) {
            if (!strong) {
                strong = true;
                t_strong_ms = tnow_ms;
            } else if ((uint32_t)(tnow_ms - t_strong_ms) >= DYNPOWER_DOWN_HOLD_MS) {
                if (idx > 0) idx--;
                t_strong_ms = tnow_ms; // the next step down waits for the other end to report the lower power
            }
        } else {
            strong = false;
        }

        return idx;
    }

  private:
    uint8_t idx;
    uint32_t t_last_up_ms;
    uint32_t t_strong_ms;
    bool strong;
};


#endif // DYNAMIC_POWER_H
