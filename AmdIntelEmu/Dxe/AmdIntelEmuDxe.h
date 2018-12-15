#ifndef AMD_INTEL_EMU_DXE_H_
#define AMD_INTEL_EMU_DXE_H_

#include "../AmdIntelEmu.h"

UINTN
EFIAPI
AmdIntelEmuInternalDisableTf (
  VOID
  );

/**
  The function will check if page table entry should be splitted to smaller
  granularity to unmap.

  @param[in] Context           The function context.
  @param[in] Address           Physical memory address.
  @param[in] Size              Size of the given physical memory.
  @param[in] PageTableEntry4K  4K PTE if applicable.

  @retval TRUE   Page table should be split to unmap.
  @retval FALSE  Page table should not be split to unmap.
**/
typedef
BOOLEAN
(*AMD_INTEL_EMU_UNMAP_SPLIT_PAGE) (
  IN CONST VOID           *Context,
  IN PHYSICAL_ADDRESS     Address,
  IN UINTN                Size,
  IN PAGE_TABLE_4K_ENTRY  *PageTableEntry4K  OPTIONAL
  );

VOID
AmdIntelEmuInternalInitMsrPm (
  IN OUT AMD_VMCB_CONTROL_AREA  *Vmcb
  );

/**
  Allocates and fills in the Page Directory and Page Table Entries to
  establish a 1:1 Virtual to Physical mapping.

  @param[in] Context         The function context.
  @param[in] SplitUnmapPage  Function to check whether to split and unmap.

  @return The address of 4 level page map.

**/
UINTN
CreateIdentityMappingPageTables (
  IN CONST VOID                      *Context,
  IN AMD_INTEL_EMU_UNMAP_SPLIT_PAGE  SplitUnmapPage
  );

VOID
AmdIntelEmuRunTestIntercepts (
  VOID
  );

#endif // AMD_INTEL_EMU_DXE_H_
