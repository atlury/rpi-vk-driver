#pragma once

#include <string.h>

/* Free-list links are native pointers, not GPU addresses. memcpy also allows
 * pools whose block stride is not aligned to sizeof(void *). */
static inline void *poolNext(const void *block)
{
    void *next;
    memcpy(&next, block, sizeof(next));
    return next;
}

static inline void poolSetNext(void *block, void *next)
{
    memcpy(block, &next, sizeof(next));
}
