#include "map.h"

static uint32_t getIndex(uintptr_t key, uint32_t capacity)
{
    /* Unsigned multiplication is defined on both 32- and 64-bit hosts. */
    return (uint32_t)((key * (uintptr_t)0x678DDE6F) % capacity);
}

void *getMapElement(map m, uintptr_t key)
{
    if (!m.elements || !m.maxData) return NULL;
    uint32_t index = getIndex(key, m.maxData);
    for (uint32_t i = 0; i < m.maxData; ++i, index = (index + 1) % m.maxData) {
        if (!m.elements[index].data) return NULL;
        if (m.elements[index].key == key) return m.elements[index].data;
    }
    return NULL;
}

void setMapElement(map *m, uintptr_t key, void *data)
{
    if (!m->elements || !m->maxData) return;
    if (!data) { deleteMapElement(m, key); return; }
    uint32_t index = getIndex(key, m->maxData);
    for (uint32_t i = 0; i < m->maxData; ++i, index = (index + 1) % m->maxData) {
        if (!m->elements[index].data || m->elements[index].key == key) {
            m->elements[index].key = key;
            m->elements[index].data = data;
            return;
        }
    }
}

void deleteMapElement(map *m, uintptr_t key)
{
    if (!m->elements || !m->maxData) return;
    uint32_t index = getIndex(key, m->maxData);
    for (uint32_t i = 0; i < m->maxData; ++i, index = (index + 1) % m->maxData) {
        if (!m->elements[index].data) return;
        if (m->elements[index].key != key) continue;
        m->elements[index].data = NULL;
        /* Reinsert the following cluster so a deleted collision does not
         * hide a later key. Bound the walk even when the table was full. */
        index = (index + 1) % m->maxData;
        for (uint32_t j = 0; j + 1 < m->maxData && m->elements[index].data; ++j) {
            mapElem entry = m->elements[index];
            m->elements[index].data = NULL;
            setMapElement(m, entry.key, entry.data);
            index = (index + 1) % m->maxData;
        }
        return;
    }
}

map createMap(void *buf, uint32_t capacity)
{
    map m = {buf, buf ? capacity : 0};
    for (uint32_t i = 0; i < m.maxData; ++i) m.elements[i] = (mapElem){0};
    return m;
}

void destroyMap(map *m) { *m = (map){0}; }
