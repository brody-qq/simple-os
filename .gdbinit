
file build/boot.elf
target remote localhost:1234
break _start
#break kernel_main()

#break user_process_start()
#break vspace.cpp:374
#break vspace.cpp:368
#break vspace.cpp:547
#break scheduler.cpp:82
#break vspace.cpp:554
#break cpu.cpp:387
#break sys_alloc(u64, u64)
#break vspace.cpp:544
#break vspace.cpp:400
break cpu.cpp:987

#break user_process_start()
#break tty.cpp:113

layout split src asm

