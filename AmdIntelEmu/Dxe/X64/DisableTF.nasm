    DEFAULT REL
    SECTION .text

global ASM_PFX (AmdIntelEmuInternalDisableTf)
ASM_PFX (AmdIntelEmuInternalDisableTf):
    pushf
    mov     rax, [rsp]
    and     qword [rsp], ~0x0100
    popf
    retn
