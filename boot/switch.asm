; boot/switch.asm
; context_switch(uint32_t *old_esp, uint32_t new_esp)
;   [esp+4] = pointer to save old esp into
;   [esp+8] = new esp value to load
global context_switch
context_switch:
    pushad                      ; Save EAX ECX EDX EBX ESP EBP ESI EDI
    mov  eax, [esp + 36]        ; arg0: old_esp ptr  (36 = 8 regs × 4 + ret)
    mov  [eax], esp             ; *old_esp = current ESP
    mov  esp, [esp + 40]        ; arg1: new_esp — switch stacks
    popad                       ; Restore next process's registers
    ret                         ; Return into next process's code