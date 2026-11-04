//*******************************************************
// Copyright (c) TetherLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.html
//*******************************************************
// Host test of the two-radio FHSS on one band (src/common/fhss.h): with dual redundant and bonded the 2nd sequencer
// trails the 1st by half the hops in use, after Start as after Init, and also when ortho or the WiFi except cut the
// list below Num. Run with test/run_tests.sh.
#define DEVICE_HAS_SX128x
#define FREQUENCY_BAND_2P4_GHZ
#define USE_SX2
#include <cstdio>
#include "fhss.h"
#include "fhss.cpp"

static int fails = 0;
static void check(const char* name, bool ok)
{
    printf("%-76s %s\n", name, ok ? "ok" : "FAIL");
    if (!ok) fails++;
}

// both radios over two full rounds of the hop list: never on the same channel in the same slot
static bool apart(tFhss& f)
{
    for (uint16_t k = 0; k < 2 * f.Cnt(); k++) {
        if (f.GetCurrFreq() == f.GetCurrFreq2()) return false;
        f.HopToNext();
    }
    return true;
}

int main(void)
{
    tFhssGlobalConfig c = {};
    c.Num = 24; c.Seed = 0x12345678; c.FrequencyBand = SX_FHSS_FREQUENCY_BAND_2P4_GHZ; c.BindScan_mask = 0x01;
    c.Ortho = ORTHO_NONE; c.Except = EXCEPT_NONE; c.Plan = FHSS_PLAN_DEFAULT; c.Offset = 0;
    tFhssGlobalConfig c2 = c;
    c2.Offset = 1;

    tFhss f;
    f.Init(&c, &c2);
    check("dual redundant: the radios apart right after Init", apart(f));
    f.Init(&c, &c2);
    f.Start();
    check("  and after Start, which puts the 1st sequencer back to 0", f.GetCurrI() == 0 && apart(f));
    check("  the 2nd trails by half the hops", f.GetCurrI2() == (f.GetCurrI() + f.Cnt() / 2) % f.Cnt());

    c.Ortho = ORTHO_1_3; c2 = c; c2.Offset = 1;
    f.Init(&c, &c2);
    f.Start();
    check("with ortho the list is cut below Num", f.Cnt() < c.Num);
    check("  and the radios stay apart", apart(f));

    c.Ortho = ORTHO_NONE; c.Except = EXCEPT_2P4_GHZ_WIFIBAND_1; c2 = c; c2.Offset = 1;
    f.Init(&c, &c2);
    f.Start();
    check("with a WiFi band excepted the radios stay apart", apart(f));

    c.Except = EXCEPT_NONE; c2 = c;
    f.Init(&c, &c2);
    f.Start();
    bool same = true;
    for (uint8_t k = 0; k < f.Cnt(); k++) { same = same && (f.GetCurrFreq() == f.GetCurrFreq2()); f.HopToNext(); }
    check("one frame per radio, no offset (diversity): both on the same channel", same);

    c2.Offset = 1;
    f.Init(&c, &c2);
    f.Start();
    f.SetCurrI(5);
    check("the receiver catching up on the 1st keeps the 2nd trailing", f.GetCurrI2() == (5 + f.Cnt() / 2) % f.Cnt());

    printf("%s\n", fails ? "FAILED" : "all ok");
    return fails ? 1 : 0;
}
