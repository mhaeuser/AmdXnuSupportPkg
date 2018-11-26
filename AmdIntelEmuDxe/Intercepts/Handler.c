#include <Base.h>

#include <Protocol/DebugSupport.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>

#include "../AmdIntelEmu.h"

VOID
AmdEmuInterceptCpuid (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_INTEL_EMU_REGISTERS          *Registers
  );

VOID
AmdIntelEmuInternalInterceptRdmsr (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_INTEL_EMU_REGISTERS          *Registers
  );

VOID
AmdIntelEmuInternalInterceptWrmsr (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_INTEL_EMU_REGISTERS          *Registers
  );

VOID
AmdIntelEmuInternalExceptionUd (
  IN OUT AMD_VMCB_CONTROL_AREA    *Vmcb,
  IN OUT AMD_INTEL_EMU_REGISTERS  *Registers
  );

VOID
AmdIntelEmuInternalExceptionDebug (
  IN OUT AMD_VMCB_CONTROL_AREA  *Vmcb
  );

VOID
AmdIntelEmuInternalGetRipInstruction (
  IN  CONST AMD_VMCB_CONTROL_AREA  *Vmcb,
  OUT hde64s                       *Instruction
  )
{
  CONST AMD_VMCB_SAVE_STATE_AREA_NON_ES *SaveState;
  UINTN                                 Address;
  IA32_SEGMENT_ATTRIBUTES               SegAttrib;

  ASSERT (Vmcb != NULL);
  ASSERT (Instruction != NULL);

  SaveState = (AMD_VMCB_SAVE_STATE_AREA_NON_ES *)(UINTN)Vmcb->VmcbSaveState;
  ASSERT (SaveState != NULL);

  Address = SaveState->RIP;
  //
  // CS Base is ignored in Long Mode.
  //
  SegAttrib.Uint16 = SaveState->CS.Attributes;
  if (SegAttrib.Bits.L == 0) {
    Address += (SaveState->CS.Base << 4U);
  }

  hde64_disasm ((VOID *)Address, Instruction);
}

STATIC
VOID
InternalRaiseRip (
  IN OUT AMD_VMCB_CONTROL_AREA  *Vmcb
  )
{
  AMD_VMCB_SAVE_STATE_AREA_NON_ES *SaveState;
  hde64s                          Instruction;

  ASSERT (Vmcb != NULL);

  SaveState = (AMD_VMCB_SAVE_STATE_AREA_NON_ES *)(UINTN)Vmcb->VmcbSaveState;
  ASSERT (SaveState != NULL);

  if (mAmdIntelEmuInternalNrip) {
    SaveState->RIP = Vmcb->nRIP;
  } else {
    AmdIntelEmuInternalGetRipInstruction (Vmcb, &Instruction);
    ASSERT ((Instruction.flags & F_ERROR) != 0);
    SaveState->RIP += Instruction.len;
  }
}

STATIC
VOID
InternalHandleEvents (
  IN OUT AMD_VMCB_CONTROL_AREA  *Vmcb
  )
{
  STATIC CONST UINT8 ExceptionPriorities[] =
    { 18, 1, 2, 32, 1, 13, 14, 6, 7, 13, 0, 10, 11, 12, 13, 14, 16, 17, 19 };

  UINTN Index;
  UINT8 Priority;

  ASSERT (Vmcb != NULL);

  if (Vmcb->EXITINTINFO.Bits.V != 0) {
    if (Vmcb->EVENTINJ.Bits.V == 0) {
      CopyMem (
        &Vmcb->EVENTINJ,
        &Vmcb->EXITINTINFO,
        sizeof (Vmcb->EVENTINJ)
        );
      return;
    }
    
    if ((Vmcb->EVENTINJ.Bits.TYPE    == AmdVmcbException)
     && (Vmcb->EXITINTINFO.Bits.TYPE == AmdVmcbException)) {
      //
      // Combine into #DF if necessary.
      //
      switch (Vmcb->EXITINTINFO.Bits.VECTOR) {
        case EXCEPT_IA32_DIVIDE_ERROR:
        case EXCEPT_IA32_INVALID_TSS:
        case EXCEPT_IA32_SEG_NOT_PRESENT:
        case EXCEPT_IA32_STACK_FAULT:
        case EXCEPT_IA32_GP_FAULT:
        {
          switch (Vmcb->EVENTINJ.Bits.VECTOR) {
            case EXCEPT_IA32_INVALID_TSS:
            case EXCEPT_IA32_SEG_NOT_PRESENT:
            case EXCEPT_IA32_STACK_FAULT:
            case EXCEPT_IA32_GP_FAULT:
            {
              AmdIntelEmuInternalInjectDf (Vmcb);
              return;
            }

            default:
            {
              break;
            }
          }
        }

        case EXCEPT_IA32_PAGE_FAULT:
        {
          switch (Vmcb->EVENTINJ.Bits.VECTOR) {
            case EXCEPT_IA32_PAGE_FAULT:
            case EXCEPT_IA32_INVALID_TSS:
            case EXCEPT_IA32_SEG_NOT_PRESENT:
            case EXCEPT_IA32_STACK_FAULT:
            case EXCEPT_IA32_GP_FAULT:
            {
              AmdIntelEmuInternalInjectDf (Vmcb);
              return;
            }

            default:
            {
              break;
            }
          }
        }

        default:
        {
          break;
        }
      }
      //
      // Discard the lower priority exception.
      //
      for (Index = 0; Index < ARRAY_SIZE (ExceptionPriorities); ++Index) {
        Priority = ExceptionPriorities[Index];

        if ((Vmcb->EVENTINJ.Bits.VECTOR == Priority)
          || ((Priority == 32)
           && (Vmcb->EVENTINJ.Bits.VECTOR > 32)
           && (Vmcb->EVENTINJ.Bits.VECTOR <= 255))) {
          break;
        }

        if ((Vmcb->EXITINTINFO.Bits.VECTOR == Priority)
          || ((Priority == 32)
           && (Vmcb->EXITINTINFO.Bits.VECTOR > 32)
           && (Vmcb->EXITINTINFO.Bits.VECTOR <= 255))) {
          CopyMem (
            &Vmcb->EVENTINJ,
            &Vmcb->EXITINTINFO,
            sizeof (Vmcb->EVENTINJ)
            );
          break;
        }
      }

      return;
    }

    // TODO: Implement queue.
  }
}

VOID
EFIAPI
AmdIntelEmuInternalInterceptionHandler (
  IN OUT AMD_VMCB_CONTROL_AREA    *Vmcb,
  IN OUT AMD_INTEL_EMU_REGISTERS  *Registers
  )
{
  AMD_VMCB_SAVE_STATE_AREA_NON_ES *SaveState;
  
  Vmcb->EVENTINJ.Uint64 = 0;
  //
  // For forward compatibility, if the hypervisor has not modified the VMCB,
  // the hypervisor may write FFFF_FFFFh to the VMCB Clean Field to indicate
  // that it has not changed any VMCB contents other than the fields described
  // below as explicitly uncached.
  //
  Vmcb->VmcbCleanBits.Uint32 = MAX_UINT32;

  SaveState = (AMD_VMCB_SAVE_STATE_AREA_NON_ES *)(UINTN)Vmcb->VmcbSaveState;

  switch (Vmcb->EXITCODE) {
    case VMEXIT_EXCP_UD:
    {
      AmdIntelEmuInternalExceptionUd (Vmcb, Registers);
      break;
    }
    
    case VMEXIT_EXCP_DB:
    {
      AmdIntelEmuInternalExceptionDebug (Vmcb);
      break;
    }

    case VMEXIT_CPUID:
    {
      AmdEmuInterceptCpuid (SaveState, Registers);
      InternalRaiseRip (Vmcb);
      break;
    }

    case VMEXIT_MSR:
    {
      if (Vmcb->EXITINFO1 == 0) {
        AmdIntelEmuInternalInterceptRdmsr (SaveState, Registers);
      } else {
        ASSERT (Vmcb->EXITINFO1 == 1);
        AmdIntelEmuInternalInterceptWrmsr (SaveState, Registers);
      }

      InternalRaiseRip (Vmcb);

      break;
    }

    case VMEXIT_VMRUN:
    {
      AmdIntelEmuInternalInjectUd (Vmcb);
      break;
    }

    default:
    {
      ASSERT (FALSE);
      break;
    }
  }

  InternalHandleEvents (Vmcb);
}
