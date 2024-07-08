## INTRO

I developed and tested this OS on Ubuntu 22.04. I did not try to make this platform independent, so it will likely only work on Ubuntu 22.04.

Considering slight variations in OS versions, and variations in the dependencies required to build and run the OS, it is possible that the OS will not build or run on your machine even if you're running Ubuntu 22.04. For example, I already ran into a problem where I updated some packages, and then I started getting a compiler error that I wasn't getting previously.

The steps for building and running my project are:

1: Use a computer running Ubuntu 22.04 (It may work on other versions of Ubuntu but I have not tested it)

2: Use `git clone` to clone this repo onto your computer, or extract the zip file and go to the directory `supporting/source_code/simple-os/`.

3: Set your current directory to the base directory of the project (the directory that contains the file `ensure-toolchain.sh`).

4: Run the command `sudo bash package_installs.sh`. This installs software that the OS and build system depend on. Note that this may take a while.

5: Run the command `bash ensure-toolchain.sh`. This builds a GCC cross-compiler. Note that this may take a while.

6: Run the command `sudo bash img_build.sh`. This builds an OS image.

7: Run the command `sudo bash run.sh` to run the QEMU virtual machine with the OS image (QEMU is one of the dependencies installed earlier).

8: The OS should now be running in a QEMU window. See the section 'USING THE OS' below for more info on how to use the OS.

Note that many of the shell scripts require super user permissions:

* The build and run scripts use `mount` to create and populate a filesystem image for the OS. `mount` requires super user permissions

* `package_installs.sh` runs `apt install` to install the dependencies. `apt install` requires super user permissions

---

## USING THE OS

When you run the `run.sh` script, a QEMU console will pop up running the OS.

The first thing you will see is the GRUB bootloader menu. Press enter to select the entry called 'os'. This will begin running the OS.

After this, some lines of text will be printed to screen, these are messages printed by the OS during its' initialization code.

When the OS finishes initialization, it will switch to running the shell program.

The shell program will print a help message to screen that lists all the available commands. You can begin typing commands into the shell at this point. You can type 'help' to see the help message again.

Shell key bindings:

* Press alpha-numeric keys to type into the command line

* Press enter to run the command

* Press backspace to delete the character before the cursor

* Use the left and right arrow keys to move the cursor

* Press the page up or page down keys to scroll the terminal output

Note: In the command line, a space is a separator between arguments. If you want to put spaces in an argument, put the argument inside double quotes. For example:

* The command `echo a b c d`, will run `echo` with the 4 strings `a`, `b`, `c` and `d` as arguments.

* The command `echo "a b c d"`, will run `echo` with the 1 string `a b c d` as an argument.

* Quotes can be escaped, so `\"` will be treated as a literal `"` character, instead of being interpreted by the argument parsing code.

---

## SETUP

NOTE: both of these scripts can be time consuming, especially `ensure-toolchain.sh` since it has to build GCC

install OS package dependencies:

`sudo bash package_installs.sh`

download & build toolchain:

`bash ensure-toolchain.sh`

---

## BUILD & RUN

build OS image:

`sudo bash img_build.sh`

run OS in QEMU:

`sudo bash run.sh`

---

## DEBUG BUILD & RUN

build OS image with debug info:

`sudo bash img_build.sh --debug`

run OS and debug with GDB (run both of these commands in separate terminal windows):

`sudo bash debug.sh`

`gdb`

NOTE:

* I have a custom .gdbinit in the base project directory

* the kernel will output debug information to log files in the current directory

