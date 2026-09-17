#include "../include/fs.h"
#include "../include/ramdisk.h"

#define MAX_FDS 16
#define DATA_BLOCK_START 515

typedef struct
{
    int in_use;
    int flags;
    uint32_t inode_num;
    uint32_t offset;
    inode_t inode;
} file_desc_t;

static file_desc_t fd_table[MAX_FDS];
static superblock_t sb;
static char current_dir[28] = "/";

static void fs_memset(void *dst, uint8_t val, uint32_t count)
{
    uint8_t *d = (uint8_t *)dst;
    while (count--)
        *d++ = val;
}
static void fs_memcpy(void *dst, const void *src, uint32_t count)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (count--)
        *d++ = *s++;
}
static int fs_strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}
static void fs_strcpy(char *dst, const char *src)
{
    while ((*dst++ = *src++))
        ;
}

void fs_set_cwd(const char *dir) { fs_strcpy(current_dir, dir); }
const char *fs_get_cwd(void) { return current_dir; }

static void read_inode(uint32_t inum, inode_t *inode)
{
    uint8_t buf[BLOCK_SIZE];
    uint32_t block = 3 + (inum / 2);
    uint32_t offset = (inum % 2) * 256;
    ramdisk_read(block, buf);
    fs_memcpy(inode, buf + offset, sizeof(inode_t));
}

static void write_inode(uint32_t inum, inode_t *inode)
{
    uint8_t buf[BLOCK_SIZE];
    uint32_t block = 3 + (inum / 2);
    uint32_t offset = (inum % 2) * 256;
    ramdisk_read(block, buf);
    fs_memcpy(buf + offset, inode, sizeof(inode_t));
    ramdisk_write(block, buf);
}

static int alloc_bit(uint32_t block_idx, uint32_t max_bits)
{
    uint8_t buf[BLOCK_SIZE];
    ramdisk_read(block_idx, buf);
    for (uint32_t i = 0; i < max_bits; i++)
    {
        uint32_t byte = i / 8;
        uint8_t bit = 1 << (i % 8);
        if ((buf[byte] & bit) == 0)
        {
            buf[byte] |= bit;
            ramdisk_write(block_idx, buf);
            return i;
        }
    }
    return -1;
}

static void free_bit(uint32_t block_idx, uint32_t bit_idx)
{
    uint8_t buf[BLOCK_SIZE];
    ramdisk_read(block_idx, buf);
    buf[bit_idx / 8] &= ~(1 << (bit_idx % 8));
    ramdisk_write(block_idx, buf);
}


void fs_init(void)
{
    ramdisk_init();
    fs_memset(fd_table, 0, sizeof(fd_table));

    uint8_t buf[BLOCK_SIZE];
    ramdisk_read(0, buf);
    fs_memcpy(&sb, buf, sizeof(superblock_t));

    if (sb.magic != FS_MAGIC)
    {
        sb.magic = FS_MAGIC;
        sb.total_blocks = 2048;
        sb.total_inodes = MAX_INODES;
        sb.free_blocks = 2048 - DATA_BLOCK_START;
        sb.free_inodes = MAX_INODES;
        sb.inode_table_off = 0x600;
        sb.data_off = 0x40600;

        fs_memset(buf, 0, BLOCK_SIZE);
        fs_memcpy(buf, &sb, sizeof(superblock_t));
        ramdisk_write(0, buf);

        fs_memset(buf, 0, BLOCK_SIZE);
        ramdisk_write(1, buf);
        ramdisk_write(2, buf);
    }
}

int fs_ls(inode_t *out, int max) {
    int count = 0;
    uint8_t bitmap[BLOCK_SIZE];
    ramdisk_read(1, bitmap); 

    for (int i = 0; i < MAX_INODES && count < max; i++) {
        if (bitmap[i / 8] & (1 << (i % 8))) {
            inode_t node;
            read_inode(i, &node);
            /* Only list files that belong to the current directory */
            if (node.type != 0 && fs_strcmp(node.parent, current_dir) == 0) {
                out[count++] = node;
            }
        }
    }
    return count;
}

int fs_open(const char *name, int flags) {
    int fd = -1;
    for (int i = 0; i < MAX_FDS; i++) {
        if (!fd_table[i].in_use) { fd = i; break; }
    }
    if (fd == -1) return -1;

    inode_t inodes[MAX_INODES];
    int count = fs_ls(inodes, MAX_INODES);
    int found_idx = -1;

    for (int i = 0; i < count; i++) {
        if (fs_strcmp(inodes[i].name, name) == 0) {
            found_idx = i; 
            break;
        }
    }

    uint32_t inum;
    inode_t node;

    if (found_idx != -1) {
        /* File exists - lookup */
        uint8_t bm[BLOCK_SIZE];
        ramdisk_read(1, bm);
        for (inum = 0; inum < MAX_INODES; inum++) {
            if (bm[inum/8] & (1<<(inum%8))) {
                read_inode(inum, &node);
                if (fs_strcmp(node.name, name) == 0 && fs_strcmp(node.parent, current_dir) == 0) break;
            }
        }
        if (flags & O_TRUNC) {
            for (uint32_t i = 0; i < node.block_count; i++) {
                free_bit(2, node.blocks[i] - DATA_BLOCK_START);
            }
            node.size = 0;
            node.block_count = 0;
            write_inode(inum, &node);
        }
    } else {
        if (!(flags & O_CREAT)) return -1;
        inum = alloc_bit(1, MAX_INODES);
        if (inum == (uint32_t)-1) return -1;

        fs_memset(&node, 0, sizeof(inode_t));
        node.type = 1;
        fs_strcpy(node.name, name);
        fs_strcpy(node.parent, current_dir); /* Assign to current directory */
        write_inode(inum, &node);
    }

    fd_table[fd].in_use = 1;
    fd_table[fd].flags = flags;
    fd_table[fd].inode_num = inum;
    fd_table[fd].offset = 0;
    fs_memcpy(&fd_table[fd].inode, &node, sizeof(inode_t));

    return fd;
}

int fs_write(int fd, const void *buf, int n)
{
    file_desc_t *f = &fd_table[fd];
    if (!f->in_use || !(f->flags & O_WRONLY))
        return -1;

    const uint8_t *src = (const uint8_t *)buf;
    int written = 0;
    uint8_t block_buf[BLOCK_SIZE];

    while (n > 0 && f->inode.block_count < INODE_DIRECT)
    {
        uint32_t b_idx;
        if (f->offset % BLOCK_SIZE == 0)
        {
            int free_b = alloc_bit(2, sb.total_blocks - DATA_BLOCK_START);
            if (free_b == -1)
                break;
            b_idx = DATA_BLOCK_START + free_b;
            f->inode.blocks[f->inode.block_count++] = b_idx;
            fs_memset(block_buf, 0, BLOCK_SIZE);
        }
        else
        {
            b_idx = f->inode.blocks[f->inode.block_count - 1];
            ramdisk_read(b_idx, block_buf);
        }

        uint32_t b_off = f->offset % BLOCK_SIZE;
        uint32_t chunk = BLOCK_SIZE - b_off;
        if (chunk > (uint32_t)n)
            chunk = n;

        fs_memcpy(block_buf + b_off, src, chunk);
        ramdisk_write(b_idx, block_buf);

        f->offset += chunk;
        src += chunk;
        written += chunk;
        n -= chunk;
        if (f->offset > f->inode.size)
            f->inode.size = f->offset;
    }

    write_inode(f->inode_num, &f->inode);
    return written;
}

int fs_read(int fd, void *buf, int n)
{
    file_desc_t *f = &fd_table[fd];
    if (!f->in_use)
        return -1;

    uint8_t *dst = (uint8_t *)buf;
    int bytes_read = 0;
    uint8_t block_buf[BLOCK_SIZE];

    if (f->offset + n > f->inode.size)
        n = f->inode.size - f->offset;

    while (n > 0)
    {
        uint32_t block_entry = f->offset / BLOCK_SIZE;
        if (block_entry >= f->inode.block_count)
            break;

        uint32_t b_idx = f->inode.blocks[block_entry];
        ramdisk_read(b_idx, block_buf);

        uint32_t b_off = f->offset % BLOCK_SIZE;
        uint32_t chunk = BLOCK_SIZE - b_off;
        if (chunk > (uint32_t)n)
            chunk = n;

        fs_memcpy(dst, block_buf + b_off, chunk);

        f->offset += chunk;
        dst += chunk;
        bytes_read += chunk;
        n -= chunk;
    }
    return bytes_read;
}

void fs_close(int fd)
{
    if (fd >= 0 && fd < MAX_FDS)
        fd_table[fd].in_use = 0;
}

int fs_unlink(const char *name) {
    uint8_t bm[BLOCK_SIZE];
    ramdisk_read(1, bm);
    inode_t node;

    for (uint32_t inum = 0; inum < MAX_INODES; inum++) {
        if (bm[inum/8] & (1<<(inum%8))) {
            read_inode(inum, &node);
            if (fs_strcmp(node.name, name) == 0 && fs_strcmp(node.parent, current_dir) == 0) {
                for (uint32_t i = 0; i < node.block_count; i++) {
                    free_bit(2, node.blocks[i] - DATA_BLOCK_START);
                }
                free_bit(1, inum);
                return 0;
            }
        }
    }
    return -1;
}

int fs_is_dir(const char *name) {
    if (fs_strcmp(name, "/") == 0) return 1; 
    uint8_t bm[BLOCK_SIZE];
    ramdisk_read(1, bm);
    inode_t node;

    for (uint32_t inum = 0; inum < MAX_INODES; inum++) {
        if (bm[inum/8] & (1<<(inum%8))) {
            read_inode(inum, &node);
            if (node.type == 2 && fs_strcmp(node.name, name) == 0 && fs_strcmp(node.parent, current_dir) == 0) return 1;
        }
    }
    return 0;
}

int fs_mkdir(const char *name) {
    if (fs_is_dir(name)) return -1; 
    
    uint32_t inum = alloc_bit(1, MAX_INODES);
    if (inum == (uint32_t)-1) return -1;

    inode_t node;
    fs_memset(&node, 0, sizeof(inode_t));
    node.type = 2; 
    fs_strcpy(node.name, name);
    fs_strcpy(node.parent, current_dir); /* Assign to current directory */
    write_inode(inum, &node);
    
    return 0;
}