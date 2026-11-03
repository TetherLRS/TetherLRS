//*******************************************************
// Copyright (c) TetherLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.html
//*******************************************************
// Host test of the Mode = auto ladder (src/common/auto_ladder.h): the dB arithmetic, the bands' path loss, which
// (band, mode) steps the ladder keeps, the next step up and down, the base mode. Run with test/run_tests.sh.
#include <cstdio>
#include <cstdint>
#include "setup_types.h"
#include "auto_ladder.h"

static int fails = 0;
static void check(const char* name, bool ok)
{
    printf("%-76s %s\n", name, ok ? "ok" : "FAIL");
    if (!ok) fails++;
}

// a ladder from one or two bands' modes, each mode at its power, as mode_auto_ladder() in tx_main.cpp builds it
struct tBand { uint8_t band; uint16_t modes; int8_t power_dbm; int8_t lr_power_dbm; };

static uint8_t build(const tBand* bands, uint8_t nb, uint8_t band_low, tAutoStep* steps)
{
    tAutoStep all[2 * MODE_AUTO];
    uint8_t n = 0;
    for (uint8_t k = 0; k < nb; k++) {
        for (uint8_t m = 0; m < MODE_AUTO; m++) {
            if (!(bands[k].modes & (1 << m))) continue;
            int8_t p = (m == MODE_LR) ? bands[k].lr_power_dbm : bands[k].power_dbm;
            tAutoStep st = { bands[k].band, m, mode_rate_hz(m), (int16_t)(p - mode_sensitivity_dbm(m) - band_offset_db(bands[k].band, band_low)) };
            auto_ladder_add(all, &n, st);
        }
    }
    return auto_ladder_front(all, n, steps);
}

static bool is(const tAutoStep& s, uint8_t band, uint8_t mode) { return s.band == band && s.mode == mode; }

#define B(m) (1 << (m))
#define B2P4 SETUP_FREQUENCY_BAND_2P4_GHZ
#define B868 SETUP_FREQUENCY_BAND_868_MHZ

int main(void)
{
    check("10 log10 to the nearest dB: 1 mW is 0 dBm", db_from_x100(100) == 0);
    check("  10 mW 10 dBm, 25 mW 14 dBm, 100 mW 20 dBm", db_from_x100(1000) == 10 && db_from_x100(2500) == 14 && db_from_x100(10000) == 20);
    check("  500 mW 27 dBm, 1 W 30 dBm, 2 W 33 dBm", db_from_x100(50000) == 27 && db_from_x100(100000) == 30 && db_from_x100(200000) == 33);
    check("2.4 GHz loses 9 dB over 868 MHz, 9 over 915", band_offset_db(B2P4, B868) == 9 && band_offset_db(B2P4, SETUP_FREQUENCY_BAND_915_MHZ_FCC) == 9);
    check("  868 MHz 6 over 433 MHz; the lower band loses nothing", band_offset_db(B868, SETUP_FREQUENCY_BAND_433_MHZ) == 6 && band_offset_db(B868, B2P4) == 0 && band_offset_db(B868, B868) == 0);

    tAutoStep st[2 * MODE_AUTO];
    int8_t up, down;

    // an LR1121 on 868 MHz, free region: 31/19 Hz, FSK, 19 Hz 7x, 17/8 Hz, 18 Hz and LR, all at 20 dBm
    const uint16_t lr11_868 = B(MODE_31HZ) | B(MODE_19HZ) | B(MODE_FSK_50HZ) | B(MODE_19HZ_7X) | B(MODE_17HZ) | B(MODE_8HZ) | B(MODE_18HZ) | B(MODE_LR);
    tBand one[1] = { { B868, lr11_868, 20, 20 } };
    uint8_t n = build(one, 1, B868, st);
    check("one band: FSK, 31 Hz, 19 Hz, 17 Hz, 8 Hz", n == 5 && is(st[0], B868, MODE_FSK_50HZ) && is(st[1], B868, MODE_31HZ) &&
          is(st[2], B868, MODE_19HZ) && is(st[3], B868, MODE_17HZ) && is(st[4], B868, MODE_8HZ));
    check("  19 Hz 7x, 18 Hz and LR give nothing for their rate", n == 5);
    auto_ladder_neighbours(st, n, mode_rate_hz(MODE_19HZ), st[2].budget, &up, &down);
    check("  from 19 Hz: up to 31 Hz, down to 17 Hz", up == 1 && down == 3);
    auto_ladder_neighbours(st, n, mode_rate_hz(MODE_FSK_50HZ), st[0].budget, &up, &down);
    check("  from the fastest: no step up", up == -1 && down == 1);
    auto_ladder_neighbours(st, n, mode_rate_hz(MODE_8HZ), st[4].budget, &up, &down);
    check("  from the slowest: no step down", up == 3 && down == -1);

    // the same LR1121 with RF Band 2 = 2.4 GHz (50/31/19 Hz): FSK at 868 beats 2.4 GHz at every rate
    tBand two[2] = { { B868, lr11_868, 20, 20 }, { B2P4, B(MODE_50HZ) | B(MODE_31HZ) | B(MODE_19HZ), 20, 20 } };
    n = build(two, 2, B868, st);
    bool any_2p4 = false;
    for (uint8_t i = 0; i < n; i++) if (st[i].band == B2P4) any_2p4 = true;
    check("LR1121, 868 MHz and 2.4 GHz, free region: 2.4 GHz never pays", n == 5 && !any_2p4);
    auto_ladder_neighbours(st, n, mode_rate_hz(MODE_50HZ), 20 + 105 - 9, &up, &down);
    check("  from 2.4 GHz 50 Hz, off the ladder: down goes to FSK at the same rate", up == -1 && down == 0 && is(st[0], B868, MODE_FSK_50HZ));

    // an LR2021 with FLRC on 2.4 GHz: the fastest step is there
    tBand lr20[2] = { { B868, lr11_868, 20, 20 }, { B2P4, B(MODE_50HZ) | B(MODE_31HZ) | B(MODE_19HZ) | B(MODE_FLRC_111HZ), 20, 20 } };
    n = build(lr20, 2, B868, st);
    check("LR2021: 2.4 GHz FLRC on top, then the 868 MHz steps", n == 6 && is(st[0], B2P4, MODE_FLRC_111HZ) && is(st[1], B868, MODE_FSK_50HZ));
    auto_ladder_neighbours(st, n, mode_rate_hz(MODE_FSK_50HZ), st[1].budget, &up, &down);
    check("  from FSK: up to 2.4 GHz FLRC", up == 0 && down == 2);

    // the EU region: 868 MHz only FSK, 18 Hz and LR at 14 dBm (LR 27 dBm), 2.4 GHz at 18 dBm
    tBand eu[2] = { { B868, B(MODE_FSK_50HZ) | B(MODE_18HZ) | B(MODE_LR), 14, 27 }, { B2P4, B(MODE_50HZ) | B(MODE_31HZ) | B(MODE_19HZ), 18, 18 } };
    n = build(eu, 2, B868, st);
    check("EU: FSK, then 2.4 GHz 19 Hz, 18 Hz, LR", n == 4 && is(st[0], B868, MODE_FSK_50HZ) && is(st[1], B2P4, MODE_19HZ) &&
          is(st[2], B868, MODE_18HZ) && is(st[3], B868, MODE_LR));
    auto_ladder_neighbours(st, n, mode_rate_hz(MODE_18HZ), st[2].budget, &up, &down);
    check("  from 18 Hz: up to 2.4 GHz 19 Hz, down to LR", up == 1 && down == 3);

    // equal rate and budget on both bands: RF Band, added first, stays first
    tBand same[2] = { { B868, B(MODE_19HZ), 20, 20 }, { SETUP_FREQUENCY_BAND_866_MHZ_IN, B(MODE_19HZ), 20, 20 } };
    n = build(same, 2, SETUP_FREQUENCY_BAND_866_MHZ_IN, st);
    check("a tie keeps the band added first", n == 1 && st[0].band == B868);

    check("base mode: 19 Hz where there is one", mode_auto_base(lr11_868) == MODE_19HZ);
    check("  EU 868 MHz has none: 18 Hz, not FSK", mode_auto_base(B(MODE_FSK_50HZ) | B(MODE_18HZ) | B(MODE_LR)) == MODE_18HZ);
    check("  an SX127x has 19 Hz 7x", mode_auto_base(B(MODE_19HZ_7X)) == MODE_19HZ_7X);

    printf("%s\n", fails ? "FAILED" : "all ok");
    return fails ? 1 : 0;
}
