#include <Base.h>

#include <Register/Msr.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

#include "AmdIntelEmuRuntime.h"

STATIC
VOID
InternalInitializeApSaveState (
  OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState
  )
{
  IA32_SEGMENT_ATTRIBUTES SegAttributes;
  IA32_CR0                BspCr0;
  IA32_CR0                GuestCr0;
  UINT64                  PatMsr;

  ASSERT (SaveState != NULL);

  SaveState->CR2    = 0;
  SaveState->CR3    = 0;
  SaveState->CR4    = 0;
  SaveState->RFLAGS = 0x02;
  SaveState->EFER   = 0;
  SaveState->RIP    = 0xFFF0;

  SegAttributes.Bits.G   = 0;
  SegAttributes.Bits.DB  = 0;
  SegAttributes.Bits.L   = 0;
  SegAttributes.Bits.P   = 1;
  SegAttributes.Bits.DPL = 0;
  
  SaveState->CS.Selector   = 0xF000;
  SaveState->CS.Base       = 0xFFFF0000;
  SaveState->CS.Limit      = 0xFFFF;
  SegAttributes.Bits.S     = 1;
  SegAttributes.Bits.Type  = (BIT1 | BIT3);
  SaveState->CS.Attributes = SegAttributes.Uint16;

  SaveState->CS.Selector   = 0;
  SaveState->CS.Base       = 0;
  SaveState->CS.Limit      = 0xFFFF;
  SegAttributes.Bits.S     = 1;
  SegAttributes.Bits.Type  = BIT1;
  SaveState->DS.Attributes = SegAttributes.Uint16;

  SaveState->ES = SaveState->DS;
  SaveState->FS = SaveState->DS;
  SaveState->GS = SaveState->DS;
  SaveState->SS = SaveState->DS;

  SaveState->GDTR.Base  = 0;
  SaveState->GDTR.Limit = 0xFFFF;

  SaveState->IDTR = SaveState->GDTR;

  SaveState->LDTR.Selector   = 0;
  SaveState->LDTR.Base       = 0;
  SaveState->LDTR.Limit      = 0xFFFF;
  SegAttributes.Bits.S       = 0;
  SegAttributes.Bits.Type    = BIT1;
  SaveState->LDTR.Attributes = SegAttributes.Uint16;

  SaveState->TR            = SaveState->LDTR;
  SegAttributes.Bits.Type  = (BIT0 | BIT1);
  SaveState->TR.Attributes = SegAttributes.Uint16;

  SaveState->DR6 = 0xFFFF0FF0;
  SaveState->DR7 = 0x0400;
  //
  // TODO: Use the current AP values instead of the BSP's.
  //
  BspCr0.UintN     = AsmReadCr0 ();
  GuestCr0.UintN   = 0;
  GuestCr0.Bits.ET = 1;
  GuestCr0.Bits.NW = BspCr0.Bits.NW;
  GuestCr0.Bits.CD = BspCr0.Bits.CD;
  SaveState->CR0   = 0x60000010;

  SaveState->G_PAT = AsmReadMsr64 (MSR_IA32_PAT);
}
