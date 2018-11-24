#include <Base.h>

#include <Register/ArchitecturalMsr.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

#include "../../../AmdIntelEmu.h"

VOID
AmdIntelEmuInternalUdSysenter (
  IN OUT AMD_INTEL_EMU_THREAD_CONTEXT  *ThreadContext,
  IN     CONST hde64s                  *Instruction
  )
{
  AMD_VMCB_CONTROL_AREA                  *Vmcb;
  AMD_VMCB_SAVE_STATE_AREA_NON_ES        *SaveState;
  MSR_IA32_SYSENTER_CS_REGISTER          SysenterCs;
  IA32_EFLAGS32                          Eflags;
  AMD_VMCB_SAVE_STATE_SEGMENT_ATTRIBUTES SegAttrib;
  //
  // sysenter is available in all legacy and compatibility modes (except Real
  // Mode), hence this function is safe to assume Long Mode.
  //
  ASSERT (ThreadContext != NULL);
  ASSERT (Instruction != NULL);

  ASSERT (ThreadContext->Registers != NULL);

  Vmcb = ThreadContext->Vmcb;
  ASSERT (Vmcb != NULL);

  if (Instruction->p_lock != 0) {
    AmdIntelEmuInternalInjectUd (Vmcb);
    return;
  }

  SaveState = (AMD_VMCB_SAVE_STATE_AREA_NON_ES *)(UINTN)Vmcb->VmcbSaveState;
  ASSERT (SaveState != NULL);

  SysenterCs.Uint64 = AsmReadMsr64 (MSR_IA32_SYSENTER_CS);
  if ((SysenterCs.Bits.CS & 0xFFFCU) == 0) {
    AmdIntelEmuInternalInjectGp (Vmcb, 0);
    return;
  }
  //
  // RSP and RIP are not cached.
  //
  SaveState->RSP = AsmReadMsr64 (MSR_IA32_SYSENTER_ESP);
  SaveState->RIP = AsmReadMsr64 (MSR_IA32_SYSENTER_EIP);
  //
  // RFLAGS is not cached.
  //
  Eflags.UintN = SaveState->RFLAGS;
  //
  // Ensures protected mode execution
  //
  Eflags.Bits.VM = 0;
  //
  // Mask interrupts.
  //
  Eflags.Bits.IF    = 0;
  SaveState->RFLAGS = Eflags.UintN;

  SaveState->CS.Selector = (SysenterCs.Bits.CS & 0xFFFCU);
  //
  // Operating system provides CS; RPL forced to 0
  // Flat segment
  //
  SaveState->CS.Base = 0;
  //
  // With 4 - KByte granularity, implies a 4 - GByte limit
  //
  SaveState->CS.Limit = 0x0FFFFF;
  SegAttrib.Uint16    = SaveState->CS.Attributes;
  //
  // Execute / read code, accessed
  //
  SegAttrib.Bits.Type      = 11;
  SegAttrib.Bits.S         = 1;
  SegAttrib.Bits.DPL       = 0;
  SegAttrib.Bits.P         = 1;
  SegAttrib.Bits.L         = 1;
  SegAttrib.Bits.DB        = 0;
  SegAttrib.Bits.G         = 1;
  SaveState->CS.Attributes = SegAttrib.Uint16;

  SaveState->CPL = 0;
  //
  // SS just above CS
  //
  SaveState->SS.Selector = (SaveState->CS.Selector + 8);
  //
  // Flat segment
  //
  SaveState->SS.Base = 0;
  //
  // With 4-KByte granularity, implies a 4-GByte limit
  //
  SaveState->SS.Limit = 0x0FFFFF;
  SegAttrib.Uint16    = SaveState->SS.Attributes;
  //
  // Read/write data, accessed
  //
  SegAttrib.Bits.Type = 3;
  SegAttrib.Bits.S    = 1;
  SegAttrib.Bits.DPL  = 0;
  SegAttrib.Bits.P    = 1;
  //
  // 32-bit stack segment
  //
  SegAttrib.Bits.DB = 1;
  //
  // 4-KByte granularity
  //
  SegAttrib.Bits.G         = 1;
  SaveState->SS.Attributes = SegAttrib.Uint16;

  Vmcb->VmcbCleanBits.Bits.SEG = 0;
}
