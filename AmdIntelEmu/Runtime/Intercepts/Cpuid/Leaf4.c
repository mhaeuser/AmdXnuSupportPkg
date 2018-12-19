#include <Base.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

#include "../../../AmdIntelEmu.h"

VOID
AmdIntelEmuInternalCpuidLeaf4 (
  IN  UINT32  SubIndex,
  OUT UINT32  *Eax,
  OUT UINT32  *Ebx,
  OUT UINT32  *Ecx,
  OUT UINT32  *Edx
  )
{
  CPUID_AMD_APIC_ID_SIZE_CORE_COUNT_ECX  CoreCountEcx;
  CPUID_AMD_COMPUTE_UNIT_IDENTIFIERS_EBX CuIdEbx;
  UINT16                                 ThreadCount;
  UINT16                                 CoreCount;

  ASSERT (Eax != NULL);
  ASSERT (Ebx != NULL);
  ASSERT (Ecx != NULL);
  ASSERT (Edx != NULL);
  //
  // TODO: Abstract for shared usage with the MSR.
  //
  AsmCpuid (
    CPUID_AMD_APIC_ID_SIZE_CORE_COUNT,
    NULL,
    NULL,
    &CoreCountEcx.Uint32,
    NULL
    );
  ThreadCount = (UINT16)(CoreCountEcx.Bits.NC + 1);

  AsmCpuid (
    CPUID_AMD_COMPUTE_UNIT_IDENTIFIERS,
    NULL,
    &CuIdEbx.Uint32,
    NULL,
    NULL
    );
  CoreCount = (ThreadCount / (UINT16)(CuIdEbx.Bits.CoresPerComputeUnit + 1));
  //
  // Intel Leaf 4 and AMD Leaf 0x8000001D are mostly compatible.
  //
  AsmCpuidEx (0x8000001D, SubIndex, Eax, Ebx, Ecx, Edx);
  *Eax = BitFieldWrite32 (*Eax, 26, 31, (CoreCount - 1));
}
