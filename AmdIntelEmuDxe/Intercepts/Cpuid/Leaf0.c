#include <Base.h>

#include <Register/Cpuid.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

VOID
AmdIntelEmuInternalCpuidLeaf0 (
  OUT UINT32  *Eax,
  OUT UINT32  *Ebx,
  OUT UINT32  *Ecx,
  OUT UINT32  *Edx
  )
{
  ASSERT (Eax != NULL);
  ASSERT (Ebx != NULL);
  ASSERT (Ecx != NULL);
  ASSERT (Edx != NULL);

  AsmCpuid (CPUID_SIGNATURE, Eax, NULL, NULL, NULL);
  *Ebx = CPUID_SIGNATURE_GENUINE_INTEL_EBX;
  *Ecx = CPUID_SIGNATURE_GENUINE_INTEL_ECX;
  *Edx = CPUID_SIGNATURE_GENUINE_INTEL_EDX;
}
