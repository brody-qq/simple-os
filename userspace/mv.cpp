#include "include/syscall.h"
#include "include/string.h"
#include "include/types.h"
#include "include/math.h"

int main(int argc, char **argv)
{
    if(argc != 3) {
        usage_error(argv[0], "<source_path> <dest_path>");
        return 1;
    }

    char *src = argv[1];
    char *dst = argv[2];

    auto stat = sys_stat(src);
    if(!stat.found_file) {
        prog_error(argv[0], "source file doesn't exist");
        return 1;
    }

    int result = sys_fs_mv(src, dst);
    if(result != FS_STATUS_OK) {
        prog_error(argv[0], "error moving file");
        return 1;
    }

    return 0;
}
