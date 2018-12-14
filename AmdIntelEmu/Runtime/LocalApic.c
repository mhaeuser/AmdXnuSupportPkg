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
