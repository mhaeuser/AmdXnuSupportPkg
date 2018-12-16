/** @file
  Subset of the Local APIC Library.

  This local APIC library instance supports x2APIC capable processors
  which have xAPIC and x2APIC modes.

  Copyright (c) 2010 - 2018, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2017, AMD Inc. All rights reserved.<BR>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Base.h>

#include <Register/ArchitecturalMsr.h>
#include <Register/LocalApic.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/LocalApicLib.h>
#include <Library/PcdLib.h>

/**
  Determine if the CPU supports the Local APIC Base Address MSR.

  @retval TRUE  The CPU supports the Local APIC Base Address MSR.
  @retval FALSE The CPU does not support the Local APIC Base Address MSR.

**/
STATIC
BOOLEAN
InternalLocalApicBaseAddressMsrSupported (
  VOID
  )
{
  UINT32  RegEax;
  UINTN   FamilyId;

  AsmCpuid (1, &RegEax, NULL, NULL, NULL);
  FamilyId = BitFieldRead32 (RegEax, 8, 11);
  if (FamilyId == 0x04 || FamilyId == 0x05) {
    //
    // CPUs with a FamilyId of 0x04 or 0x05 do not support the
    // Local APIC Base Address MSR
    //
    return FALSE;
  }
  return TRUE;
}

/**
  Get the current local APIC mode.

  If local APIC is disabled, then ASSERT.

  @retval LOCAL_APIC_MODE_XAPIC  current APIC mode is xAPIC.
  @retval LOCAL_APIC_MODE_X2APIC current APIC mode is x2APIC.

**/
STATIC
UINTN
InternalGetApicMode (
  VOID
  )
{
  MSR_IA32_APIC_BASE_REGISTER  ApicBaseMsr;

  if (!InternalLocalApicBaseAddressMsrSupported ()) {
    //
    // If CPU does not support APIC Base Address MSR, then return XAPIC mode
    //
    return LOCAL_APIC_MODE_XAPIC;
  }

  ApicBaseMsr.Uint64 = AsmReadMsr64 (MSR_IA32_APIC_BASE);
  //
  // Local APIC should have been enabled
  //
  ASSERT (ApicBaseMsr.Bits.EN != 0);
  if (ApicBaseMsr.Bits.EXTD != 0) {
    return LOCAL_APIC_MODE_X2APIC;
  }

  return LOCAL_APIC_MODE_XAPIC;
}

/**
  Retrieve the base address of local APIC.

  @return The base address of local APIC.

**/
UINTN
AmdIntelEmuGetLocalApicBaseAddress (
  VOID
  )
{
  MSR_IA32_APIC_BASE_REGISTER  ApicBaseMsr;

  if (!InternalLocalApicBaseAddressMsrSupported ()) {
    //
    // If CPU does not support Local APIC Base Address MSR, then retrieve
    // Local APIC Base Address from PCD
    //
    return PcdGet32 (PcdCpuLocalApicBaseAddress);
  }

  ApicBaseMsr.Uint64 = AsmReadMsr64 (MSR_IA32_APIC_BASE);

  return (UINTN)(LShiftU64 ((UINT64) ApicBaseMsr.Bits.ApicBaseHi, 32)) +
           (((UINTN)ApicBaseMsr.Bits.ApicBase) << 12);
}

/**
  Read from a local APIC register.

  This function reads from a local APIC register either in xAPIC or x2APIC mode.
  It is required that in xAPIC mode wider registers (64-bit or 256-bit) must be
  accessed using multiple 32-bit loads or stores, so this function only performs
  32-bit read.

  @param  MmioOffset  The MMIO offset of the local APIC register in xAPIC mode.
                      It must be 16-byte aligned.

  @return 32-bit      Value read from the register.

**/
UINT32
AmdIntelEmuReadLocalApicReg (
  IN UINTN  MmioOffset
  )
{
  UINT32 MsrIndex;

  ASSERT ((MmioOffset & 0xf) == 0);

  if (InternalGetApicMode () == LOCAL_APIC_MODE_XAPIC) {
    return MmioRead32 (AmdIntelEmuGetLocalApicBaseAddress () + MmioOffset);
  }
  //
  // DFR is not supported in x2APIC mode.
  //
  ASSERT (MmioOffset != XAPIC_ICR_DFR_OFFSET);
  //
  // Note that in x2APIC mode, ICR is a 64-bit MSR that needs special treatment. It
  // is not supported in this function for simplicity.
  //
  ASSERT (MmioOffset != XAPIC_ICR_HIGH_OFFSET);

  MsrIndex = (UINT32)(MmioOffset >> 4) + X2APIC_MSR_BASE_ADDRESS;
  return AsmReadMsr32 (MsrIndex);
}

/**
  Write to a local APIC register.

  This function writes to a local APIC register either in xAPIC or x2APIC mode.
  It is required that in xAPIC mode wider registers (64-bit or 256-bit) must be
  accessed using multiple 32-bit loads or stores, so this function only performs
  32-bit write.

  if the register index is invalid or unsupported in current APIC mode, then ASSERT.

  @param  MmioOffset  The MMIO offset of the local APIC register in xAPIC mode.
                      It must be 16-byte aligned.
  @param  Value       Value to be written to the register.

**/
STATIC
VOID
InternalWriteLocalApicReg (
  IN UINTN  MmioOffset,
  IN UINT32 Value
  )
{
  UINT32 MsrIndex;

  ASSERT ((MmioOffset & 0xf) == 0);

  if (InternalGetApicMode () == LOCAL_APIC_MODE_XAPIC) {
    MmioWrite32 (AmdIntelEmuGetLocalApicBaseAddress () + MmioOffset, Value);
  } else {
    //
    // DFR is not supported in x2APIC mode.
    //
    ASSERT (MmioOffset != XAPIC_ICR_DFR_OFFSET);
    //
    // Note that in x2APIC mode, ICR is a 64-bit MSR that needs special treatment. It
    // is not supported in this function for simplicity.
    //
    ASSERT (MmioOffset != XAPIC_ICR_HIGH_OFFSET);
    ASSERT (MmioOffset != XAPIC_ICR_LOW_OFFSET);

    MsrIndex =  (UINT32)(MmioOffset >> 4) + X2APIC_MSR_BASE_ADDRESS;
    //
    // The serializing semantics of WRMSR are relaxed when writing to the APIC registers.
    // Use memory fence here to force the serializing semantics to be consisent with xAPIC mode.
    //
    MemoryFence ();
    AsmWriteMsr32 (MsrIndex, Value);
  }
}

/**
  Initialize the state of the SoftwareEnable bit in the Local APIC
  Spurious Interrupt Vector register.

  @param  Enable  If TRUE, then set SoftwareEnable to 1
                  If FALSE, then set SoftwareEnable to 0.

**/
STATIC
VOID
InternalInitializeLocalApicSoftwareEnable (
  IN BOOLEAN  Enable
  )
{
  LOCAL_APIC_SVR  Svr;

  //
  // Set local APIC software-enabled bit.
  //
  Svr.Uint32 = AmdIntelEmuReadLocalApicReg (XAPIC_SPURIOUS_VECTOR_OFFSET);
  if (Enable) {
    if (Svr.Bits.SoftwareEnable == 0) {
      Svr.Bits.SoftwareEnable = 1;
      InternalWriteLocalApicReg (XAPIC_SPURIOUS_VECTOR_OFFSET, Svr.Uint32);
    }
  } else {
    if (Svr.Bits.SoftwareEnable == 1) {
      Svr.Bits.SoftwareEnable = 0;
      InternalWriteLocalApicReg (XAPIC_SPURIOUS_VECTOR_OFFSET, Svr.Uint32);
    }
  }
}

/**
  Initialize the local APIC timer.

  The local APIC timer is initialized and enabled.

  @param DivideValue   The divide value for the DCR. It is one of 1,2,4,8,16,32,64,128.
                       If it is 0, then use the current divide value in the DCR.
  @param InitCount     The initial count value.
  @param PeriodicMode  If TRUE, timer mode is peridoic. Othewise, timer mode is one-shot.
  @param Vector        The timer interrupt vector number.

**/
VOID
AmdIntelEmuInitializeApicTimer (
  IN UINTN   DivideValue,
  IN UINT32  InitCount,
  IN BOOLEAN PeriodicMode,
  IN UINT8   Vector
  )
{
  LOCAL_APIC_DCR       Dcr;
  LOCAL_APIC_LVT_TIMER LvtTimer;
  UINT32               Divisor;

  //
  // Ensure local APIC is in software-enabled state.
  //
  InternalInitializeLocalApicSoftwareEnable (TRUE);

  //
  // Program init-count register.
  //
  InternalWriteLocalApicReg (XAPIC_TIMER_INIT_COUNT_OFFSET, InitCount);

  if (DivideValue != 0) {
    ASSERT (DivideValue <= 128);
    ASSERT (DivideValue == GetPowerOfTwo32((UINT32)DivideValue));
    Divisor = (UINT32)((HighBitSet32 ((UINT32)DivideValue) - 1) & 0x7);

    Dcr.Uint32 = AmdIntelEmuReadLocalApicReg (XAPIC_TIMER_DIVIDE_CONFIGURATION_OFFSET);
    Dcr.Bits.DivideValue1 = (Divisor & 0x3);
    Dcr.Bits.DivideValue2 = (Divisor >> 2);
    InternalWriteLocalApicReg (XAPIC_TIMER_DIVIDE_CONFIGURATION_OFFSET, Dcr.Uint32);
  }

  //
  // Enable APIC timer interrupt with specified timer mode.
  //
  LvtTimer.Uint32 = AmdIntelEmuReadLocalApicReg (XAPIC_LVT_TIMER_OFFSET);
  if (PeriodicMode) {
    LvtTimer.Bits.TimerMode = 1;
  } else {
    LvtTimer.Bits.TimerMode = 0;
  }
  LvtTimer.Bits.Mask = 0;
  LvtTimer.Bits.Vector = Vector;
  InternalWriteLocalApicReg (XAPIC_LVT_TIMER_OFFSET, LvtTimer.Uint32);
}

/**
  Disable the local APIC timer interrupt.

**/
VOID
AmdIntelEmuDisableApicTimerInterrupt (
  VOID
  )
{
  LOCAL_APIC_LVT_TIMER LvtTimer;

  LvtTimer.Uint32 = AmdIntelEmuReadLocalApicReg (XAPIC_LVT_TIMER_OFFSET);
  LvtTimer.Bits.Mask = 1;
  InternalWriteLocalApicReg (XAPIC_LVT_TIMER_OFFSET, LvtTimer.Uint32);
}
