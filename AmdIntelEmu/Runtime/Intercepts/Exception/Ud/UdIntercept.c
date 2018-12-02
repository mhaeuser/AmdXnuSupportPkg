#include <PiDxe.h>

#include <Library/DebugLib.h>

#include "../../../AmdIntelEmuRuntime.h"

VOID
AmdIntelEmuInternalUdSysenter (
  IN OUT AMD_VMCB_CONTROL_AREA  *Vmcb,
  IN     CONST hde64s           *Instruction
  );

VOID
AmdIntelEmuInternalUdSysexit (
  IN OUT AMD_VMCB_CONTROL_AREA    *Vmcb,
  IN OUT AMD_INTEL_EMU_REGISTERS  *Registers,
  IN     CONST hde64s             *Instruction
  );

VOID
AmdIntelEmuInternalInjectUd (
  IN OUT AMD_VMCB_CONTROL_AREA  *Vmcb
  )
{
  ASSERT (Vmcb != NULL);
  //
  // Even injections are not cached.
  //
  Vmcb->EVENTINJ.Bits.VECTOR = EXCEPT_IA32_INVALID_OPCODE;
  Vmcb->EVENTINJ.Bits.TYPE   = AmdVmcbException;
  Vmcb->EVENTINJ.Bits.V      = 1;
  Vmcb->EVENTINJ.Bits.EV     = 0;
}

VOID
AmdIntelEmuInternalInjectDf (
  IN OUT AMD_VMCB_CONTROL_AREA  *Vmcb
  )
{
  ASSERT (Vmcb != NULL);
  //
  // Even injections are not cached.
  //
  Vmcb->EVENTINJ.Bits.VECTOR    = EXCEPT_IA32_DOUBLE_FAULT;
  Vmcb->EVENTINJ.Bits.TYPE      = AmdVmcbException;
  Vmcb->EVENTINJ.Bits.V         = 1;
  Vmcb->EVENTINJ.Bits.EV        = 1;
  Vmcb->EVENTINJ.Bits.ERRORCODE = 0;
}

VOID
AmdIntelEmuInternalInjectGp (
  IN OUT AMD_VMCB_CONTROL_AREA  *Vmcb,
  IN     UINT8                  ErrorCode
  )
{
  ASSERT (Vmcb != NULL);
  //
  // Even injections are not cached.
  //
  Vmcb->EVENTINJ.Bits.VECTOR    = EXCEPT_IA32_GP_FAULT;
  Vmcb->EVENTINJ.Bits.TYPE      = AmdVmcbException;
  Vmcb->EVENTINJ.Bits.V         = 1;
  Vmcb->EVENTINJ.Bits.EV        = 1;
  Vmcb->EVENTINJ.Bits.ERRORCODE = ErrorCode;
}

VOID
AmdIntelEmuInternalExceptionUd (
  IN OUT AMD_VMCB_CONTROL_AREA    *Vmcb,
  IN OUT AMD_INTEL_EMU_REGISTERS  *Registers
  )
{
  CONST AMD_VMCB_SAVE_STATE_AREA_NON_ES *SaveState;
  hde64s                                Instruction;

  ASSERT (Vmcb != NULL);

  SaveState = (AMD_VMCB_SAVE_STATE_AREA_NON_ES *)(UINTN)Vmcb->VmcbSaveState;
  ASSERT (SaveState != NULL);

  AmdIntelEmuInternalGetRipInstruction (SaveState, &Instruction);

  if ((Instruction.flags & F_ERROR) == 0) {
    if (Instruction.opcode == 0x0F) {
      switch (Instruction.opcode2) {
        case 34:
        {
          AmdIntelEmuInternalUdSysenter (Vmcb, &Instruction);
          return;
        }

        case 35:
        {
          AmdIntelEmuInternalUdSysexit (Vmcb, Registers, &Instruction);
          return;
        }

        default:
        {
          break;
        }
      }
    }
  }
  //
  // Transparently pass-through the #UD if the opcode is not emulated or cannot
  // be parsed.
  //
  AmdIntelEmuInternalInjectUd (Vmcb);
}
