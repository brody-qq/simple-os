#include "include/syscall.h"
#include "include/string.h"
#include "include/types.h"
#include "include/math.h"

int main(int argc, char **argv)
{
    char *arg_path;
    bool should_free = false;
    if(argc == 1) {
        int len = sys_pwd_length();
        arg_path = (char *)sys_alloc(len+1, 64);
        sys_get_pwd(arg_path, len);
        arg_path[len] = 0;
        should_free = true;
    } else if(argc == 2) {
        arg_path = argv[1];
    } else {
        usage_error(argv[0], "[path]");
        return 1;
    }

    auto res = sys_stat(arg_path);
    if(!res.found_file) {
        prog_error(argv[0], "path not found");
        return 1;
    } else if(!res.is_dir) {
        prog_error(argv[0], "path is not a directory");
        return 1;
    }

    int buf_size = sys_list_dir_buf_size(arg_path);
    char *buf = (char *)sys_alloc(buf_size, 64);
    char *buf_one_past_end = buf;
    sys_list_dir(arg_path, buf, buf_size, &buf_one_past_end);

    char *ptr = buf;
    int max_filename = 0;
    while(ptr < buf_one_past_end) {
        int len = strlen_workaround(ptr);
        max_filename = max(max_filename, len);

        ptr += len+1;
    }
    int padding = 3;
    max_filename += padding;

    ptr = buf;
    while(ptr < buf_one_past_end) {
        int full_path_len = strlen_workaround(arg_path) + strlen_workaround(ptr) +1; // +1 is for the '/' between the 2 paths
        char *full_path = (char *)sys_alloc(full_path_len+1, 64);
        resolve_path(arg_path, ptr, full_path, full_path_len+1);

        int len = strlen_workaround(ptr);
        auto stat = sys_stat(full_path);
        const char *type_str = stat.is_dir ? "dir" : "file";
        int max_type_str_len = 4 + padding;
        int type_str_len = strlen_workaround(type_str); // TODO this length is known statically

        sys_tty_write(ptr, len);
        for(int i = 0; i < max_filename - len; ++i)
            sys_tty_write(" ", 1);
        sys_tty_write(type_str);
        for(int i = 0; i < max_type_str_len - type_str_len; ++i)
            sys_tty_write(" ", 1);
        write_uint(stat.size);
        sys_tty_write("\n", 1);

        ptr += len+1;
        sys_free(full_path);
    }
    sys_tty_flush();

    if(should_free)
        sys_free(arg_path);
    sys_free(buf);

    return 0;
}
