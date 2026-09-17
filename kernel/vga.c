/* =============================================================================
 * SENG21213-OS :: VGA Text-Mode Driver (WITH SCROLLBACK HISTORY)
 * File   : kernel/vga.c
 * ============================================================================*/
#include "vga.h"
#include "../include/types.h"

#define HISTORY_MAX 250 
#define VGA_COLS 80
#define VGA_ROWS 25

/* ---------------------------------------------------------------------------
 * Internal state
 * --------------------------------------------------------------------------*/
static uint16_t history[HISTORY_MAX][VGA_COLS];
static int cursor_row  = 0; 
static int cursor_col  = 0;
static int view_offset = 0; 
static uint8_t cur_attr = 0;

/* ---------------------------------------------------------------------------
 * The Render Engine (The "Camera")
 * --------------------------------------------------------------------------*/
static void vga_render(void) {
    volatile uint16_t *vga = VGA_ADDR;
    
    int max_scroll = cursor_row - VGA_ROWS + 1;
    if (max_scroll < 0) max_scroll = 0;
    
    int start_row = max_scroll - view_offset;
    if (start_row < 0) start_row = 0;

    for (int r = 0; r < VGA_ROWS; r++) {
        int h_row = start_row + r;
        for (int c = 0; c < VGA_COLS; c++) {
            if (h_row <= cursor_row) {
                vga[r * VGA_COLS + c] = history[h_row][c];
            } else {
                vga[r * VGA_COLS + c] = (uint16_t)((cur_attr << 8) | ' ');
            }
        }
    }
}

/* ---------------------------------------------------------------------------
 * Hardware cursor update
 * --------------------------------------------------------------------------*/
static void update_hw_cursor(void) {
    int max_scroll = cursor_row - VGA_ROWS + 1;
    if (max_scroll < 0) max_scroll = 0;
    
    int start_row = max_scroll - view_offset;
    int visual_row = cursor_row - start_row;
    
    uint16_t pos;
    
    if (visual_row < 0 || visual_row >= VGA_ROWS) {
        pos = VGA_ROWS * VGA_COLS; 
    } else {
        pos = (uint16_t)(visual_row * VGA_COLS + cursor_col);
    }
    
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}


static inline void vga_write_history(int h_row, int h_col, char c, uint8_t attr) {
    if (h_row >= 0 && h_row < HISTORY_MAX && h_col >= 0 && h_col < VGA_COLS) {
        history[h_row][h_col] = (uint16_t)((attr << 8) | (uint8_t)c);
    }
}

/* ---------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------*/

void vga_init(void) {
    cur_attr = VGA_ATTR(VGA_LIGHT_GREY, VGA_BLACK);
    vga_clear(VGA_BLACK);
}

void vga_clear(vga_color_t bg) {
    cur_attr = VGA_ATTR(VGA_LIGHT_GREY, bg);
    uint16_t blank = (uint16_t)((cur_attr << 8) | ' ');
    
    for (int r = 0; r < HISTORY_MAX; r++) {
        for (int c = 0; c < VGA_COLS; c++) {
            history[r][c] = blank;
        }
    }
    cursor_row = 0;
    cursor_col = 0;
    view_offset = 0;
    
    vga_render();
    update_hw_cursor();
}

void vga_set_color(vga_color_t fg, vga_color_t bg) {
    cur_attr = VGA_ATTR(fg, bg);
}


void vga_scroll_up(void) {
    int max_scroll = cursor_row - VGA_ROWS + 1;
    if (max_scroll < 0) max_scroll = 0;
    
    if (view_offset < max_scroll) {
        view_offset++;
        vga_render();
        update_hw_cursor();
    }
}

void vga_scroll_down(void) {
    if (view_offset > 0) {
        view_offset--;
        vga_render();
        update_hw_cursor();
    }
}

void vga_putchar(char c) {
    
    if (view_offset > 0) view_offset = 0;

    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\r') {
        cursor_col = 0;
    } else if (c == '\t') {
        cursor_col = (cursor_col + 8) & ~7;
        if (cursor_col >= VGA_COLS) { cursor_col = 0; cursor_row++; }
    } else if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
            vga_write_history(cursor_row, cursor_col, ' ', cur_attr);
        }
    } else {
        vga_write_history(cursor_row, cursor_col, c, cur_attr);
        cursor_col++;
        if (cursor_col >= VGA_COLS) { 
            cursor_col = 0; 
            cursor_row++; 
        }
    }

   
    if (cursor_row >= HISTORY_MAX) {
        for (int r = 1; r < HISTORY_MAX; r++) {
            for (int col = 0; col < VGA_COLS; col++) {
                history[r-1][col] = history[r][col];
            }
        }
        cursor_row = HISTORY_MAX - 1;
        

        uint16_t blank = (uint16_t)((cur_attr << 8) | ' ');
        for (int col = 0; col < VGA_COLS; col++) {
            history[cursor_row][col] = blank;
        }
    }
    
    vga_render();
    update_hw_cursor();
}

void vga_puts(const char *str) {
    if (!str) return;
    while (*str) vga_putchar(*str++);
}

void vga_puts_color(const char *str, vga_color_t fg, vga_color_t bg) {
    uint8_t saved = cur_attr;
    vga_set_color(fg, bg);
    vga_puts(str);
    cur_attr = saved;
}

void vga_set_cursor(int row, int col) {
    int base_row = cursor_row - VGA_ROWS + 1;
    if (base_row < 0) base_row = 0;
    
    int target_row = base_row + row;
    cursor_row = (target_row < 0) ? 0 : (target_row >= HISTORY_MAX ? HISTORY_MAX - 1 : target_row);
    cursor_col = (col < 0) ? 0 : (col >= VGA_COLS ? VGA_COLS - 1 : col);
    
    vga_render();
    update_hw_cursor();
}

static void print_uint(uint32_t n, int base) {
    char buf[32];
    int  i = 0;
    if (n == 0) { vga_putchar('0'); return; }
    while (n > 0) {
        int r = n % base;
        buf[i++] = (r < 10) ? ('0' + r) : ('a' + r - 10);
        n /= base;
    }
    while (i > 0) vga_putchar(buf[--i]);
}

void vga_printf(const char *fmt, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    while (*fmt) {
        if (*fmt != '%') { vga_putchar(*fmt++); continue; }
        fmt++;
        switch (*fmt) {
            case 's': vga_puts(__builtin_va_arg(args, const char *)); break;
            case 'c': vga_putchar((char)__builtin_va_arg(args, int)); break;
            case 'd': {
                int v = __builtin_va_arg(args, int);
                if (v < 0) { vga_putchar('-'); v = -v; }
                print_uint((uint32_t)v, 10);
                break;
            }
            case 'u': print_uint(__builtin_va_arg(args, uint32_t), 10); break;
            case 'x': print_uint(__builtin_va_arg(args, uint32_t), 16); break;
            case '%': vga_putchar('%'); break;
            default:  vga_putchar(*fmt); break;
        }
        fmt++;
    }
    __builtin_va_end(args);
}

void vga_draw_box(int row, int col, int height, int width, vga_color_t color) {
    uint8_t saved = cur_attr;
    vga_set_color(color, VGA_BLACK);

    int base_row = cursor_row - VGA_ROWS + 1;
    if (base_row < 0) base_row = 0;

    vga_write_history(base_row + row,          col,         0xC9, cur_attr); 
    vga_write_history(base_row + row,          col+width-1, 0xBB, cur_attr); 
    vga_write_history(base_row + row+height-1, col,         0xC8, cur_attr); 
    vga_write_history(base_row + row+height-1, col+width-1, 0xBC, cur_attr); 

    for (int c = col+1; c < col+width-1; c++) {
        vga_write_history(base_row + row,          c, 0xCD, cur_attr); 
        vga_write_history(base_row + row+height-1, c, 0xCD, cur_attr);
    }
  
    for (int r = row+1; r < row+height-1; r++) {
        vga_write_history(base_row + r, col,         0xBA, cur_attr);
        vga_write_history(base_row + r, col+width-1, 0xBA, cur_attr);
    }

    cur_attr = saved;
    vga_render();
}