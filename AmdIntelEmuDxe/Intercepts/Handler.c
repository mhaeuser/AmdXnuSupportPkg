#include <Base.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

#include "../AmdIntelEmu.h"

VOID
AmdEmuInterceptCpuid (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_EMU_REGISTERS                *Registers
  );

VOID
AmdIntelEmuInternalInterceptRdmsr (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_EMU_REGISTERS                *Registers
  );

VOID
AmdIntelEmuInternalInterceptWrmsr (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_EMU_REGISTERS                *Registers
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

VOID
EFIAPI
AmdInterceptionHandler (
  IN OUT AMD_VMCB_CONTROL_AREA  *Vmcb,
  IN OUT AMD_EMU_REGISTERS      *Registers
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
    case VMEXIT_CPUID:
    {
      AmdEmuInterceptCpuid (SaveState, Registers);
      InternalRaiseRip (Vmcb, NULL);
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

      InternalRaiseRip (Vmcb, NULL);

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

  if (Vmcb->EXITINTINFO.Bits.V != 0) {
    if (Vmcb->EVENTINJ.Bits.V == 0) {
      Vmcb->EVENTINJ = Vmcb->EXITINTINFO;
    } else {
      // TODO: Implement.
    }
  }
}
