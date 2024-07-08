#pragma once
#include "include/types.h"

void memmove_workaround(void *dest, void *src, size_t len);

void memset_workaround(void *dest, int val, size_t len);

int strncmp_workaround(const char *str1, const char *str2, size_t len);

size_t strlen_workaround(const char *str);

const char *strchr_workaround(const char *str, int c);
