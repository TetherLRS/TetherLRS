// Host test of the SiK AT parser and mappings (mLRS/CommonTx/sik_at.h, TetherLRS). Run with test/run_tests.sh.
#include <cstdio>
#include <cstring>
#include "sik_at.h"

static int fails = 0;
static void check(const char* name, bool ok)
{
    printf("%-64s %s\n", name, ok ? "ok" : "FAIL");
    if (!ok) fails++;
}

static bool is(const char* line, bool remote, uint8_t cmd, uint8_t n = 0, uint32_t value = 0)
{
    tSikAtCmd c;
    if (!sik_at_parse(line, &c)) return false;
    if (cmd == SIK_AT_ERROR) return c.remote == remote && c.cmd == cmd; // n may be set, it is not used
    return c.remote == remote && c.cmd == cmd && c.n == n && c.value == value;
}

int main()
{
    tSikAtCmd c;
    check("not AT: CLI commands pass through", !sik_at_parse("pl", &c) && !sik_at_parse("p mode = 1", &c) &&
                                               !sik_at_parse("a", &c) && !sik_at_parse("", &c));
    check("AT, at, ATO, ato, ATZ, AT&W", is("AT", false, SIK_AT_OK) && is("at", false, SIK_AT_OK) &&
          is("ATO", false, SIK_AT_EXIT) && is("ato", false, SIK_AT_EXIT) && is("ATZ", false, SIK_AT_RESET) &&
          is("AT&W", false, SIK_AT_WRITE));
    check("ATI, ATI5, RTI5", is("ATI", false, SIK_AT_INFO, 0) && is("ATI5", false, SIK_AT_INFO, 5) &&
          is("RTI5", true, SIK_AT_INFO, 5));
    check("ATS3?, RTS1?, ATS15?", is("ATS3?", false, SIK_AT_GET, 3) && is("RTS1?", true, SIK_AT_GET, 1) &&
          is("ATS15?", false, SIK_AT_GET, 15));
    check("ATS3=25, rts4=20, spaces ignored", is("ATS3=25", false, SIK_AT_SET, 3, 25) &&
          is("rts4=20", true, SIK_AT_SET, 4, 20) && is("AT S 1 = 57", false, SIK_AT_SET, 1, 57));
    check("errors: S16, S3=, S3=x, ATI55, ATX, ATS3=1x", is("ATS16?", false, SIK_AT_ERROR) &&
          is("ATS3=", false, SIK_AT_ERROR) && is("ATS3=x", false, SIK_AT_ERROR) && is("ATI55", false, SIK_AT_ERROR) &&
          is("ATX", false, SIK_AT_ERROR) && is("ATS3=1x", false, SIK_AT_ERROR) && is("AT&F", false, SIK_AT_ERROR));
    check("huge value is an error, not a wrap", is("ATS8=99999999999", false, SIK_AT_ERROR));

    char bp[7];
    sik_bindphrase_from_netid(bp, 25);
    check("NETID 25 -> sik025 -> 25", !strcmp(bp, "sik025") && sik_netid_from_bindphrase(bp) == 25);
    sik_bindphrase_from_netid(bp, 499);
    check("NETID 499 round trip", sik_netid_from_bindphrase(bp) == 499);
    uint16_t h = sik_netid_from_bindphrase("mlrs.0");
    check("other phrases: stable number below 500", h < 500 && h == sik_netid_from_bindphrase("mlrs.0") &&
          h != sik_netid_from_bindphrase("mlrs.1"));
    check("sik999 is not a NETID (above 499), still below 500", sik_netid_from_bindphrase("sik999") < 500);

    check("serial speeds 57/115/230 -> 0/1/2, 38 -> none", sik_serial_speed_to_idx(57) == 0 &&
          sik_serial_speed_to_idx(115) == 1 && sik_serial_speed_to_idx(230) == 2 && sik_serial_speed_to_idx(38) < 0);

    const uint16_t rates[] = { 4100, 2562, 1547, 9111, 4100, 1547, 1490, 146 };
    check("air speed: 50 Hz 32, 31 Hz 19, 19 Hz 16, FLRC 64, 18 Hz 16, LR 4",
          sik_air_speed_from_rate(4100) == 32 && sik_air_speed_from_rate(2562) == 19 &&
          sik_air_speed_from_rate(1547) == 16 && sik_air_speed_from_rate(9111) == 64 &&
          sik_air_speed_from_rate(1490) == 16 && sik_air_speed_from_rate(146) == 4);
    uint32_t eu868 = (1 << 4) | (1 << 6) | (1 << 7); // FSK, 18 Hz, LR
    check("EU 868: 64 -> FSK, 16 -> 18 Hz, 4 -> LR", sik_pick_rate(64, rates, 8, eu868) == 4 &&
          sik_pick_rate(16, rates, 8, eu868) == 6 && sik_pick_rate(4, rates, 8, eu868) == 7);
    uint32_t free868 = (1 << 1) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7);
    check("free 868: 16 -> 19 Hz before 18 Hz, 19 -> 31 Hz", sik_pick_rate(16, rates, 8, free868) == 2 &&
          sik_pick_rate(19, rates, 8, free868) == 1);
    check("nothing allowed or 0 kbps -> -1", sik_pick_rate(16, rates, 8, 0) == -1 && sik_pick_rate(0, rates, 8, eu868) == -1);

    char s[16];
    check("utoa 0, 7, 868300, 4294967295", !strcmp(sik_utoa(0, s), "0") && !strcmp(sik_utoa(7, s), "7") &&
          !strcmp(sik_utoa(868300, s), "868300") && !strcmp(sik_utoa(4294967295u, s), "4294967295"));

    printf("%s\n", fails ? "FAILED" : "all passed");
    return fails ? 1 : 0;
}
