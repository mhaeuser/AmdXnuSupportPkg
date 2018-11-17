SECTION .text

extern ASM_PFX (AmdIntelEmuInternalDisableTf)
ASM_PFX (AmdIntelEmuInternalDisableTf):
    pushf
    mov     rax, [rsp]
    and     [rsp], ~0x0100
    popf
    retn
