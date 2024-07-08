#pragma once
#include "kernel/types.h"
#include "include/math.h"
#include "kernel/kmalloc.h"

template<typename T>
struct Vector
{
    T *mem;
    u32 length;
    u32 capacity;

    Vector() : mem(0), length(0), capacity(0) {}

    Vector(u32 initial_capacity) : mem(0), length(0), capacity(initial_capacity)
    {
        if(initial_capacity == 0) {
        } else {
            dbg_str("DEBUG: VECTOR KMALLOC\n");
            mem = (T *)kmalloc(initial_capacity * sizeof(T), alignof(T));
        }
    }

    ~Vector()
    {
        if(capacity == 0) {
            ASSERT(mem == 0);
        } else {
            for(u32 i = 0; i < length; ++i) {
                mem[i].~T();
            }
            kfree((vaddr)mem);
        }
    }

    void append(T obj)
    {
        if(length + 1 > capacity)
            expand_capacity(max(capacity * 2, 64u)); // NOTE capcity can be 0
        mem[length] = obj;
        length++;
    }

    void expand_capacity(u32 new_capacity)
    {
        if(new_capacity <= capacity)
            return;

        dbg_str("DEBUG: VECTOR KMALLOC\n");
        T *new_mem = (T *)kmalloc(new_capacity, alignof(T));

        // TODO zero memory? or add option to kmalloc to zero memory
        for(u32 i = 0; i < length; ++i)
            new_mem[i] = mem[i];

        if(mem)
            kfree((vaddr)mem);

        mem = new_mem;
        capacity = new_capacity;
    }

    void unstable_remove(u32 index)
    {
        ASSERT(length > 0);
        ASSERT(index < length);
        mem[index].~T();
        mem[index] = mem[length-1];
        length--;
    }

    T& operator[](size_t index)
    {
        ASSERT(length > 0);
        ASSERT(index < length);
        return mem[index];
    }

    const T& operator[](size_t index) const
    {
        ASSERT(length > 0);
        ASSERT(index < length);
        return mem[index];
    }
};
