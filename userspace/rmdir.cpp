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
    if(!stat.found_file) {
        prog_error(argv[0], "file doesn't exist");
        return 1;
    } else if (!stat.is_dir) {
        prog_error(argv[0], "file is not a directory");
        return 1;
    }

    int result = sys_fs_rm_dir(path);
    if(result != FS_STATUS_OK) {
        if(result == FS_STATUS_DIR_NOT_EMPTY) {
            prog_error(argv[0], "directory is not empty");
            return 1;
        } else {
            prog_error(argv[0], "error while deleting directory");
            return 1;
        }
    }
}
