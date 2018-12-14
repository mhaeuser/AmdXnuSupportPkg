#include <Base.h>

#include <Register/LocalApic.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>

#include "../../AmdIntelEmuRuntime.h"

STATIC CONST VOID *mLapicVersionPage = NULL;

VOID
AmdIntelEmuInternalMmioLapicSetPage (
  IN VOID  *Page
  )
{
  ASSERT (Page != NULL);
  ASSERT (((UINTN)Page % SIZE_4KB) == 0);

  *(UINT32 *)((UINTN)Page + XAPIC_VERSION_OFFSET) = BitFieldWrite32 (
                                                      AmdIntelEmuReadLocalApicReg (
                                                        XAPIC_VERSION_OFFSET
                                                        ),
                                                      0,
                                                      7,
                                                      0x14
                                                      );

  mLapicVersionPage = Page;
}

UINT64
AmdIntelEmuInternalMmioLapic (
  IN UINT64  Address
  )
{
  UINT32 BaseAddress;

  ASSERT (mLapicVersionPage != NULL);

  BaseAddress = FixedPcdGet32 (PcdCpuLocalApicBaseAddress);
  if ((Address >= (BaseAddress + XAPIC_VERSION_OFFSET))
   && (Address <  (BaseAddress + XAPIC_VERSION_OFFSET + 4))) {
    return (UINT64)(UINTN)mLapicVersionPage;
  }

  return Address;
}
