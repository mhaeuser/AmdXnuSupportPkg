#include <Base.h>

#include <Register/Msr.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

#include "../../AmdIntelEmu.h"

VOID
AmdIntelEmuInternalRdmsrCoreThreadCount (
  IN OUT UINT64             *Rax,
  IN OUT AMD_EMU_REGISTERS  *Registers
  )
{
  MSR_HASWELL_E_CORE_THREAD_COUNT_REGISTER CoreThreadCountMsr;
  CPUID_AMD_APIC_ID_SIZE_CORE_COUNT_ECX    CoreCountEcx;
  CPUID_AMD_COMPUTE_UNIT_IDENTIFIERS_EBX   CuIdEbx;
  UINT16                                   ThreadCount;
  UINT16                                   CoreCount;

  ASSERT (Rax != NULL);
  ASSERT (Registers != NULL);

  AsmCpuid (
    CPUID_AMD_APIC_ID_SIZE_CORE_COUNT,
    NULL,
    NULL,
    &CoreCountEcx.Uint32,
    NULL
    );
  ThreadCount = (CoreCountEcx.Bits.NC + 1);

  AsmCpuid (
    CPUID_AMD_COMPUTE_UNIT_IDENTIFIERS,
    NULL,
    &CuIdEbx.Uint32,
    NULL,
    NULL
    );
  CoreCount = (ThreadCount / (UINT16)(CuIdEbx.Bits.CoresPerComputeUnit + 1));

  CoreThreadCountMsr.Uint64            = 0;
  CoreThreadCountMsr.Bits.Core_Count   = CoreCount;
  CoreThreadCountMsr.Bits.Thread_Count = ThreadCount;
  AmdIntelEmuInternalWriteMsrValue64 (
    Rax,
    Registers,
    CoreThreadCountMsr.Uint64
    );
}

VOID
AmdIntelEmuInternalWrmsrCoreThreadCount (
  IN OUT UINT64             *Rax,
  IN OUT AMD_EMU_REGISTERS  *Registers
  )
{
  ASSERT (Rax != NULL);
  ASSERT (Registers != NULL);
  //
  // THREAD_CORE_COUNT is read-only.
  //
}
