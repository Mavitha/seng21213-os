SENG21213-OS
============

A custom, bare-metal 32-bit operating system built for SENG21213. This project features a custom bootloader, an interrupt-driven keyboard, a thread scheduler, physical memory management, and a hierarchical RAM disk file system.

Quick Start
-----------

To build the OS and launch it in the QEMU emulator:

Bash

Plain textANTLR4BashCC#CSSCoffeeScriptCMakeDartDjangoDockerEJSErlangGitGoGraphQLGroovyHTMLJavaJavaScriptJSONJSXKotlinLaTeXLessLuaMakefileMarkdownMATLABMarkupObjective-CPerlPHPPowerShell.propertiesProtocol BuffersPythonRRubySass (Sass)Sass (Scss)SchemeSQLShellSwiftSVGTSXTypeScriptWebAssemblyYAMLXML`   make clean && make run   `

## Features to Test
* **VGA Scrollback Buffer:** Press **Page Up (Fn + Arrow Up) ** and **Page Down (Fn + Arrow Down) ** to scroll through the terminal history.
* **Hierarchical RAM Disk:** A 1 MB in-memory block device featuring an inode-based file system. Includes isolated directories, a dynamic CWD shell prompt (`ksh:dir>`), and file creation.
* **Thread Synchronization:** Run the `race`, `race_mutex`, and `prodcons` commands to test the kernel's scheduler, blocking mutexes, and counting semaphores.

## Command Reference

### General & System
| Command | Description |
| :--- | :--- |
| `help` | Show the help message |
| `clear` | Clear the VGA screen |
| `about` | About this OS and course |
| `echo <text>` | Echo text to the screen |
| `mem` | Display memory map (stub) |
| `free` | Show free memory via the Physical Memory Manager (PMM) |

### Process & Thread Management
| Command | Description |
| :--- | :--- |
| `ps` | List running processes |
| `kill <pid>` | Terminate a process |
| `threads` | List kernel threads |
| `race` | Run the standard race condition test |
| `race_mutex` | Run the mutex synchronization test |
| `prodcons` | Run the producer-consumer semaphore test |

### File System (RAM Disk)
| Command | Description |
| :--- | :--- |
| `pwd` | Print working directory |
| `ls` | List files and directories in the current directory |
| `cd <dir>` | Change the current directory |
| `mkdir <dir>` | Create a new directory |
| `touch <file>` | Create a new empty file |
| `write <file> <text>` | Write text to a file |
| `cat <file>` | Print file contents to the screen |
| `rm <file>` | Remove a file |