#include "include/types.h"
#include "include/syscall.h"
#include "include/stdlib_workaround.h"
#include "include/string.h"

int main(int argc, char **argv)
{
    if(argc != 2) {
        usage_error(argv[0], "<path>");
        return 1;
    }
    char *path = argv[1];
    int len = strlen_workaround(path);
    if(argv[1][len-1] == '/') {
        prog_error(argv[0], "path is a directory path");
        return 1;
    }

    auto stat = sys_stat(path);
    if(stat.found_file) {
        prog_error(argv[0], "file already exists");
        return 1;
    }

    int result = sys_fs_create_file(path);
    if(result != FS_STATUS_OK) {
        prog_error(argv[0], "error while creating file");
        return 1;
    }
}
