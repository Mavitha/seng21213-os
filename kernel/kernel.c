/* =============================================================================
 * SENG21213-OS :: Main Kernel  (Stage 0 - Foundations)
 * File   : kernel/kernel.c
 *
 * PURPOSE
 *   This is the heart of your operating system. Right now it:
 *     1. Initialises VGA text-mode display
 *     2. Initialises the keyboard driver
 *     3. Prints a splash screen
 *     4. Runs a minimal interactive shell ("ksh")
 *
 * ASSIGNMENT MILESTONES  (what YOU will add in later lectures)
 *   Lecture  9  - Process Management  →  process.h / process.c / scheduler.c
 *   Lecture 10  - Threads             →  thread.h  / thread.c
 *   Lecture 11  - Memory Management   →  pmm.h     / pmm.c / vmm.c
 *   Lecture 12  - File System         →  fs.h      / fs.c
 *
 * CODING CONVENTIONx
 *   - Prefix kernel-internal functions with k_ (e.g. k_strcmp)
 *   - All driver APIs live in their own .h/.c pair
 *   - NEVER call malloc - use the PMM you build in Lecture 11
 * ============================================================================*/

#include "vga.h"
#include "keyboard.h"
#include "../include/types.h"
#include "../include/process.h"
#include "../include/thread.h"
#include "../include/mutex.h"
#include "../include/semaphore.h"
#include "../include/pmm.h"
#include "../include/fs.h"

#define ITERS 100000
#define BUF_SIZE 8

extern void pit_init(void);
extern void idt_init(void);
extern tcb_t thread_table[16];
/* ---------------------------------------------------------------------------
 * Forward declarations of shell commands
 * --------------------------------------------------------------------------*/
static void cmd_help(void);
static void cmd_clear(void);
static void cmd_about(void);
static void cmd_echo(const char *args);
static void cmd_mem(void);
static void cmd_ps(void);
static volatile int myglobal = 0;
static mutex_t lock;
static int buffer[BUF_SIZE];
static int in_idx = 0;
static int out_idx = 0;
static semaphore_t sem_empty, sem_full, sem_mutex;

/* ---------------------------------------------------------------------------
 * Utility: minimal string helpers (no libc in a freestanding kernel!)
 * --------------------------------------------------------------------------*/
static int k_strcmp(const char *a, const char *b)
{
    while (*a && (*a == *b))
    {
        a++;
        b++;
    }
    return (uint8_t)*a - (uint8_t)*b;
}

// static int k_strncmp(const char *a, const char *b, size_t n)
// {
//     while (n-- && *a && (*a == *b))
//     {
//         a++;
//         b++;
//     }
//     return n == (size_t)-1 ? 0 : (uint8_t)*a - (uint8_t)*b;
// }

static size_t k_strlen(const char *s)
{
    size_t n = 0;
    while (s[n])
        n++;
    return n;
}

/* Skip leading spaces */
static const char *k_ltrim(const char *s)
{
    while (*s == ' ')
        s++;
    return s;
}

static int k_atoi(const char *str)
{
    int res = 0;
    for (int i = 0; str[i] != '\0'; ++i)
    {
        if (str[i] >= '0' && str[i] <= '9')
        {
            res = res * 10 + (str[i] - '0');
        }
    }
    return res;
}

/* ---------------------------------------------------------------------------
 * Splash Screen
 * --------------------------------------------------------------------------*/
static void print_splash(void)
{
    vga_clear(VGA_BLACK);

    /* Top banner box */
    vga_draw_box(0, 0, 7, 80, VGA_LIGHT_MAGENTA);

    vga_set_cursor(1, 2);
    vga_puts_color("  SENG21213-OS  |  Computer Architecture & Operating Systems",
                   VGA_YELLOW, VGA_BLACK);

    vga_set_cursor(2, 2);
    vga_puts_color("  Stage 0: Kernel Foundations", VGA_LIGHT_CYAN, VGA_BLACK);

    vga_set_cursor(3, 2);
    vga_puts_color("  Faculty of Engineering - Department of Software Engineering",
                   VGA_LIGHT_GREY, VGA_BLACK);

    vga_set_cursor(4, 2);
    vga_puts_color("  Built by students, for students.  Type 'help' to begin.",
                   VGA_LIGHT_GREEN, VGA_BLACK);

    vga_set_cursor(5, 2);
    vga_puts_color("  CPU: i686 (32-bit Protected Mode)  |  Display: VGA 80x25",
                   VGA_DARK_GREY, VGA_BLACK);

    vga_set_cursor(8, 0);
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_puts("  Welcome! This kernel was compiled from source and booted entirely\n");
    vga_puts("  from bare metal. There is no Linux or Windows underneath - only\n");
    vga_puts("  the code you and your team write.\n");
    vga_puts("\n");
    vga_puts("  Assignment milestones to implement:\n");
    vga_puts_color("    [L09] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("Process Management  - PCB, ready queue, round-robin scheduler\n");
    vga_puts_color("    [L10] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("Threads & Sync      - kernel threads, mutex, semaphore\n");
    vga_puts_color("    [L11] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("Memory Management   - physical page allocator, virtual memory\n");
    vga_puts_color("    [L12] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("File System         - RAM disk, FAT-like directory structure\n");
    vga_puts("\n");
}

void race_thread_a(void)
{
    __asm__ volatile("sti");
    for (int i = 0; i < ITERS; i++)
    {
        mutex_lock(&lock);
        myglobal++;
        mutex_unlock(&lock);
    }
    vga_printf("A done. myglobal = %d\n", myglobal);
    while (true)
        ;
}

void race_thread_b(void)
{
    __asm__ volatile("sti");
    for (int i = 0; i < ITERS; i++)
    {
        mutex_lock(&lock);
        myglobal++;
        mutex_unlock(&lock);
    }
    vga_printf("B done. myglobal = %d\n", myglobal);
    while (true)
        ;
}

void producer_thread(void)
{
    __asm__ volatile("sti");
    for (int i = 0; i < 20; i++)
    {
        sem_wait(&sem_empty);
        sem_wait(&sem_mutex);

        buffer[in_idx] = i;
        vga_printf("Produced: %d\n", i);
        in_idx = (in_idx + 1) % BUF_SIZE;

        sem_signal(&sem_mutex);
        sem_signal(&sem_full);
    }
    while (true)
        ;
}

void consumer_thread(void)
{
    __asm__ volatile("sti");
    for (int i = 0; i < 20; i++)
    {
        sem_wait(&sem_full);
        sem_wait(&sem_mutex);

        int item = buffer[out_idx];
        vga_printf("Consumed: %d\n", item);
        out_idx = (out_idx + 1) % BUF_SIZE;

        sem_signal(&sem_mutex);
        sem_signal(&sem_empty);
    }
    while (true)
        ;
}

/* ---------------------------------------------------------------------------
 * Shell command implementations
 * --------------------------------------------------------------------------*/
static void cmd_help(void)
{
    vga_puts_color("\n  SENG21213-OS Shell Commands\n", VGA_YELLOW, VGA_BLACK);
    vga_puts("  -----------------------------------------\n");
    vga_puts("  help    - Show this help message\n");
    vga_puts("  clear   - Clear the screen\n");
    vga_puts("  about   - About this OS and course\n");
    vga_puts("  echo    - Echo text to screen\n");
    vga_puts("  mem     - Memory map (stub)\n");
    vga_puts("  ps      - [L09] List processes\n");
    vga_puts("  kill    - [L09] Terminate a process\n");
    vga_puts("  threads - [L10] List kernel threads\n");
    vga_puts("  race    - [L10] Run race condition test\n");
    vga_puts("  race_mutex - [L10] Run mutex test\n");
    vga_puts("  prodcons - [L10] Run producer-consumer test\n");
    vga_puts("  free    - [L11] Show free memory\n");
    vga_puts("  ls      - [L12] List files\n");
    vga_puts("  cat     - [L12] Print file contents\n");
    vga_puts("  write   - [L12] Write to a file\n");
    vga_puts("  rm      - [L12] Remove a file\n");
    vga_puts("  touch   - [L12] Create a new file\n");
    vga_puts("  mkdir   - [L12] Create a new directory\n");
    vga_puts("  cd      - [L12] Change current directory\n");
    vga_puts("  pwd     - [L12] Print working directory\n");
}

static void cmd_clear(void)
{
    vga_clear(VGA_BLACK);
}

static void cmd_about(void)
{
    vga_puts_color("\n  About SENG21213-OS\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ---------------------------------------------\n");
    vga_puts("  Architecture : x86 (i686), 32-bit Protected Mode\n");
    vga_puts("  Bootloader   : Custom MBR (NASM)\n");
    vga_puts("  Kernel       : Freestanding C (GCC, no libc)\n");
    vga_puts("  VM Target    : QEMU (qemu-system-i386)\n");
    vga_puts("  Course       : SENG 21213 - Sem 2\n");
    vga_puts("  Reference    : Stallings, OS: Internals & Design Principles\n\n");
}

static void cmd_echo(const char *args)
{
    vga_puts("  ");
    vga_puts(args);
    vga_puts("\n");
}

static void cmd_mem(void)
{
    vga_puts_color("\n  Memory Map (stub - implement PMM in Lecture 11)\n",
                   VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  ---------------------------------------------\n");
    vga_puts("  0x00000000 - 0x000FFFFF  :  First 1 MB (reserved/BIOS)\n");
    vga_puts("  0x00100000 - 0x00EFFFFF  :  Extended memory (usable ~14 MB)\n");
    vga_puts("  0x00F00000 - 0x00FFFFFF  :  BIOS / ROM area\n");
    vga_puts("  0xB8000    - 0xBFFFF     :  VGA frame buffer\n");
    vga_puts_color("\n  TODO: Use BIOS int 0x15, EAX=0xE820 to get real memory map\n\n",
                   VGA_YELLOW, VGA_BLACK);
}

static void cmd_ps(void)
{

    vga_printf("PID   NAME        STATE       TICKS\n");
    vga_puts("----  ----------  ----------  -----\n");

    for (int i = 0; i < 16; i++)
    {
        if (proc_table[i].state != PROC_UNUSED)
        {
            const char *state_str = "UNKNOWN";
            switch (proc_table[i].state)
            {
            case PROC_READY:
                state_str = "READY  ";
                break;
            case PROC_RUNNING:
                state_str = "RUNNING";
                break;
            case PROC_BLOCKED:
                state_str = "BLOCKED";
                break;
            case PROC_ZOMBIE:
                state_str = "ZOMBIE ";
                break;
            default:
                break;
            }
            vga_printf("%d    %s          %s\n",
                       i, proc_table[i].name, state_str);
        }
    }
}

static void cmd_race(void)
{
    myglobal = 0;
    vga_puts("Starting race condition test (Expected: 200000)...\n");
    thread_create(1, "r_a", race_thread_a);
    thread_create(1, "r_b", race_thread_b);
}

static void cmd_race_mutex(void)
{
    myglobal = 0;
    mutex_init(&lock);
    vga_puts("Starting mutex test (Expected: 200000)...\n");
    thread_create(0, "m_a", race_thread_a);
    thread_create(0, "m_b", race_thread_b);
}

static void cmd_prodcons(void)
{
    vga_puts("Starting Producer-Consumer test...\n");
    sem_init(&sem_empty, BUF_SIZE);
    sem_init(&sem_full, 0);
    sem_init(&sem_mutex, 1);
    in_idx = 0;
    out_idx = 0;

    thread_create(0, "prod", producer_thread);
    thread_create(0, "cons", consumer_thread);
}

static void cmd_kill(const char *cmd)
{

    if (cmd[4] != ' ' || cmd[5] == '\0')
    {
        vga_puts("Usage: kill <pid>\n");
        return;
    }

    int pid = k_atoi(&cmd[5]);

    if (pid < 0 || pid >= MAX_PROCS)
    {
        vga_puts("Error: Invalid PID.\n");
        return;
    }

    if (proc_table[pid].state == PROC_UNUSED)
    {
        vga_puts("Error: Process is already dead or unused.\n");
        return;
    }

    proc_table[pid].state = PROC_ZOMBIE;
    for (int i = 0; i < 16; i++)
    {
        if (thread_table[i].state != PROC_UNUSED && thread_table[i].pid == (uint32_t)pid)
        {
            thread_table[i].state = PROC_ZOMBIE;
        }
    }

    vga_printf("Process %d and its threads killed.\n", pid);
}

static void cmd_threads(void)
{
    vga_puts("TID  PID  NAME          STATE\n");
    vga_puts("---- ---- ------------- -------\n");

    for (int i = 0; i < 16; i++)
    {
        if (thread_table[i].state != PROC_UNUSED)
        {
            const char *state_str = "UNKNOWN";
            switch (thread_table[i].state)
            {
            case PROC_READY:
                state_str = "READY  ";
                break;
            case PROC_RUNNING:
                state_str = "RUNNING";
                break;
            case PROC_BLOCKED:
                state_str = "BLOCKED";
                break;
            case PROC_ZOMBIE:
                state_str = "ZOMBIE ";
                break;
            default:
                break;
            }
            vga_printf("%d    %d    %s          %s\n",
                       thread_table[i].tid,
                       thread_table[i].pid,
                       thread_table[i].name,
                       state_str);
        }
    }
}

static void cmd_meminfo(void)
{

    uint32_t free_kb = pmm_free_frames() * 4;
    uint32_t total_kb = pmm_total_frames() * 4;
    uint32_t used_kb = total_kb - free_kb;

    vga_printf("Free: %d KB  Used: %d KB  Total: %d KB\n", free_kb, used_kb, total_kb);
}
static void cmd_ls(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    inode_t inodes[MAX_INODES];
    int n = fs_ls(inodes, MAX_INODES);

    vga_puts("TYPE   NAME                     SIZE\n");
    vga_puts("------ ------------------------ --------\n");

    for (int i = 0; i < n; i++)
    {
        /* Print the Type tag */
        const char *type_str = (inodes[i].type == 2) ? "<DIR>" : "FILE ";
        vga_printf("%s  %s", type_str, inodes[i].name);

        int len = k_strlen(inodes[i].name);
        for (int j = len; j < 25; j++)
        {
            vga_putchar(' ');
        }

        vga_printf("%u\n", inodes[i].size);
    }
    vga_printf("%d item(s)\n", n);
}

static void cmd_write(int argc, char *argv[])
{
    if (argc < 3)
    {
        vga_puts("Usage: write <file> <text>\n");
        return;
    }
    int fd = fs_open(argv[1], O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0)
    {
        vga_puts("Cannot open file\n");
        return;
    }
    for (int i = 2; i < argc; i++)
    {
        fs_write(fd, argv[i], k_strlen(argv[i]));
        if (i < argc - 1)
            fs_write(fd, " ", 1);
    }
    fs_close(fd);
}

static void cmd_touch(int argc, char *argv[])
{
    if (argc < 2)
    {
        vga_puts("Usage: touch <file>\n");
        return;
    }
    int fd = fs_open(argv[1], O_RDONLY | O_CREAT);
    if (fd >= 0)
        fs_close(fd);
}

static void cmd_cat(int argc, char *argv[])
{
    if (argc < 2)
    {
        vga_puts("Usage: cat <file>\n");
        return;
    }
    int fd = fs_open(argv[1], O_RDONLY);
    if (fd < 0)
    {
        vga_puts("File not found.\n");
        return;
    }

    char buf[64];
    int bytes;
    while ((bytes = fs_read(fd, buf, sizeof(buf) - 1)) > 0)
    {
        buf[bytes] = '\0';
        vga_puts(buf);
    }
    vga_putchar('\n');
    fs_close(fd);
}

static void cmd_rm(int argc, char *argv[])
{
    if (argc < 2)
    {
        vga_puts("Usage: rm <file>\n");
        return;
    }
    if (fs_unlink(argv[1]) == 0)
        vga_puts("File deleted.\n");
    else
        vga_puts("File not found.\n");
}

static void cmd_pwd(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    vga_puts(fs_get_cwd());
    vga_putchar('\n');
}

static void cmd_mkdir(int argc, char *argv[])
{
    if (argc < 2)
    {
        vga_puts("Usage: mkdir <dir>\n");
        return;
    }
    if (fs_mkdir(argv[1]) == 0)
        vga_puts("Directory created.\n");
    else
        vga_puts("Failed to create directory.\n");
}

static void cmd_cd(int argc, char *argv[])
{
    if (argc < 2 || k_strcmp(argv[1], "/") == 0)
    {
        fs_set_cwd("/");
        return;
    }
    if (fs_is_dir(argv[1]))
    {
        fs_set_cwd(argv[1]);
    }
    else
    {
        vga_puts("cd: no such directory\n");
    }
}

/* ---------------------------------------------------------------------------
 * Shell process
 * --------------------------------------------------------------------------*/
static char shell_buf[256];

static void shell_run(void)
{
    vga_puts_color("\n  Kernel Shell ready. Type 'help' for commands.\n",
                   VGA_LIGHT_GREEN, VGA_BLACK);

    while (true)
    {

        vga_puts_color("ksh:", VGA_LIGHT_GREEN, VGA_BLACK);
        vga_puts_color(fs_get_cwd(), VGA_LIGHT_BLUE, VGA_BLACK);
        vga_puts_color("> ", VGA_LIGHT_GREEN, VGA_BLACK);
        kb_readline(shell_buf, sizeof(shell_buf));

        char *raw_cmd = (char *)k_ltrim(shell_buf);
        if (k_strlen(raw_cmd) == 0)
            continue;

        char cmd_backup[128];
        char *dst = cmd_backup;
        const char *src = raw_cmd;
        while ((*dst++ = *src++))
            ;

        char *argv[16];
        int argc = 0;
        char *p = raw_cmd;

        while (*p)
        {
            while (*p == ' ')
                *p++ = '\0';
            if (*p)
            {
                if (argc < 16)
                    argv[argc++] = p;
                while (*p && *p != ' ')
                    p++;
            }
        }

        if (argc == 0)
            continue;

        if (k_strcmp(argv[0], "help") == 0)
        {
            cmd_help();
            continue;
        }
        if (k_strcmp(argv[0], "clear") == 0)
        {
            cmd_clear();
            continue;
        }
        if (k_strcmp(argv[0], "about") == 0)
        {
            cmd_about();
            continue;
        }
        if (k_strcmp(argv[0], "mem") == 0)
        {
            cmd_mem();
            continue;
        }
        if (k_strcmp(argv[0], "ps") == 0)
        {
            cmd_ps();
            continue;
        }
        if (k_strcmp(argv[0], "race") == 0)
        {
            cmd_race();
            continue;
        }
        if (k_strcmp(argv[0], "race_mutex") == 0)
        {
            cmd_race_mutex();
            continue;
        }
        if (k_strcmp(argv[0], "prodcons") == 0)
        {
            cmd_prodcons();
            continue;
        }
        if (k_strcmp(argv[0], "threads") == 0)
        {
            cmd_threads();
            continue;
        }
        if (k_strcmp(argv[0], "meminfo") == 0 || k_strcmp(argv[0], "free") == 0)
        {
            cmd_meminfo();
            continue;
        }

        if (k_strcmp(argv[0], "ls") == 0)
        {
            cmd_ls(argc, argv);
            continue;
        }
        if (k_strcmp(argv[0], "touch") == 0)
        {
            cmd_touch(argc, argv);
            continue;
        }
        if (k_strcmp(argv[0], "write") == 0)
        {
            cmd_write(argc, argv);
            continue;
        }
        if (k_strcmp(argv[0], "cat") == 0)
        {
            cmd_cat(argc, argv);
            continue;
        }
        if (k_strcmp(argv[0], "rm") == 0)
        {
            cmd_rm(argc, argv);
            continue;
        }

        if (k_strcmp(argv[0], "echo") == 0)
        {
            if (k_strlen(cmd_backup) > 5)
                cmd_echo(cmd_backup + 5);
            else
                vga_puts("\n");
            continue;
        }
        if (k_strcmp(argv[0], "kill") == 0)
        {
            cmd_kill(cmd_backup);
            continue;
        }
        if (k_strcmp(argv[0], "pwd") == 0)
        {
            cmd_pwd(argc, argv);
            continue;
        }
        if (k_strcmp(argv[0], "mkdir") == 0)
        {
            cmd_mkdir(argc, argv);
            continue;
        }
        if (k_strcmp(argv[0], "cd") == 0)
        {
            cmd_cd(argc, argv);
            continue;
        }

        vga_puts_color("  Unknown command: ", VGA_LIGHT_RED, VGA_BLACK);
        vga_puts(argv[0]);
        vga_puts("\n  Type 'help' for a list of commands.\n");
    }
}

/* ---------------------------------------------------------------------------
 * Test Processes for Stage 1
 * --------------------------------------------------------------------------*/
void proc1(void)
{
    __asm__ volatile("sti");
    while (true)
    {
    }
}

void proc2(void)
{
    __asm__ volatile("sti");
    while (true)
    {
    }
}

/* ---------------------------------------------------------------------------
 * Kernel entry point - called from kernel_entry.asm
 * --------------------------------------------------------------------------*/
void kernel_main(void)
{
    pmm_init();
    fs_init();
    vga_init();
    kb_init();

    for (int i = 0; i < MAX_PROCS; i++)
    {
        proc_table[i].state = 0;
    }

    current_proc = 0;
    proc_table[0].pid = 0;
    proc_table[0].state = PROC_RUNNING;
    proc_table[0].ticks = 0;

    proc_table[0].name[0] = 'i';
    proc_table[0].name[1] = 'd';
    proc_table[0].name[2] = 'l';
    proc_table[0].name[3] = 'e';
    proc_table[0].name[4] = '\0';

    proc_create("proc1", proc1);
    proc_create("proc2", proc2);

    pit_init();
    idt_init();

    print_splash();
    shell_run();
}
