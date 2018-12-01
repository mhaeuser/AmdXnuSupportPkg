/** @file
  x64 Virtual Memory Management Services in the form of an IA-32 driver.
  Used to establish a 1:1 Virtual to Physical Mapping that is required to
  enter Long Mode (x64 64-bit mode).

  While we make a 1:1 mapping (identity mapping) for all physical pages
  we still need to use the MTRR's to ensure that the cachability attributes
  for all memory regions is correct.

  The basic idea is to use 2MB page table entries where ever possible. If
  more granularity of cachability is required then 4K page tables are used.

  References:
    1) IA-32 Intel(R) Architecture Software Developer's Manual Volume 1:Basic Architecture, Intel
    2) IA-32 Intel(R) Architecture Software Developer's Manual Volume 2:Instruction Set Reference, Intel
    3) IA-32 Intel(R) Architecture Software Developer's Manual Volume 3:System Programmer's Guide, Intel

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
Copyright (c) 2017, AMD Incorporated. All rights reserved.<BR>

This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

//
// This file has been derieved from MdeModulePkg/CoreIplPeim.
// Changes:
//   - Removed unused code such as to support memory encryption or stack NX.
//   - Marked Page Table structures as "User" when applicable, so that they are
//     accessible by the guest.
//   - Unmap pages and mark remove User flag on match to cause Guest #PFs.
//   - Allocate Page Table structures as reserved memory.
//

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>

#include "VirtualMemory.h"
#include "../AmdIntelEmu.h"

#define PAGE_TABLE_POOL_ALIGNMENT   BASE_2MB
#define PAGE_TABLE_POOL_UNIT_SIZE   SIZE_2MB
#define PAGE_TABLE_POOL_UNIT_PAGES  EFI_SIZE_TO_PAGES (PAGE_TABLE_POOL_UNIT_SIZE)
#define PAGE_TABLE_POOL_ALIGN_MASK  \
  (~(EFI_PHYSICAL_ADDRESS)(PAGE_TABLE_POOL_ALIGNMENT - 1))

typedef struct {
  VOID            *NextPool;
  UINTN           Offset;
  UINTN           FreePages;
} PAGE_TABLE_POOL;


//
// Global variable to keep track current available memory used as page table.
//
PAGE_TABLE_POOL   *mPageTablePool = NULL;

/**
  Initialize a buffer pool for page table use only.

  To reduce the potential split operation on page table, the pages reserved for
  page table should be allocated in the times of PAGE_TABLE_POOL_UNIT_PAGES and
  at the boundary of PAGE_TABLE_POOL_ALIGNMENT. So the page pool is always
  initialized with number of pages greater than or equal to the given PoolPages.

  Once the pages in the pool are used up, this method should be called again to
  reserve at least another PAGE_TABLE_POOL_UNIT_PAGES. But usually this won't
  happen in practice.

  @param PoolPages  The least page number of the pool to be created.

  @retval TRUE    The pool is initialized successfully.
  @retval FALSE   The memory is out of resource.
**/
BOOLEAN
InitializePageTablePool (
  IN UINTN           PoolPages
  )
{
  VOID          *Buffer;

  //
  // Always reserve at least PAGE_TABLE_POOL_UNIT_PAGES, including one page for
  // header.
  //
  PoolPages += 1;   // Add one page for header.
  PoolPages = ((PoolPages - 1) / PAGE_TABLE_POOL_UNIT_PAGES + 1) *
              PAGE_TABLE_POOL_UNIT_PAGES;
  Buffer = AllocateAlignedReservedPages (PoolPages, PAGE_TABLE_POOL_ALIGNMENT);
  if (Buffer == NULL) {
    DEBUG ((DEBUG_ERROR, "ERROR: Out of aligned pages\r\n"));
    return FALSE;
  }

  //
  // Link all pools into a list for easier track later.
  //
  if (mPageTablePool == NULL) {
    mPageTablePool = Buffer;
    mPageTablePool->NextPool = mPageTablePool;
  } else {
    ((PAGE_TABLE_POOL *)Buffer)->NextPool = mPageTablePool->NextPool;
    mPageTablePool->NextPool = Buffer;
    mPageTablePool = Buffer;
  }

  //
  // Reserve one page for pool header.
  //
  mPageTablePool->FreePages  = PoolPages - 1;
  mPageTablePool->Offset = EFI_PAGES_TO_SIZE (1);

  return TRUE;
}

/**
  This API provides a way to allocate memory for page table.

  This API can be called more than once to allocate memory for page tables.

  Allocates the number of 4KB pages and returns a pointer to the allocated
  buffer. The buffer returned is aligned on a 4KB boundary.

  If Pages is 0, then NULL is returned.
  If there is not enough memory remaining to satisfy the request, then NULL is
  returned.

  @param  Pages                 The number of 4 KB pages to allocate.

  @return A pointer to the allocated buffer or NULL if allocation fails.

**/
VOID *
AllocatePageTableMemory (
  IN UINTN           Pages
  )
{
  VOID          *Buffer;

  ASSERT (Pages != 0);

  //
  // Renew the pool if necessary.
  //
  if (mPageTablePool == NULL ||
      Pages > mPageTablePool->FreePages) {
    if (!InitializePageTablePool (Pages)) {
      return NULL;
    }
  }

  Buffer = (UINT8 *)mPageTablePool + mPageTablePool->Offset;

  mPageTablePool->Offset     += EFI_PAGES_TO_SIZE (Pages);
  mPageTablePool->FreePages  -= Pages;

  return Buffer;
}

/**
  Split 2M page to 4K.

  @param[in]      Context               The function context.
  @param[in]      SplitUnmapPage        Function to check whether to split and unmap.
  @param[in]      PhysicalAddress       Start physical address the 2M page covered.
  @param[in, out] PageEntry2M           Pointer to 2M page entry.

**/
VOID
Split2MPageTo4K (
  IN CONST VOID                         *Context,
  IN AMD_INTEL_EMU_UNMAP_SPLIT_PAGE     SplitUnmapPage,
  IN EFI_PHYSICAL_ADDRESS               PhysicalAddress,
  IN OUT UINT64                         *PageEntry2M
  )
{
  EFI_PHYSICAL_ADDRESS                  PhysicalAddress4K;
  UINTN                                 IndexOfPageTableEntries;
  PAGE_TABLE_4K_ENTRY                   *PageTableEntry;

  PageTableEntry = AllocatePageTableMemory (1);
  ASSERT (PageTableEntry != NULL);

  //
  // Fill in 2M page entry.
  //
  *PageEntry2M = (UINT64)(UINTN)PageTableEntry | IA32_PG_P | IA32_PG_RW;

  PhysicalAddress4K = PhysicalAddress;
  for (IndexOfPageTableEntries = 0; IndexOfPageTableEntries < 512; IndexOfPageTableEntries++, PageTableEntry++, PhysicalAddress4K += SIZE_4KB) {
    //
    // Fill in the Page Table entries
    //
    PageTableEntry->Uint64 = (UINT64)PhysicalAddress4K;
    PageTableEntry->Bits.UserSupervisor = 1;
    PageTableEntry->Bits.ReadWrite = 1;
    //
    // Page is not present by default.
    //
    if (!SplitUnmapPage (Context, PhysicalAddress4K, SIZE_4KB, PageTableEntry)) {
      PageTableEntry->Bits.Present = 1;
    }
  }
}

/**
  Split 1G page to 2M.

  @param[in]      Context               The function context.
  @param[in]      SplitUnmapPage        Function to check whether to split and unmap.
  @param[in]      PhysicalAddress       Start physical address the 1G page covered.
  @param[in, out] PageEntry1G           Pointer to 1G page entry.

**/
VOID
Split1GPageTo2M (
  IN CONST VOID                         *Context,
  IN AMD_INTEL_EMU_UNMAP_SPLIT_PAGE     SplitUnmapPage,
  IN EFI_PHYSICAL_ADDRESS               PhysicalAddress,
  IN OUT UINT64                         *PageEntry1G
  )
{
  EFI_PHYSICAL_ADDRESS                  PhysicalAddress2M;
  UINTN                                 IndexOfPageDirectoryEntries;
  PAGE_TABLE_ENTRY                      *PageDirectoryEntry;

  PageDirectoryEntry = AllocatePageTableMemory (1);
  ASSERT (PageDirectoryEntry != NULL);

  //
  // Fill in 1G page entry.
  //
  *PageEntry1G = (UINT64)(UINTN)PageDirectoryEntry | IA32_PG_P | IA32_PG_RW;

  PhysicalAddress2M = PhysicalAddress;
  for (IndexOfPageDirectoryEntries = 0; IndexOfPageDirectoryEntries < 512; IndexOfPageDirectoryEntries++, PageDirectoryEntry++, PhysicalAddress2M += SIZE_2MB) {
    if (SplitUnmapPage (Context, PhysicalAddress2M, SIZE_2MB, NULL)) {
      //
      // Need to split this 2M page that covers NULL or stack range.
      //
      Split2MPageTo4K (Context, SplitUnmapPage, PhysicalAddress2M, (UINT64 *)PageDirectoryEntry);
    } else {
      //
      // Fill in the Page Directory entries
      //
      PageDirectoryEntry->Uint64 = (UINT64)PhysicalAddress2M;
      PageDirectoryEntry->Bits.UserSupervisor = 1;
      PageDirectoryEntry->Bits.ReadWrite = 1;
      PageDirectoryEntry->Bits.Present = 1;
      PageDirectoryEntry->Bits.MustBe1 = 1;
    }
  }
}

/**
  Set one page of page table pool memory to be read-only.

  @param[in] PageTableBase    Base address of page table (CR3).
  @param[in] Address          Start address of a page to be set as read-only.

**/
VOID
SetPageTablePoolUnmapped (
  IN  UINTN                             PageTableBase,
  IN  EFI_PHYSICAL_ADDRESS              Address
  )
{
  UINTN                 Index;
  UINTN                 EntryIndex;
  EFI_PHYSICAL_ADDRESS  PhysicalAddress;
  UINT64                *PageTable;
  UINT64                *NewPageTable;
  UINT64                PageAttr;
  UINT64                LevelSize[5];
  UINT64                LevelMask[5];
  UINTN                 LevelShift[5];
  UINTN                 Level;
  UINT64                PoolUnitSize;

  ASSERT (PageTableBase != 0);

  //
  // Since the page table is always from page table pool, which is always
  // located at the boundary of PcdPageTablePoolAlignment, we just need to
  // set the whole pool unit to be read-only.
  //
  Address = Address & PAGE_TABLE_POOL_ALIGN_MASK;

  LevelShift[1] = PAGING_L1_ADDRESS_SHIFT;
  LevelShift[2] = PAGING_L2_ADDRESS_SHIFT;
  LevelShift[3] = PAGING_L3_ADDRESS_SHIFT;
  LevelShift[4] = PAGING_L4_ADDRESS_SHIFT;

  LevelMask[1] = PAGING_4K_ADDRESS_MASK_64;
  LevelMask[2] = PAGING_2M_ADDRESS_MASK_64;
  LevelMask[3] = PAGING_1G_ADDRESS_MASK_64;
  LevelMask[4] = PAGING_1G_ADDRESS_MASK_64;

  LevelSize[1] = SIZE_4KB;
  LevelSize[2] = SIZE_2MB;
  LevelSize[3] = SIZE_1GB;
  LevelSize[4] = SIZE_512GB;

  PageTable       = (UINT64 *)PageTableBase;
  PoolUnitSize    = PAGE_TABLE_POOL_UNIT_SIZE;

  for (Level = 4; Level > 0; --Level) {
    Index = ((UINTN)RShiftU64 (Address, LevelShift[Level]));
    Index &= PAGING_PAE_INDEX_MASK;

    PageAttr = PageTable[Index];
    if ((PageAttr & IA32_PG_PS) == 0) {
      //
      // Go to next level of table.
      //
      PageTable = (UINT64 *)(UINTN)(PageAttr & PAGING_4K_ADDRESS_MASK_64);
      continue;
    }

    if (PoolUnitSize >= LevelSize[Level]) {
      //
      // Clear R/W bit if current page granularity is not larger than pool unit
      // size.
      //
      if ((PageAttr & IA32_PG_P) != 0) {
        while (PoolUnitSize > 0) {
          //
          // PAGE_TABLE_POOL_UNIT_SIZE and PAGE_TABLE_POOL_ALIGNMENT are fit in
          // one page (2MB). Then we don't need to update attributes for pages
          // crossing page directory. ASSERT below is for that purpose.
          //
          ASSERT (Index < EFI_PAGE_SIZE/sizeof (UINT64));

          PageTable[Index] &= ~(UINT64)(IA32_PG_P | IA32_PG_RW | IA32_PG_US);
          PoolUnitSize    -= LevelSize[Level];

          ++Index;
        }
      }

      break;

    }
    //
    // The smaller granularity of page must be needed.
    //
    ASSERT (Level > 1);

    NewPageTable = AllocatePageTableMemory (1);
    ASSERT (NewPageTable != NULL);

    PhysicalAddress = PageAttr & LevelMask[Level];
    for (EntryIndex = 0;
          EntryIndex < EFI_PAGE_SIZE / sizeof (UINT64);
          ++EntryIndex) {
      NewPageTable[EntryIndex] = PhysicalAddress | IA32_PG_P | IA32_PG_RW;
      if (Level > 2) {
        NewPageTable[EntryIndex] |= IA32_PG_PS;
      }
      PhysicalAddress += LevelSize[Level - 1];
    }

    PageTable[Index] = (UINT64)(UINTN)NewPageTable | IA32_PG_P | IA32_PG_RW;
    PageTable = NewPageTable;
  }
}

/**
  Prevent the memory pages used for page table from been overwritten.

  @param[in] PageTableBase    Base address of page table (CR3).

**/
VOID
EnablePageTableProtection (
  IN  UINTN     PageTableBase
  )
{
  PAGE_TABLE_POOL         *HeadPool;
  PAGE_TABLE_POOL         *Pool;
  UINT64                  PoolSize;
  EFI_PHYSICAL_ADDRESS    Address;

  ASSERT (mPageTablePool != NULL);

  //
  // SetPageTablePoolUnmapped might update mPageTablePool. It's safer to
  // remember original one in advance.
  //
  HeadPool = mPageTablePool;
  Pool = HeadPool;
  do {
    Address  = (EFI_PHYSICAL_ADDRESS)(UINTN)Pool;
    PoolSize = Pool->Offset + EFI_PAGES_TO_SIZE (Pool->FreePages);

    //
    // The size of one pool must be multiple of PAGE_TABLE_POOL_UNIT_SIZE, which
    // is one of page size of the processor (2MB by default). Let's apply the
    // protection to them one by one.
    //
    while (PoolSize > 0) {
      SetPageTablePoolUnmapped (PageTableBase, Address);
      Address   += PAGE_TABLE_POOL_UNIT_SIZE;
      PoolSize  -= PAGE_TABLE_POOL_UNIT_SIZE;
    }

    Pool = Pool->NextPool;
  } while (Pool != HeadPool);
}

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
  )
{
  UINT32                                        RegEax;
  UINT32                                        RegEdx;
  UINT8                                         PhysicalAddressBits;
  EFI_PHYSICAL_ADDRESS                          PageAddress;
  UINTN                                         IndexOfPml4Entries;
  UINTN                                         IndexOfPdpEntries;
  UINTN                                         IndexOfPageDirectoryEntries;
  UINT32                                        NumberOfPml4EntriesNeeded;
  UINT32                                        NumberOfPdpEntriesNeeded;
  PAGE_MAP_AND_DIRECTORY_POINTER                *PageMapLevel4Entry;
  PAGE_MAP_AND_DIRECTORY_POINTER                *PageMap;
  PAGE_MAP_AND_DIRECTORY_POINTER                *PageDirectoryPointerEntry;
  PAGE_TABLE_ENTRY                              *PageDirectoryEntry;
  UINTN                                         TotalPagesNum;
  UINTN                                         BigPageAddress;
  VOID                                          *Hob;
  BOOLEAN                                       Page1GSupport;
  PAGE_TABLE_1G_ENTRY                           *PageDirectory1GEntry;

  Page1GSupport = FALSE;
  AsmCpuid (0x80000000, &RegEax, NULL, NULL, NULL);
  if (RegEax >= 0x80000001) {
    AsmCpuid (0x80000001, NULL, NULL, NULL, &RegEdx);
    if ((RegEdx & BIT26) != 0) {
      Page1GSupport = TRUE;
    }
  }

  //
  // Get physical address bits supported.
  //
  Hob = GetFirstHob (EFI_HOB_TYPE_CPU);
  if (Hob != NULL) {
    PhysicalAddressBits = ((EFI_HOB_CPU *)Hob)->SizeOfMemorySpace;
  } else {
    AsmCpuid (0x80000000, &RegEax, NULL, NULL, NULL);
    if (RegEax >= 0x80000008) {
      AsmCpuid (0x80000008, &RegEax, NULL, NULL, NULL);
      PhysicalAddressBits = (UINT8)RegEax;
    } else {
      PhysicalAddressBits = 36;
    }
  }

  //
  // IA-32e paging translates 48-bit linear addresses to 52-bit physical addresses.
  //
  ASSERT (PhysicalAddressBits <= 52);
  if (PhysicalAddressBits > 48) {
    PhysicalAddressBits = 48;
  }

  //
  // Calculate the table entries needed.
  //
  if (PhysicalAddressBits <= 39) {
    NumberOfPml4EntriesNeeded = 1;
    NumberOfPdpEntriesNeeded = (UINT32)LShiftU64 (1, (PhysicalAddressBits - 30));
  } else {
    NumberOfPml4EntriesNeeded = (UINT32)LShiftU64 (1, (PhysicalAddressBits - 39));
    NumberOfPdpEntriesNeeded = 512;
  }

  //
  // Pre-allocate big pages to avoid later allocations.
  //
  if (!Page1GSupport) {
    TotalPagesNum = (NumberOfPdpEntriesNeeded + 1) * NumberOfPml4EntriesNeeded + 1;
  } else {
    TotalPagesNum = NumberOfPml4EntriesNeeded + 1;
  }
  BigPageAddress = (UINTN)AllocatePageTableMemory (TotalPagesNum);
  ASSERT (BigPageAddress != 0);

  //
  // By architecture only one PageMapLevel4 exists - so lets allocate storage for it.
  //
  PageMap         = (VOID *)BigPageAddress;
  BigPageAddress += SIZE_4KB;

  PageMapLevel4Entry = PageMap;
  PageAddress        = 0;
  for (IndexOfPml4Entries = 0; IndexOfPml4Entries < NumberOfPml4EntriesNeeded; IndexOfPml4Entries++, PageMapLevel4Entry++) {
    //
    // Each PML4 entry points to a page of Page Directory Pointer entires.
    // So lets allocate space for them and fill them in in the IndexOfPdpEntries loop.
    //
    PageDirectoryPointerEntry = (VOID *)BigPageAddress;
    BigPageAddress += SIZE_4KB;

    //
    // Make a PML4 Entry
    //
    PageMapLevel4Entry->Uint64 = (UINT64)(UINTN)PageDirectoryPointerEntry;
    PageMapLevel4Entry->Bits.UserSupervisor = 1;
    PageMapLevel4Entry->Bits.ReadWrite = 1;
    PageMapLevel4Entry->Bits.Present = 1;

    if (Page1GSupport) {
      PageDirectory1GEntry = (VOID *)PageDirectoryPointerEntry;

      for (IndexOfPageDirectoryEntries = 0; IndexOfPageDirectoryEntries < 512; IndexOfPageDirectoryEntries++, PageDirectory1GEntry++, PageAddress += SIZE_1GB) {
        if (SplitUnmapPage (Context, PageAddress, SIZE_1GB, NULL)) {
          Split1GPageTo2M (Context, SplitUnmapPage, PageAddress, (UINT64 *)PageDirectory1GEntry);
        } else {
          //
          // Fill in the Page Directory entries
          //
          PageDirectory1GEntry->Uint64 = (UINT64)PageAddress;
          PageDirectory1GEntry->Bits.UserSupervisor = 1;
          PageDirectory1GEntry->Bits.ReadWrite = 1;
          PageDirectory1GEntry->Bits.Present = 1;
          PageDirectory1GEntry->Bits.MustBe1 = 1;
        }
      }
    } else {
      for (IndexOfPdpEntries = 0; IndexOfPdpEntries < NumberOfPdpEntriesNeeded; IndexOfPdpEntries++, PageDirectoryPointerEntry++) {
        //
        // Each Directory Pointer entries points to a page of Page Directory entires.
        // So allocate space for them and fill them in in the IndexOfPageDirectoryEntries loop.
        //
        PageDirectoryEntry = (VOID *)BigPageAddress;
        BigPageAddress += SIZE_4KB;

        //
        // Fill in a Page Directory Pointer Entries
        //
        PageDirectoryPointerEntry->Uint64 = (UINT64)(UINTN)PageDirectoryEntry;
        PageDirectoryPointerEntry->Bits.UserSupervisor = 1;
        PageDirectoryPointerEntry->Bits.ReadWrite = 1;
        PageDirectoryPointerEntry->Bits.Present = 1;

        for (IndexOfPageDirectoryEntries = 0; IndexOfPageDirectoryEntries < 512; IndexOfPageDirectoryEntries++, PageDirectoryEntry++, PageAddress += SIZE_2MB) {
          if (SplitUnmapPage (Context, PageAddress, SIZE_2MB, NULL)) {
            //
            // Need to split this 2M page that covers NULL or stack range.
            //
            Split2MPageTo4K (Context, SplitUnmapPage, PageAddress, (UINT64 *)PageDirectoryEntry);
          } else {
            //
            // Fill in the Page Directory entries
            //
            PageDirectoryEntry->Uint64 = (UINT64)PageAddress;
            PageDirectoryEntry->Bits.UserSupervisor = 1;
            PageDirectoryEntry->Bits.ReadWrite = 1;
            PageDirectoryEntry->Bits.Present = 1;
            PageDirectoryEntry->Bits.MustBe1 = 1;
          }
        }
      }

      for (; IndexOfPdpEntries < 512; IndexOfPdpEntries++, PageDirectoryPointerEntry++) {
        ZeroMem (
          PageDirectoryPointerEntry,
          sizeof (PAGE_MAP_AND_DIRECTORY_POINTER)
          );
      }
    }
  }

  //
  // For the PML4 entries we are not using fill in a null entry.
  //
  for (; IndexOfPml4Entries < 512; IndexOfPml4Entries++, PageMapLevel4Entry++) {
    ZeroMem (
      PageMapLevel4Entry,
      sizeof (PAGE_MAP_AND_DIRECTORY_POINTER)
      );
  }

  //
  // Protect the page table by marking the memory used for page table to be
  // unmapped for the Guest.
  //
  EnablePageTableProtection ((UINTN)PageMap);

  return (UINTN)PageMap;
}
