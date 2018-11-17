#include <Base.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

#include "../../AmdIntelEmu.h"

#define CPUID_VMM_MAXVAL  0x40000010U

#define CPUID_SIGNATURE_VMM_EBX  SIGNATURE_32 ('G', 'e', 'n', 'u')
#define CPUID_SIGNATURE_VMM_EDX  SIGNATURE_32 ('i', 'n', 'e', 'A')
#define CPUID_SIGNATURE_VMM_ECX  SIGNATURE_32 ('M', 'D', '\0', '\0')

STATIC
VOID
InternalCpuidLeaf40000000 (
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

  *Eax = 0x40000010;
  *Ebx = CPUID_SIGNATURE_VMM_EBX;
  *Ecx = CPUID_SIGNATURE_VMM_EDX;
  *Edx = CPUID_SIGNATURE_VMM_ECX;
}

STATIC
VOID
InternalCpuidLeaf40000010 (
  OUT UINT32  *Eax,
  OUT UINT32  *Ebx,
  OUT UINT32  *Ecx,
  OUT UINT32  *Edx
  )
{
  MSR_AMD_PSTATE_DEF_REGISTER Pstate0DefMsr;
  UINT8                       CpuDfsId;
  UINT8                       CpuFid;
  UINT8                       Multiplier;
  UINT32                      BusFreq;
  UINT32                      TscFreq;

  ASSERT (Eax != NULL);
  ASSERT (Ebx != NULL);
  ASSERT (Ecx != NULL);
  ASSERT (Edx != NULL);

  Pstate0DefMsr.Uint64 = AsmReadMsr64 (MSR_AMD_PSTATE0_DEF);
  CpuDfsId   = (UINT8)Pstate0DefMsr.Bits.CpuDfsId;
  CpuFid     = (UINT8)Pstate0DefMsr.Bits.CpuFid;
  Multiplier = ((CpuFid / CpuDfsId) * 2U);
  //
  // The values are returned in KHz.
  //
  TscFreq = (UINT32)((UINT32)(CpuFid / CpuDfsId) * 200U * 1000U);
  BusFreq = (TscFreq / Multiplier);
  //
  // Refer to XNU's tsc.c file for more information.
  //
  *Eax = TscFreq;
  *Ebx = BusFreq;
  *Ecx = 0;
  *Edx = 0;
}

VOID
AmdEmuInterceptCpuidVmm (
  IN  UINT32  Index,
  OUT UINT32  *Eax,
  OUT UINT32  *Ebx,
  OUT UINT32  *Ecx,
  OUT UINT32  *Edx
  )
{
  ASSERT ((Index >= 0x40000000) && (Index <= 0x400000FF));
  ASSERT (Eax != NULL);
  ASSERT (Ebx != NULL);
  ASSERT (Ecx != NULL);
  ASSERT (Edx != NULL);

  switch (Index) {
    case 0x40000000:
    {
      InternalCpuidLeaf40000000 (Eax, Ebx, Ecx, Edx);
      break;
    }

    case 0x40000010:
    {
      InternalCpuidLeaf40000010 (Eax, Ebx, Ecx, Edx);
      break;
    }

    default:
    {
      ASSERT (Index <= CPUID_VMM_MAXVAL);

      *Eax = 0;
      *Ebx = 0;
      *Ecx = 0;
      *Edx = 0;

      break;
    }
  }
}
