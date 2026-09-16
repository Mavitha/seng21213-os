; =============================================================================
; SENG21213-OS :: Kernel Entry Point
; File   : kernel/kernel_entry.asm
; Purpose: Bridges the bootloader (NASM) to the C kernel. Sets up calling
;          conventions then calls kernel_main().
; =============================================================================

[BITS 32]
[EXTERN kernel_main]   ; Defined in kernel.c
[GLOBAL _start]

_start:
    ; The bootloader already set up segments and a stack at 0x90000.
    ; We just call the C kernel main function.
    call kernel_main

    ; If kernel_main ever returns, halt the CPU permanently.
    cli
.halt:
    hlt
    jmp .halt

; ---------------------------------------------------------------------------
; Interrupt Handling (Added for Stage 1)
; ---------------------------------------------------------------------------
[GLOBAL idt_load]
[GLOBAL irq0_handler]
[EXTERN scheduler_tick] ; This is your C function!

; Loads the IDT pointer into the CPU and enables interrupts
idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    sti                 ; STI sets the Interrupt Flag (enables interrupts)
    ret

; Catches the IRQ0 timer, saves state, and calls the scheduler
irq0_handler:
    pushad              ; Save all current registers
    call scheduler_tick ; Jump into your C scheduler
    popad               ; Restore all registers
    iret                ; Return from interrupt
