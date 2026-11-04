//*******************************************************
// Copyright (c) TetherLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.html
//*******************************************************
// Host test of tStatsLQ (src/common/lq_counter.cpp): an LQ never passes 100, whatever counts more than once a cycle.
// Run with test/run_tests.sh.
#include <cstdio>
#include <cstdint>
#include "lq_counter.cpp"

static int fails = 0;
static void check(const char* name, bool ok)
{
    printf("%-76s %s\n", name, ok ? "ok" : "FAIL");
    if (!ok) fails++;
}

int main(void)
{
    tStatsLQ lq;
    lq.Init(50);

    for (int i = 0; i < 45; i++) lq.Inc();
    lq.Update1Hz();
    check("45 frames of 50: 90", lq.GetLQ() == 90);

    for (int i = 0; i < 100; i++) lq.Inc(); // two lanes counted each
    lq.Update1Hz();
    check("100 counts in a second of 50 frames: 100, not 200 (72 in 7 bits)", lq.GetLQ() == 100);

    for (int i = 0; i < 200; i++) lq.Inc(); // four receivers
    lq.Update1Hz();
    check("200 counts: 100, not 400 (144 in a byte)", lq.GetLQ() == 100);

    lq.Update1Hz();
    check("nothing: 0", lq.GetLQ() == 0);

    printf("%s\n", fails ? "FAILED" : "all ok");
    return fails ? 1 : 0;
}
