#define _XOPEN_SOURCE 700
#include "tge/tge_unicode.h"

#include <langinfo.h>
#include <locale.h>
#include <stdio.h>

static TGE_UnicodeMode override_mode = TGE_UNICODE_AUTO;
static int detected = -1; /* -1 unresolved, 0 ASCII, 1 Unicode */

static bool codeset_is_utf8(const char *codeset)
{
    if (!codeset)
        return false;
    for (const char *p = codeset; *p; p++) {
        if ((*p == 'u' || *p == 'U') && (p[1] == 't' || p[1] == 'T') &&
            (p[2] == 'f' || p[2] == 'F'))
            return true;
    }
    return false;
}

void tge_unicode_set_mode(TGE_UnicodeMode mode)
{
    override_mode = mode;
}

TGE_UnicodeMode tge_unicode_mode(void)
{
    if (override_mode != TGE_UNICODE_AUTO)
        return override_mode;
    return tge_unicode_supported() ? TGE_UNICODE_ON : TGE_UNICODE_OFF;
}

bool tge_unicode_supported(void)
{
    if (override_mode == TGE_UNICODE_ON)
        return true;
    if (override_mode == TGE_UNICODE_OFF)
        return false;

    if (detected < 0) {
        const char *codeset = NULL;
        if (setlocale(LC_CTYPE, "") != NULL)
            codeset = nl_langinfo(CODESET);
        bool utf8 = codeset_is_utf8(codeset);
        if (!utf8) {
            fprintf(stderr, "TGE: unicode=OFF (codeset=%s)\n",
                    codeset ? codeset : "unknown");
        }
        detected = utf8 ? 1 : 0;
    }
    return detected == 1;
}
