#include "tge/tge_unicode.h"

#include "tge_test.h"

TGE_TEST(set_and_read_modes)
{
    tge_unicode_set_mode(TGE_UNICODE_ON);
    TGE_ASSERT(tge_unicode_mode() == TGE_UNICODE_ON, "mode reads ON");
    TGE_ASSERT(tge_unicode_supported(), "ON means supported");

    tge_unicode_set_mode(TGE_UNICODE_OFF);
    TGE_ASSERT(tge_unicode_mode() == TGE_UNICODE_OFF, "mode reads OFF");
    TGE_ASSERT(!tge_unicode_supported(), "OFF means not supported");

    tge_unicode_set_mode(TGE_UNICODE_AUTO);
    TGE_ASSERT(tge_unicode_mode() == TGE_UNICODE_ON ||
                   tge_unicode_mode() == TGE_UNICODE_OFF,
               "AUTO resolves to a concrete mode");
    TGE_ASSERT(tge_unicode_supported() ==
                   (tge_unicode_mode() == TGE_UNICODE_ON),
               "supported follows the resolved AUTO mode");
}

TGE_TEST(override_stable_while_set)
{
    tge_unicode_set_mode(TGE_UNICODE_OFF);
    bool off = tge_unicode_supported();
    tge_unicode_set_mode(TGE_UNICODE_ON);
    bool on = tge_unicode_supported();
    TGE_ASSERT(!off && on, "override flips support");
    tge_unicode_set_mode(TGE_UNICODE_AUTO);
}

int main(void)
{
    test_set_and_read_modes();
    test_override_stable_while_set();
    return tge_test_report();
}
