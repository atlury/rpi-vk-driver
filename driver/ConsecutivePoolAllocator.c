#include "ConsecutivePoolAllocator.h"
#include "PoolLink.h"
#include <stdint.h>
#include <string.h>

ConsecutivePoolAllocator createConsecutivePoolAllocator(void *b, unsigned bs, unsigned s)
{
    if (!b || bs < sizeof(void *) || s < bs || s % bs)
        return (ConsecutivePoolAllocator){0};
    ConsecutivePoolAllocator pa = {b, b, bs, s, s / bs};
    for (unsigned offset = 0; offset < s; offset += bs)
        poolSetNext((char *)b + offset,
                    offset + bs < s ? (char *)b + offset + bs : NULL);
    return pa;
}

void destroyConsecutivePoolAllocator(ConsecutivePoolAllocator *pa)
{
    *pa = (ConsecutivePoolAllocator){0};
}

/* Links stay sorted by address so freed runs can coalesce in either order.
 * Returned offsets remain 32-bit: the pool's size already has that limit. */
uint32_t consecutivePoolAllocate(ConsecutivePoolAllocator *pa, uint32_t count)
{
    if (!count || count > pa->numFreeBlocks)
        return UINT32_MAX;
    void *prev = NULL;
    for (char *start = pa->nextFreeBlock; start; start = poolNext(start)) {
        char *last = start;
        uint32_t found = 1;
        while (found < count && poolNext(last) == last + pa->blockSize) {
            last += pa->blockSize;
            ++found;
        }
        if (found == count) {
            void *next = poolNext(last);
            if (prev) poolSetNext(prev, next);
            else pa->nextFreeBlock = next;
            pa->numFreeBlocks -= count;
            return (uint32_t)(start - (char *)pa->buf);
        }
        prev = start;
    }
    return UINT32_MAX;
}

void consecutivePoolFree(ConsecutivePoolAllocator *pa, void *p, uint32_t count)
{
    if (!count) return;
    void *prev = NULL;
    char *next = pa->nextFreeBlock;
    while (next && next < (char *)p) {
        prev = next;
        next = poolNext(next);
    }
    for (uint32_t i = 0; i < count; ++i) {
        char *block = (char *)p + i * pa->blockSize;
        poolSetNext(block, i + 1 < count ? block + pa->blockSize : next);
    }
    if (prev) poolSetNext(prev, p);
    else pa->nextFreeBlock = p;
    pa->numFreeBlocks += count;
}

uint32_t consecutivePoolReAllocate(ConsecutivePoolAllocator *pa, void *mem,
                                  uint32_t oldCount, uint32_t newCount)
{
    if (!mem || !oldCount) return UINT32_MAX;
    uint32_t offset = (uint32_t)((char *)mem - (char *)pa->buf);
    if (newCount <= oldCount) {
        consecutivePoolFree(pa, (char *)mem + newCount * pa->blockSize,
                            oldCount - newCount);
        return newCount ? offset : UINT32_MAX;
    }
    uint32_t extra = newCount - oldCount;
    if (extra > pa->numFreeBlocks) return UINT32_MAX;
    char *end = (char *)mem + oldCount * pa->blockSize;
    void *prev = NULL;
    char *next = pa->nextFreeBlock;
    while (next && next < end) {
        prev = next;
        next = poolNext(next);
    }
    if (next == end) {
        char *last = next;
        uint32_t found = 1;
        while (found < extra && poolNext(last) == last + pa->blockSize) {
            last += pa->blockSize;
            ++found;
        }
        if (found == extra) {
            if (prev) poolSetNext(prev, poolNext(last));
            else pa->nextFreeBlock = poolNext(last);
            pa->numFreeBlocks -= extra;
            return offset;
        }
    }
    uint32_t replacement = consecutivePoolAllocate(pa, newCount);
    if (replacement == UINT32_MAX) return UINT32_MAX;
    memcpy((char *)pa->buf + replacement, mem, oldCount * pa->blockSize);
    consecutivePoolFree(pa, mem, oldCount);
    return replacement;
}

void *getCPAptrFromOffset(ConsecutivePoolAllocator *pa, uint32_t offset)
{
    assert(pa && pa->buf && offset < pa->size);
    return (char *)pa->buf + offset;
}

void CPAdebugPrint(ConsecutivePoolAllocator *pa)
{
    fprintf(stderr, "CPA buffer=%p free=%u\n", pa->buf, pa->numFreeBlocks);
    for (void *p = pa->nextFreeBlock; p; p = poolNext(p))
        fprintf(stderr, "%p: %p\n", p, poolNext(p));
}
