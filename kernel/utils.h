#pragma once

#define ARRAY_SIZE(arr) ((sizeof((arr))) / (sizeof((arr[0]))))

#define KB 1024ull
#define MB 1024ull*KB
#define GB 1024ull*MB
#define TB 1024ull*GB
#define PB 1024ull*TB
#define EB 1024ull*PB

#define STRINGIFY(s) _STRINGIFY(s)
#define _STRINGIFY(s) #s
