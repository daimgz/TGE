#include "tge/tge_runtime.h"
#include "tge/tge_canvas.h"
#include "tge/tge_events.h"
#include <stdlib.h>
#include <stdio.h>

int main(void)
{
    TGE_Runtime *rt = tge_runtime_create(80, 24);
    if (!rt) {
        fprintf(stderr, "Failed to create runtime\n");
        return 1;
    }

    printf("Runtime created: %dx%d\n",
           tge_runtime_width(rt),
           tge_runtime_height(rt));

    tge_runtime_destroy(rt);
    printf("Runtime destroyed. OK\n");
    return 0;
}
