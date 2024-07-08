#pragma once
#include "kernel/types.h"

// placement new
void *operator new(size_t, void *ptr)
{
    return ptr;
}
