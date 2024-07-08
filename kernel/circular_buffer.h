#pragma once
#include "include/types.h"

template<typename T, u32 Size>
struct CircularBuffer
{
    void push_end(T obj)
    {
        arr[end_i] = obj;
        end_i = (end_i + 1) % Size;
        if(end_i == start_i)
            start_i = (end_i + 1) % Size;
    }

    /*
    TODO this is probably a more optimized way of doing
         TTY:write_str(), pass the function that invalidates
         the str_ptr entries as callback()
         use this if have time, or TTY:write_str() is a bottleneck
         NOTE: in TTY::write_str() the char * will first have to be converted
               to a u16 *, so the memset will work
    template<typename F>
    void push_buffer(T *obj, u64 start_index, u64 len, F callback)
    {
        while(len > 0) {
            dest_i = (end_i + 1) % Size;
            u64 slots_until_buf_end = Size - dest_i;
            u64 to_copy = min(slots_until_buf_end, len);
            memmove_workaround(arr + dest_i, obj + start_index, sizeof(T) * to_copy);

            len -= to_copy;
            start_index += to_copy;

            bool should_update_start_i = (empty_slots() < to_copy);
            end_i = (end_i + to_copy) % Size;
            if(should_update_start_i) {
                u64 old_start_i = start_i;
                start_i = (end_i + 1) % Size;
                callback(old_start_i, start_i);
            }
        }
    }
    */

    struct PopResult
    {
        bool has_obj = false;
        T obj;
    };
    PopResult pop_start()
    {
        if(is_empty())
            return {false, {}};
        T obj = arr[start_i];
        start_i = (start_i + 1) % Size;
        return {true, obj};
    }

    PopResult pop_end()
    {
        if(is_empty())
            return {false, {}};
        u32 i = (end_i -1) % Size; // TODO what if end_i == 0 !!!!
        T obj = arr[i];
        end_i = i;
        return {true, obj};
    }

    PopResult peek_start()
    {
        if(is_empty())
            return {false, {}};
        T obj = arr[start_i];
        return {true, obj};

    }
    PopResult peek_end()
    {
        if(is_empty())
            return {false, {}};
        u32 i = (end_i-1) % Size;
        T obj = arr[i];
        return {true, obj};

    }

    u32 count()
    {
        u32 unrolled_end_i = end_i;
        if(end_i < start_i)
            unrolled_end_i += Size; // TODO this can overflow
        u32 count = unrolled_end_i - start_i;
        return count;
    }

    bool is_empty()
    {
        return start_i == end_i;
    }

    u32 empty_slots()
    {
        if(start_i > end_i)
            return start_i - end_i;
        return start_i + (Size - end_i);
    }

    T& operator[](size_t i)
    {
        u32 index = (start_i + i) % Size;
        ASSERT(index < Size);
        return arr[index];
    }

    // TODO make a version of push() and pop() that will block the process and wait

    T arr[Size];
    // start_i == end_i only when queue is empty
    u32 start_i = 0; 
    u32 end_i = 0; // this is one past end of contents
};
