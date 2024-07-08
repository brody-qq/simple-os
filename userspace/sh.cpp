#include "include/syscall.h"
#include "include/key_event.h"
#include "include/stdlib_workaround.h"
#include "include/math.h"
#include "include/string.h"

// NOTE: if this is a char *, then gcc makes a DYN entry in the ELF, which the ELF loading code
//       does not handle!
static const char help_msg[] =
    "AVAILABLE COMMANDS:\n" \
    "help                            -> print this help message\n" \
    "\n" \
    "echo <string...>                -> print text to screen\n" \
    "cat <path>                      -> read contents of file\n" \
    "\n" \
    "cd <path>                       -> change current working directory\n" \
    "pwd                             -> print current working directory\n" \
    "ls <path>                       -> list directory contents\n" \
    "\n" \
    "touch <path>                    -> create regular file\n" \
    "rm <path>                       -> delete regular file\n" \
    "\n" \
    "mkdir <path>                    -> create directory\n" \
    "rmdir <path>                    -> delete directory\n" \
    "\n" \
    "write <path> <string> [offset]  -> write to fixed offset in file\n" \
    "                                   append to end of file if no [offset] given\n" \
    "\n" \
    "cp <src_path> <dest_path>       -> copy file\n" \
    "mv <src_path> <dest_path>       -> rename file\n" \
    "\n\0";

bool strmatch(const char *str1, const char *str2)
{
    return (strlen_workaround(str1) == strlen_workaround(str2)) && (strncmp_workaround(str1, str2, strlen_workaround(str1)) == 0);
}

// TODO for relative paths, check current directory, followed by /userspace/
void enter_cmdline(char *cmdline, int cmdline_len)
{
    const char *bin_lookup_path = "/userspace/";
    char *argv_buf = (char *)sys_alloc(cmdline_len, 64); // TODO make version of sys_alloc with default alignment arg
    char **argv = (char **)sys_alloc(cmdline_len, 64); // TODO how to decide size of this
    int cmd_start_i = 0;
    int argv_buf_i = 0;
    int argv_i = 0;
    char *argv_buf_curr_str = argv_buf;
    bool in_quote = false;
    //sys_tty_write("cmdline len: ", 13); write_uint(cmdline_len);

    // ASSERT(cmdline[cmdline_len] == 0); // TODO make assert for userspace
    int i = 0;
    char c = 0;

    auto add_argv = [&]() {
        int src_len = i - cmd_start_i;
        if(src_len > 0) {
            char *dest = argv_buf + argv_buf_i;
            char *src = cmdline + cmd_start_i;
            int dest_j = 0;
            bool escape = false;
            for(int j=0; j < src_len; j++) {
                if(!escape && src[j] == '\\') {
                    escape = true;
                    continue;
                }
                if(!escape && src[j] == '\"') {
                    continue;
                }
                dest[dest_j++] = src[j];
                escape = false;
            }
            argv_buf_i += dest_j;
            argv_buf[argv_buf_i++] = 0;

            argv[argv_i++] = argv_buf_curr_str;
            argv_buf_curr_str = argv_buf + argv_buf_i;
        }
        cmd_start_i = i+1;
        in_quote = false;
    };

    bool is_escape = false;
    for(i = 0; i < cmdline_len; ++i) {
        c = cmdline[i];

        if(!is_escape && c == '\\') {
            is_escape = true;
            continue;
        }

        if(!is_escape && c == '\"') {
            in_quote = !in_quote;
        }

        if(!in_quote && c == ' ') {
            add_argv();
        }

        is_escape = false;
    }

    if(!in_quote) {
        add_argv();
    }

    argv[argv_i] = 0; // argv must be null terminated
    int argc = argv_i;
    u64 flags = EXEC_IS_BLOCKING;
    if(in_quote) {
        cmd_error("unclosed quote");
    } else if(argc > 0) {
        int exe_result = 0;
        if(contains(argv[0], '/')) {
            exe_result = sys_exec(argv[0], argv+1, flags);
        } else if(strmatch(argv[0], "cd")) {
            if(argc != 2) {
                usage_error(argv[0], "<path>");
            } else {
                int result = sys_set_pwd(argv[1]);
                switch(result) {
                    case SYS_BAD_PATH: {
                        prog_error(argv[0], "path is not a directory");
                    } break;
                    case SYS_FILE_NOT_FOUND: {
                        prog_error(argv[0], "path not found");
                    } break;
                }
            }
        } else if(strmatch(argv[0], "echo")) {
            if(argc < 2) {
                usage_error(argv[0], "<string>...");
            } else {
                for(int i = 1; i < argc; ++i) {
                    sys_tty_write(argv[i]);
                    sys_tty_write(" ", 1);
                    sys_tty_flush();
                }
            }
        } else if(strmatch(argv[0], "help")) {
            if(argc > 1) {
                usage_error(argv[0], "");
            } else {
                sys_tty_write(help_msg);
                sys_tty_flush();
            }
        } else {
            int path_len = strlen_workaround(bin_lookup_path);
            int arg0_len = strlen_workaround(argv[0]);
            int len = path_len + arg0_len;
            char *buf = (char *)sys_alloc(len +1, 64);
            memmove_workaround(buf, (void *)bin_lookup_path, path_len);
            memmove_workaround(buf + path_len, argv[0], arg0_len);
            buf[len] = 0;
            exe_result = sys_exec(buf, argv+1, flags);
            sys_free(buf);
        }

        // TODO return codes for sys_exec?
        switch(exe_result) {
            case SYS_BAD_PATH: {
                cmd_error("path to executable must not end with a '/'");
            } break;
            case SYS_FILE_NOT_FOUND: {
                cmd_error("file not found");
            } break;
            default: {} break;
        }
        sys_tty_write("\n", 1);
    }

    sys_free(argv_buf);
    sys_free(argv);
}

int main(int argc, char **argv)
{
    sys_print("SHELL\n", 6);

    sys_tty_write(help_msg);

    TTYInfo cmd_info;
    sys_tty_info(&cmd_info);
    u32 max_len = cmd_info.cmdline_size;
    char cmd_line[max_len + 1];
    memset_workaround(cmd_line, 0, max_len);
    // TODO if prompt becomes adjustable, code must be changed to stop a prompt
    //      that's too long from overflowing cmdline
    const char * prompt = "> ";
    u32 prompt_len = strlen_workaround(prompt);

    u32 cmd_start_i = 0;
    u32 cmd_onepastend = 0;
    u32 cursor_i = 0;
    auto reset_cmd_line = [&]() {
        cmd_start_i = prompt_len;
        cmd_onepastend = cmd_start_i;
        cursor_i = cmd_start_i;

        memmove_workaround(cmd_line, (void *)prompt, prompt_len);
        memset_workaround(cmd_line + prompt_len, 0, max_len - prompt_len);
        sys_tty_set_cursor(cursor_i);
        sys_tty_set_cmdline(cmd_line);

        sys_tty_flush();
    };

    reset_cmd_line();
    while(1) {
        PollKeyboardResult res;
        sys_poll_keyboard(&res);
        if(res.has_result) {
            KeyEvent e = res.event;
            if(e.pressed) {
                switch(e.key) {
                    case KEY_PAGE_UP: {
                        sys_tty_scroll(1);
                        sys_tty_flush();
                    } break;
                    case KEY_PAGE_DOWN: {
                        sys_tty_scroll(-1);
                        sys_tty_flush();
                    } break;
                    case KEY_LEFT: {
                        if(cursor_i > cmd_start_i) {
                            cursor_i--;
                            sys_tty_set_cursor(cursor_i);
                            sys_tty_flush();
                        }
                    } break;
                    case KEY_RIGHT: {
                        if(cursor_i < cmd_onepastend) {
                            cursor_i++;
                            sys_tty_set_cursor(cursor_i);
                            sys_tty_flush();
                        }
                    } break;
                    case KEY_BACKSPACE: {
                        if(cursor_i > cmd_start_i) {
                            cursor_i--;
                            cmd_onepastend--;
                            u32 len = max_len - cursor_i -1;
                            memmove_workaround(cmd_line + cursor_i, cmd_line + cursor_i+1, len);
                            cmd_line[max_len-1] = 0;
                            sys_tty_set_cursor(cursor_i);
                            sys_tty_set_cmdline(cmd_line);
                            sys_tty_flush();
                        }
                    } break;
                    case KEY_ENTER: {
                        sys_tty_flush_cmdline();
                        enter_cmdline(cmd_line + cmd_start_i, cmd_onepastend - cmd_start_i);
                        reset_cmd_line();
                        sys_tty_set_cmdline(cmd_line);
                        sys_tty_flush();
                    } break;

                    default: {
                        if(is_ascii_event(e)) {
                            if(cursor_i < max_len) {
                                char c = keyevent_to_ascii(e);
                                u32 len = max_len - cursor_i -1;
                                memmove_workaround(cmd_line + cursor_i + 1, cmd_line + cursor_i, len);
                                cmd_line[cursor_i] = c;
                                cmd_onepastend = min(cmd_onepastend+1, max_len);
                                cursor_i = min(cursor_i+1, max_len);
                                sys_tty_set_cursor(cursor_i);
                                sys_tty_set_cmdline(cmd_line);
                                sys_tty_flush(); // TODO make a function to flush only the cmdline to vga
                            }
                        }
                    } break;
                } 
            }
        }
    }
}
