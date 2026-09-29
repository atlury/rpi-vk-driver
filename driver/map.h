#pragma once

#if defined (__cplusplus)
extern "C" {
#endif

#include <stdint.h>
#include "CustomAssert.h"

typedef struct
{
	void* data;
	uintptr_t key;
} mapElem;

typedef struct
{
	mapElem* elements;
	uint32_t maxData;
} map;

void* getMapElement(map m, uintptr_t key);
void setMapElement(map* m, uintptr_t key, void* data);
void deleteMapElement(map* m, uintptr_t key);
map createMap(void* buf, uint32_t maxData);
void destroyMap(map* m);


#if defined (__cplusplus)
}
#endif



