// Host test of tMavlinkCoalesce (mLRS/Common/mavlink_coalesce.h, TetherLRS). Run with test/run_tests.sh.
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <string>
#include <vector>
#include "mavlink_coalesce.h"

static int fails = 0;
static void check(const char* name, bool ok)
{
    printf("%-66s %s\n", name, ok ? "ok" : "FAIL");
    if (!ok) fails++;
}

enum { HEARTBEAT = 0, ATTITUDE = 30, GLOBAL_POSITION_INT = 33, PARAM_VALUE = 22, STATUSTEXT = 253 };

// a frame whose bytes say which one it is: tag repeated len times
static std::string frame(char tag, int len) { return std::string(len, tag); }

static void put(tMavlinkCoalesce& q, const std::string& f, uint32_t msgid, uint8_t sys = 1, uint8_t comp = 1)
{
    if (!q.HasSpace(f.size())) { printf("    no space for %c\n", f[0]); return; }
    q.Put((const uint8_t*)f.data(), f.size(), msgid, sys, comp);
}

static std::string drain(tMavlinkCoalesce& q, int max = 1 << 30)
{
    std::string s;
    while (q.Available() && max-- > 0) s += (char)q.Get();
    return s;
}

// the same rules on plain lists, for the random comparison
struct Model {
    struct F { std::string bytes; uint32_t msgid; uint16_t sender; bool started = false; };
    std::deque<F> q;
    void put(const std::string& b, uint32_t msgid, uint16_t sender)
    {
        if (mavlink_msg_coalescible(msgid)) {
            for (auto it = q.begin(); it != q.end();) {
                if (!it->started && it->msgid == msgid && it->sender == sender) it = q.erase(it); else ++it;
            }
        }
        q.push_back({b, msgid, sender});
    }
    size_t available() { size_t n = 0; for (auto& f : q) n += f.bytes.size(); return n; }
    char get()
    {
        q.front().started = true;
        char c = q.front().bytes[0];
        q.front().bytes.erase(0, 1);
        if (q.front().bytes.empty()) q.pop_front();
        return c;
    }
};

int main()
{
    {
        tMavlinkCoalesce q; q.Init();
        put(q, frame('a', 30), PARAM_VALUE); put(q, frame('b', 20), PARAM_VALUE); put(q, frame('c', 40), STATUSTEXT);
        check("messages that must arrive pass unchanged and in order", drain(q) == frame('a', 30) + frame('b', 20) + frame('c', 40));
    }
    {
        tMavlinkCoalesce q; q.Init();
        put(q, frame('1', 28), ATTITUDE); put(q, frame('2', 28), ATTITUDE); put(q, frame('3', 28), ATTITUDE);
        check("three queued ATTITUDE: only the newest goes", drain(q) == frame('3', 28) && q.dropped == 2);
    }
    {
        tMavlinkCoalesce q; q.Init();
        put(q, frame('1', 28), ATTITUDE, 1, 1); put(q, frame('2', 28), ATTITUDE, 2, 1); put(q, frame('3', 28), ATTITUDE, 1, 191);
        check("different senders are not merged", drain(q) == frame('1', 28) + frame('2', 28) + frame('3', 28));
    }
    {
        tMavlinkCoalesce q; q.Init();
        put(q, frame('1', 28), ATTITUDE);
        std::string out = drain(q, 3);
        put(q, frame('2', 28), ATTITUDE);
        out += drain(q);
        check("a frame on its way is never cut", out == frame('1', 28) + frame('2', 28));
    }
    {
        tMavlinkCoalesce q; q.Init();
        put(q, frame('1', 28), ATTITUDE); put(q, frame('p', 30), PARAM_VALUE); put(q, frame('2', 28), ATTITUDE);
        check("the newer copy keeps its own place, after what came before it", drain(q) == frame('p', 30) + frame('2', 28));
    }
    {
        tMavlinkCoalesce q; q.Init();
        put(q, frame('h', 21), HEARTBEAT); put(q, frame('g', 40), GLOBAL_POSITION_INT); put(q, frame('H', 21), HEARTBEAT);
        check("Available counts only what will go", q.Available() == 61 && drain(q) == frame('g', 40) + frame('H', 21));
    }
    {
        tMavlinkCoalesce q; q.Init();
        int n = 0;
        while (n < 100 && q.HasSpace(280)) { put(q, frame('a' + n % 26, 280), PARAM_VALUE); n++; }
        std::string first = drain(q, 600);
        bool again = q.HasSpace(280);
        check("full queue: no room, and room again once the link took bytes", n == 7 && first.size() == 600 && again);
    }
    {
        tMavlinkCoalesce q; q.Init();
        for (int i = 0; i < 70; i++) put(q, frame('0' + i % 10, 28), ATTITUDE); // far more than fit without dropping
        check("discarded copies give their space back (compaction)", q.Available() == 28 && q.HasSpace(2000));
    }
    {
        tMavlinkCoalesce q; q.Init();
        put(q, frame('1', 100), ATTITUDE);
        std::string out = drain(q, 60);
        for (int i = 0; i < 6; i++) put(q, frame('p', 300), PARAM_VALUE); // forces a compaction with the head half sent
        put(q, frame('2', 28), ATTITUDE);
        out += drain(q);
        check("compaction with the head half sent keeps its rest", out == frame('1', 100) + std::string(6 * 300, 'p') + frame('2', 28));
    }
    {   // random traffic against the list model
        srand(12345);
        bool same = true;
        long bytes = 0;
        for (int round = 0; round < 200 && same; round++) {
            tMavlinkCoalesce q; q.Init();
            Model m;
            for (int step = 0; step < 2000 && same; step++) {
                if (rand() % 3) {
                    static const uint32_t ids[] = { HEARTBEAT, ATTITUDE, GLOBAL_POSITION_INT, PARAM_VALUE, STATUSTEXT, 11031 };
                    uint32_t id = ids[rand() % 6];
                    uint16_t sender = (rand() % 2) ? 0x0101 : 0x01BE;
                    int len = 12 + rand() % 268;
                    std::string f;
                    for (int i = 0; i < len; i++) f += (char)(rand() & 0xFF);
                    if (q.HasSpace(len)) {
                        q.Put((const uint8_t*)f.data(), len, id, sender >> 8, sender & 0xFF);
                        m.put(f, id, sender);
                    }
                } else {
                    int k = rand() % 200;
                    for (int i = 0; i < k && q.Available(); i++) {
                        bytes++;
                        if ((char)q.Get() != m.get()) { same = false; break; }
                    }
                }
                if (q.Available() != m.available()) same = false;
            }
        }
        printf("    %ld bytes compared\n", bytes);
        check("random traffic: same bytes as the list model, 200 x 2000 steps", same && bytes > 1000000);
    }
    printf("%s\n", fails ? "FAILED" : "all passed");
    return fails ? 1 : 0;
}
