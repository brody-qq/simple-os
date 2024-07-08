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

    auto stat = sys_stat(path);
    if(stat.found_file) {
        prog_error(argv[0], "file already exists");
        return 1;
    }

    int result = sys_fs_create_dir(path);
    if(result != FS_STATUS_OK) {
        prog_error(argv[0], "error creating directory");
        return 1;
    }
}
