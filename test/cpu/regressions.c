#include "PoolAllocator.h"
#include "ConsecutivePoolAllocator.h"
#include "vkExt.h"
#include "map.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

static unsigned rng = 713;
static unsigned randomValue(void) { rng = rng * 1664525u + 1013904223u; return rng; }

static void pools(void)
{
    unsigned stride = (unsigned)sizeof(void *) + 1;
    unsigned char *storage = malloc(stride * 8 + 1);
    CHECK(storage);
    unsigned char *buf = storage + 1;
    PoolAllocator pa = createPoolAllocator(buf, stride, stride * 8);
    for (unsigned i = 0; i < 8; ++i) CHECK(poolAllocate(&pa) == buf + i * stride);
    CHECK(!poolAllocate(&pa));
    poolFree(&pa, buf + 3 * stride);
    poolFree(&pa, buf + 6 * stride);
    CHECK(poolAllocate(&pa) == buf + 6 * stride);
    CHECK(poolAllocate(&pa) == buf + 3 * stride);
    CHECK(!poolAllocate(&pa));
    destroyPoolAllocator(&pa);
    CHECK(!pa.buf && !pa.nextFreeBlock);
    CHECK(!createPoolAllocator(buf, 0, 8).buf);
    CHECK(!createConsecutivePoolAllocator(buf, 0, 8).buf);
    free(storage);
}

static void reallocCases(void)
{
    unsigned char buffer[16 * 8];
    ConsecutivePoolAllocator pa = createConsecutivePoolAllocator(buffer, 16, sizeof(buffer));
    CHECK(consecutivePoolAllocate(&pa, 0) == UINT32_MAX);
    CHECK(consecutivePoolAllocate(&pa, 1) == 0);
    CHECK(consecutivePoolAllocate(&pa, 7) == 16);
    consecutivePoolFree(&pa, buffer, 1);
    CHECK(consecutivePoolReAllocate(&pa, buffer + 16, 7, 8) == UINT32_MAX);
    CHECK(pa.numFreeBlocks == 1);
    consecutivePoolFree(&pa, buffer + 16, 7);
    CHECK(consecutivePoolAllocate(&pa, 7) == 0);
    memset(buffer, 0x45, 7 * 16);
    CHECK(consecutivePoolReAllocate(&pa, buffer, 7, 8) == 0);
    CHECK(!pa.nextFreeBlock && pa.numFreeBlocks == 0);
    CHECK(consecutivePoolReAllocate(&pa, buffer, 8, 8) == 0);
    CHECK(consecutivePoolReAllocate(&pa, buffer, 8, 2) == 0);
    CHECK(pa.numFreeBlocks == 6);
    for (unsigned i = 0; i < 32; ++i) CHECK(buffer[i] == 0x45);
    CHECK(consecutivePoolReAllocate(&pa, buffer, 2, 0) == UINT32_MAX);
    CHECK(pa.numFreeBlocks == 8);
    CHECK(consecutivePoolAllocate(&pa, 8) == 0);
    consecutivePoolFree(&pa, buffer + 4 * 16, 4);
    consecutivePoolFree(&pa, buffer, 4);
    CHECK(consecutivePoolAllocate(&pa, 8) == 0);
    destroyConsecutivePoolAllocator(&pa);
    CHECK(!pa.numFreeBlocks && !pa.buf);
}

static void randomPools(void)
{
    enum {BLOCKS = 96, SLOTS = 24};
    unsigned stride = (unsigned)sizeof(void *) + 1;
    unsigned char *storage = malloc(BLOCKS * stride + 1);
    CHECK(storage);
    unsigned char *buf = storage + 1;
    ConsecutivePoolAllocator pa = createConsecutivePoolAllocator(buf, stride, BLOCKS * stride);
    struct { uint32_t offset, count; } slots[SLOTS] = {{0}};
    for (unsigned round = 0; round < 20000; ++round) {
        unsigned slot = (randomValue() >> 16) % SLOTS;
        unsigned old = slots[slot].count;
        unsigned char tag = (unsigned char)(slot + 1);
        unsigned count = (randomValue() >> 16) % 12;
        if (!old) {
            uint32_t offset = consecutivePoolAllocate(&pa, count);
            if (offset != UINT32_MAX) {
                slots[slot].offset = offset;
                slots[slot].count = count;
                memset(buf + offset, tag, count * stride);
            }
        } else if (!count) {
            consecutivePoolFree(&pa, buf + slots[slot].offset, old);
            slots[slot].count = 0;
        } else {
            uint32_t offset = consecutivePoolReAllocate(&pa, buf + slots[slot].offset, old, count);
            if (offset != UINT32_MAX) {
                for (unsigned i = 0; i < (old < count ? old : count) * stride; ++i)
                    CHECK(buf[offset + i] == tag);
                slots[slot].offset = offset;
                slots[slot].count = count;
                memset(buf + offset, tag, count * stride);
            }
        }
        unsigned used[BLOCKS] = {0}, occupied = 0;
        for (unsigned i = 0; i < SLOTS; ++i) {
            if (!slots[i].count) continue;
            CHECK(slots[i].offset % stride == 0);
            unsigned start = slots[i].offset / stride;
            CHECK(start + slots[i].count <= BLOCKS);
            for (unsigned j = 0; j < slots[i].count; ++j) CHECK(!used[start + j]++);
            for (unsigned j = 0; j < slots[i].count * stride; ++j)
                CHECK(buf[slots[i].offset + j] == i + 1);
            occupied += slots[i].count;
        }
        CHECK(pa.numFreeBlocks == BLOCKS - occupied);
        unsigned freeCount = 0;
        for (void *p = pa.nextFreeBlock; p;) {
            uintptr_t address = (uintptr_t)p, base = (uintptr_t)buf;
            CHECK(address >= base && address - base < BLOCKS * stride);
            CHECK((address - base) % stride == 0);
            CHECK(!used[(address - base) / stride]++);
            ++freeCount;
            memcpy(&p, p, sizeof(p));
        }
        CHECK(freeCount == pa.numFreeBlocks);
    }
    free(storage);
}

static void assemblyTransport(void)
{
    VkRpiShaderModuleAssemblyCreateInfoEXT *info = calloc(1, sizeof(*info));
    CHECK(info);
    uint32_t code[6];
    vkRpiEncodeAssemblyEXT(code, info);
    CHECK(vkRpiDecodeAssemblyEXT(code, sizeof(code)) == info);
    CHECK(!vkRpiDecodeAssemblyEXT(NULL, sizeof(code)));
    CHECK(!vkRpiDecodeAssemblyEXT(code, sizeof(code) - 4));
    code[2] = 0; /* A normal SPIR-V generator ID is not our private protocol. */
    CHECK(!vkRpiDecodeAssemblyEXT(code, sizeof(code)));
    free(info);
}

static void maps(void)
{
    mapElem entries[8];
    map m = createMap(entries, 8);
    int values[9] = {0};
    uintptr_t key = (uintptr_t)entries;
    for (unsigned i = 0; i < 8; ++i) setMapElement(&m, key + i * 8, &values[i]);
    CHECK(!getMapElement(m, key + 99 * 8));
    deleteMapElement(&m, key + 99 * 8);
    deleteMapElement(&m, key + 2 * 8);
    for (unsigned i = 0; i < 8; ++i)
        CHECK(getMapElement(m, key + i * 8) == (i == 2 ? NULL : &values[i]));
    setMapElement(&m, key + 2 * 8, &values[8]);
    CHECK(getMapElement(m, key + 2 * 8) == &values[8]);
#if UINTPTR_MAX > UINT32_MAX
    m = createMap(entries, 8);
    setMapElement(&m, 1, &values[0]);
    setMapElement(&m, ((uintptr_t)1 << 32) + 1, &values[1]);
    CHECK(getMapElement(m, 1) == &values[0]);
    CHECK(getMapElement(m, ((uintptr_t)1 << 32) + 1) == &values[1]);
#endif
    mapElem *large = calloc(8192, sizeof(*large));
    CHECK(large);
    m = createMap(large, 8192);
    setMapElement(&m, key, values);
    CHECK(getMapElement(m, key) == values);
    free(large);
}

int main(void)
{
    pools(); reallocCases(); randomPools(); assemblyTransport(); maps();
    printf("CPU_REGRESSIONS_PASS pointer_bits=%zu randomized_operations=20000\n", sizeof(void *) * 8);
    return 0;
}
