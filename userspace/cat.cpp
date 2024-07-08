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
        prog_error(argv[0], "file not found");
        return 1;
    } else if(stat.is_dir) {
        prog_error(argv[0], "file is a directory");
        return 1;
    }

    TTYInfo tty_info = {};
    sys_tty_info(&tty_info);
    int offset = stat.size - tty_info.scrollback_buffer_size;
    if(offset < 0)
        offset = 0;

    int buf_size = stat.size - offset;
    char *buf = (char *)sys_alloc(buf_size+1, 64);
    int result = sys_fs_read(path, buf, offset, buf_size);
    buf[buf_size] = 0;

    if(result != FS_STATUS_OK) {
        prog_error(argv[0], "error reading file");
        return 1;
    }

    sys_tty_write(buf);
    sys_tty_flush();

    sys_free(buf);
}
