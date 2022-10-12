;------------------------------------------------------------------------------ ;
; Copyright (c) 2015 - 2018, Intel Corporation. All rights reserved.<BR>
; This program and the accompanying materials
; are licensed and made available under the terms and conditions of the BSD License
; which accompanies this distribution.  The full text of the license may be found at
; http://opensource.org/licenses/bsd-license.php.
;
; THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
; WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
;
; Module Name:
;
;   MpFuncs.nasm
;
; Abstract:
;
;   This is the assembly code for MP support
;
;-------------------------------------------------------------------------------

DEFAULT REL

SECTION .text

;-------------------------------------------------------------------------------------
;RendezvousFunnelProc  procedure follows. All APs execute their procedure. This
;procedure serializes all the AP processors through an Init sequence. It must be
;noted that APs arrive here very raw...ie: real mode, no stack.
;ALSO THIS PROCEDURE IS EXECUTED BY APs ONLY ON 16 BIT MODE. HENCE THIS PROC
;IS IN MACHINE CODE.
;-------------------------------------------------------------------------------------
global ASM_PFX(RendezvousFunnelProc)
ASM_PFX(RendezvousFunnelProc):
; At this point CS = 0x(vv00) and ip= 0x0.
; Save BIST information to ebp firstly

BITS 16
    mov        ebp, eax                        ; Save BIST information

    mov        ax, cs
    mov        ds, ax
    mov        es, ax
    mov        ss, ax
    xor        ax, ax
    mov        fs, ax
    mov        gs, ax

    mov        si,  BufferStartLocation
    mov        ebx, [si]

    mov        si,  DataSegmentLocation
    mov        edx, [si]

    ;
    ; Get start address of 32-bit code in low memory (<1MB)
    ;
    mov        edi, ModeTransitionMemoryLocation

    mov        si, GdtrLocation
o32 lgdt       [cs:si]

    mov        si, IdtrLocation
o32 lidt       [cs:si]

    ;
    ; Switch to protected mode
    ;
    mov        eax, cr0                    ; Get control register 0
    or         eax, 000000003h             ; Set PE bit (bit #0) & MP
    mov        cr0, eax

    ; Switch to 32-bit code (>1MB)
o32 jmp far    [cs:di]

;
; Following code must be copied to memory with type of EfiBootServicesCode.
; This is required if NX is enabled for EfiBootServicesCode of memory.
;
BITS 32
    mov        ds, dx
    mov        es, dx
    mov        fs, dx
    mov        gs, dx
    mov        ss, dx

    ;
    ; Enable PAE
    ;
    mov        eax, cr4
    bts        eax, 5
    mov        cr4, eax

    ;
    ; Load page table
    ;
    mov        esi, Cr3Location             ; Save CR3 in ecx
    mov        ecx, [ebx + esi]
    mov        cr3, ecx                    ; Load CR3

    ;
    ; Enable long mode
    ;
    mov        ecx, 0c0000080h             ; EFER MSR number
    rdmsr                                  ; Read EFER
    bts        eax, 8                      ; Set LME=1
    wrmsr                                  ; Write EFER

    ;
    ; Enable paging
    ;
    mov        eax, cr0                    ; Read CR0
    bts        eax, 31                     ; Set PG=1
    mov        cr0, eax                    ; Write CR0

    ;
    ; Far jump to 64-bit code
    ;
    mov        edi, ModeHighMemoryLocation
    add        edi, ebx
    jmp far    [edi]

BITS 64
    mov        esi, ebx

    mov        eax, 0
    cpuid
    cmp        eax, 0bh
    jb         NoX2Apic             ; CPUID level below CPUID_EXTENDED_TOPOLOGY

    mov        eax, 0bh
    xor        ecx, ecx
    cpuid
    test       ebx, 0ffffh
    jz         NoX2Apic             ; CPUID.0BH:EBX[15:0] is zero

    ; Processor is x2APIC capable; 32-bit x2APIC ID is already in EDX
    jmp        GetNextProcNumber

NoX2Apic:
    ; Processor is not x2APIC capable, so get 8-bit APIC ID
    mov        eax, 1
    cpuid
    shr        ebx, 24
    mov        edx, ebx

    ; TODO: VMCB
GetProcessorNumber:
    ;
    ; Get processor number for this AP
    ; Note that BSP may become an AP due to SwitchBsp()
    ;
    lea         eax, [esi + CpuInfoLocation]
    mov         edi, [eax]

GetNextProcNumber:
    cmp         dword [edi], edx                      ; APIC ID match?
    jz          CProcedureInvoke
    add         edi, 20
    jmp         GetNextProcNumber

CProcedureInvoke:
    jmp        $                 ; Should never reach here
