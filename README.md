# WengShiOS

A simple x86_64 kernel with intention to be simple and effective (it is neither simple nor effective at this time)

## Features

- Can boot into VGA text mode
- Pretty solid memory management.
- Kernel space shell containing basic commands to control and debug kernel and filesystem.
- Ring 3 space.
- 7 basic syscalls.
- ELF loader capable of loading static programs into userspace, memory management for individual tasks is still WIP.
- Basic userspace unix-like utilities(ls, cat, echo). WIP.
- Full AHCI read/write support with ext2 filesystem.
- Tested only on QEMU.

## Running it yourself

You can clone this repo by:
```bash
git clone https://github.com/weng-shi/WengShiOS.git
```
### Building dependencies
You need x86_64-elf-gcc GNU Compiler Collection toolchain, x86_64-elf-binutils, CMake and GRUB tools to build this project.

I am really sorry for everyone reading source code, and I can assure you that I will add some documentation soon.

## Licence

This project is licensed under the GNU General Public License v3.0.

You may use, modify and redistribute this software under the terms of the GPLv3.
See the `LICENCE` file for the full license text. 
