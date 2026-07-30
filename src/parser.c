#include "tge/tge_events.h"
#include "tge_internal.h"
#include <stdlib.h>
#include <string.h>

struct TGE_Parser {
    int dummy;
};

TGE_Parser *tge_parser_new(void)
{
    TGE_Parser *p = calloc(1, sizeof(TGE_Parser));
    (void)p;
    return NULL;
}

void tge_parser_free(TGE_Parser *p)
{
    (void)p;
}

void tge_parser_feed(TGE_Parser *p, const char *bytes, int len)
{
    (void)p; (void)bytes; (void)len;
}

bool tge_parser_poll(TGE_Parser *p, TGE_Event *ev)
{
    (void)p; (void)ev;
    return false;
}
