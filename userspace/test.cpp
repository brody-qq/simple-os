#include "include/syscall.h"
#include "include/key_event.h"
#include "include/stdlib_workaround.h"
#include "include/string.h"

int main(int argc, char **argv)
{
    //sys_print("ELF test\n", 9);
    sys_tty_write("ELF test\n", 9);
    sys_tty_flush();

    sys_tty_write("argc: ", 6);
    write_uint(argc);
    sys_tty_write("\n", 1);
    sys_tty_flush();

    for(int i = 0; i < argc; ++i) {
        write_uint(i);
        sys_tty_write(": ", 2);
        sys_tty_write(argv[i]);
        sys_tty_write("\n", 1);
        sys_tty_flush();
    }
    /*
    while(1) {
        PollKeyboardResult res;
        sys_poll_keyboard(&res);
        if(res.has_result) {
            KeyEvent event = res.event;

            if(is_ascii_event(event)) {
                char c = keyevent_to_ascii(event);
                char str[] = {c, 0 };
                sys_print(str, 1);
            }
        }
    }
    */
   /*
   while(1) {
    sys_tty_write("blah ", 5);
    sys_tty_flush();
   }
   */
}
