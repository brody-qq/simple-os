#pragma once
#include "include/types.h"

const u16 FS_TYPE_DIR = 1;
const u16 FS_TYPE_REG_FILE = 2;

const u16 FS_STATUS_OK = 0;
const u16 FS_STATUS_FILE_NOT_FOUND = 1;
const u16 FS_STATUS_FILE_ALREADY_EXISTS = 2;
const u16 FS_STATUS_WRONG_FILE_TYPE = 3;
const u16 FS_STATUS_OUT_OF_SPACE = 4;
const u16 FS_STATUS_DIR_NOT_EMPTY = 5;
const u16 FS_STATUS_BAD_ARG = 5;

struct FileSizeResult {
    bool found_file = false;
    u64 size = 0;
};

struct FileStatResult
{
    bool found_file = false;
    u64 size = 0;
    bool is_dir = false;
};
