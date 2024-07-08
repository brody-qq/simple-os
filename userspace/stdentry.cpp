#include "include/types.h"
#include "include/syscall.h"

typedef int (*entry_fn)(int, char **);

int main(int argc, char **argv, entry_fn entry)
{
    [[maybe_unused]] int returncode = entry(argc, argv);

    sys_exit();

}
