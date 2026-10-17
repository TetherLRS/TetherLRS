// Host test of tSerialEscapeCore (mLRS/Common/serial_escape.h, TetherLRS). Run with test/run_tests.sh.
#define SERIAL_ESCAPE_CORE_ONLY
#include <cstdio>
#include <cstring>
#include <string>
#include "serial_escape.h"

static int fails = 0;

static std::string drain(tSerialEscapeCore& e)
{
    std::string s;
    while (e.DataAvailable()) s += e.DataGetc();
    return s;
}

static void feed(tSerialEscapeCore& e, const char* s, uint32_t& t, uint32_t step_ms = 1)
{
    for (; *s; s++) { e.Feed(*s, t); t += step_ms; e.Tick(t); }
}

static void check(const char* name, bool ok)
{
    printf("%-60s %s\n", name, ok ? "ok" : "FAIL");
    if (!ok) fails++;
}

int main()
{
    {   // data without "+" goes straight through
        tSerialEscapeCore e; e.Init(); uint32_t t = 5000;
        feed(e, "hello", t);
        check("data passes through unchanged", drain(e) == "hello" && !e.IsCommand());
    }
    {   // the escape
        tSerialEscapeCore e; e.Init(); uint32_t t = 5000;
        feed(e, "+++", t, 100);
        bool before = e.IsCommand();
        bool entered = e.Tick(t + 1000);
        check("+++ after and before 1 s of silence enters command mode", !before && entered && e.IsCommand());
        check("the +++ itself is not passed as data", drain(e) == "");
    }
    {   // right at power up the line counts as silent
        tSerialEscapeCore e; e.Init(); uint32_t t = 0;
        feed(e, "+++", t, 50);
        e.Tick(t + 1000);
        check("+++ right after power up works", e.IsCommand());
    }
    {   // no leading silence: it is data
        tSerialEscapeCore e; e.Init(); uint32_t t = 5000;
        feed(e, "ab+++", t, 10);
        e.Tick(t + 2000);
        check("+++ right after data is data", !e.IsCommand() && drain(e) == "ab+++");
    }
    {   // a fourth "+" breaks the sequence at once: no wait for the guard
        tSerialEscapeCore e; e.Init(); uint32_t t = 5000;
        feed(e, "++++", t, 10);
        std::string at_once = drain(e);
        e.Tick(t + 2000);
        check("++++ is data, in order, given back at once", !e.IsCommand() && at_once == "++++" && drain(e) == "");
    }
    {   // no trailing silence
        tSerialEscapeCore e; e.Init(); uint32_t t = 5000;
        feed(e, "+++x", t, 100);
        e.Tick(t + 2000);
        check("+++ followed by a byte within 1 s is data, in order", !e.IsCommand() && drain(e) == "+++x");
    }
    {   // an incomplete sequence comes back after the guard
        tSerialEscapeCore e; e.Init(); uint32_t t = 5000;
        feed(e, "++", t, 100);
        std::string held = drain(e);
        e.Tick(t + 1000);
        check("++ then silence: held, then given back as data", held == "" && drain(e) == "++" && !e.IsCommand());
    }
    {   // "+" in the middle of a stream is not held
        tSerialEscapeCore e; e.Init(); uint32_t t = 5000;
        feed(e, "a+b", t, 1);
        check("a + inside a stream is not delayed", drain(e) == "a+b");
    }
    {   // after exit a new escape needs a new silence
        tSerialEscapeCore e; e.Init(); uint32_t t = 5000;
        feed(e, "+++", t, 10);
        e.Tick(t + 1000); t += 1000;
        e.ExitCommand(t);
        feed(e, "+++", t, 10);
        e.Tick(t + 1500);
        bool again_without_silence = e.IsCommand();
        check("after exit, +++ without a new silence is data", !again_without_silence && drain(e) == "+++");
    }
    {   // a time wrap of the millisecond counter
        tSerialEscapeCore e; e.Init(); uint32_t t = 0xFFFFFF00u;
        feed(e, "z", t, 1);
        t += 1100;
        feed(e, "+++", t, 10);
        e.Tick(t + 1000);
        check("works across the 32 bit millisecond wrap", e.IsCommand() && drain(e) == "z");
    }
    printf("%s\n", fails ? "FAILED" : "all passed");
    return fails ? 1 : 0;
}
