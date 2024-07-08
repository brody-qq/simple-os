#include "include/syscall.h"
#include "include/string.h"

int main(int argc, char **argv)
{
    if(argc < 3 || argc > 4) {
        usage_error(argv[0], "<path> <string> [offset]");
        return 1;
    }

    char *path = argv[1];
    char *str = argv[2];
    int offset = 0;
    
    auto stat = sys_stat(path);
    if(!stat.found_file) {
        sys_fs_create_file(path);
    } else if(stat.is_dir) {
        prog_error(argv[0], "path is a directory");
        return 1;
    }

    if (argc == 4) {
        char *offset_arg = argv[3];
        if(!str_is_num(offset_arg)) {
            prog_error(argv[0], "offset arg is not a number");
            return 1;
        }
        offset = str_to_uint(offset_arg);
    } else {
        stat = sys_stat(path); // NOTE: re-do stat() since above code may have just created file
        offset = stat.size;
    }

    int len = strlen_workaround(str);
    sys_fs_write(path, str, offset, len);

    return 0;
}
