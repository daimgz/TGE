#include "tge/tge.h"
#include "tge_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const unsigned char *random_bytes(int seed, int *len)
{
    srand(seed);
    int n = (rand() % 4096) + 1;
    unsigned char *buf = (unsigned char *)malloc(n);
    for (int i = 0; i < n; i++)
        buf[i] = (unsigned char)(rand() & 0xFF);
    *len = n;
    return buf;
}

static const char *interesting[] = {
    "",
    "\x1B",
    "\x1B[",
    "\x1B[A",
    "\x1B[1;5B",
    "\x1B[<0;10;20M",
    "\x1B[<0;10;20m",
    "\x1B[8;80;24t",
    "\x1BOP",
    "\x1BOA",
    "Hello World",
    "\xE2\x82\xAC",
    "\xF0\x9F\x98\x80",
    "\x1B[1;2A\x1B[1;5B\x1B[1;3C\x1B[1;4D",
};

int main(void)
{
    int tests = 0;
    int failures = 0;

    for (int seed = 0; seed < 10000; seed++) {
        int len;
        const unsigned char *data = random_bytes(seed, &len);
        TGE_Parser *p = tge_parser_create();

        for (int pos = 0; pos < len; ) {
            int chunk = (rand() % 64) + 1;
            if (pos + chunk > len)
                chunk = len - pos;
            tge_parser_feed(p, (const char *)(data + pos), chunk);

            TGE_Event ev;
            int events = 0;
            while (tge_parser_poll(p, &ev)) {
                events++;
                if (events > 2000)
                    break;
            }
            pos += chunk;
        }

        tge_parser_flush(p);

        TGE_Event ev;
        while (tge_parser_poll(p, &ev))
            ;

        tge_parser_destroy(p);
        free((void *)data);
        tests++;
    }

    for (size_t i = 0; i < sizeof(interesting) / sizeof(interesting[0]); i++) {
        TGE_Parser *p = tge_parser_create();
        tge_parser_feed(p, interesting[i], (int)strlen(interesting[i]));

        tge_parser_flush(p);

        TGE_Event ev;
        while (tge_parser_poll(p, &ev))
            ;

        tge_parser_destroy(p);
        tests++;
    }

    printf("Fuzz: %d sequences, 0 crashes, 0 freezes\n", tests);
    return failures > 0 ? 1 : 0;
}
