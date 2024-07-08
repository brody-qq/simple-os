#include "include/syscall.h"
#include "include/string.h"

int main(int argc, char **argv)
{
    if(argc != 3) {
        usage_error(argv[0], "<source_path> <dest_path>");
        return 1;
    }
    
    char *src = argv[1];
    char *dest = argv[2];

// NOTE checking if the src and dest inodes are the same will not work since
//      this program should create an entirely new, separate copy, so if both
//      files refer to the same inode it still makes a new copy
    if(sys_is_same_path(src, dest)) {
        prog_error(argv[0], "source and dest are the same");
        return 1;
    }

    auto src_stat = sys_stat(src);
    if(!src_stat.found_file) {
        prog_error(argv[0], "source file not found");
        return 1;
    }
    if(src_stat.is_dir) {
        prog_error(argv[0], "source file is a directory");
        return 1;
    }

    if(sys_is_dir_path(dest)) {
        prog_error(argv[0], "dest file is a directory");
        return 1;
    }

    auto dest_stat = sys_stat(dest);
    if(dest_stat.found_file) {
        sys_fs_truncate(dest, src_stat.size);
    } else {
        sys_fs_create_file(dest);
    }

    const int BUF_SIZE = 4096;
    char buf[BUF_SIZE];
    int remaining = src_stat.size;
    int offset = 0;
    while(remaining > 0) {
        int size = min(BUF_SIZE, remaining);
        sys_fs_read(src, buf, offset, size);
        sys_fs_write(dest, buf, offset, size);

        remaining -= size;
        offset += size;
    }

    return 0;
}
