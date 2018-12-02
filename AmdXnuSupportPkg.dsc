## @file
# AMD XNU support Package
#
# Copyright (C) 2018, Download-Fritz.  All rights reserved.<BR>
#
#    This program and the accompanying materials
#    are licensed and made available under the terms and conditions of the BSD License
#    which accompanies this distribution. The full text of the license may be found at
#    http://opensource.org/licenses/bsd-license.php
#
#    THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
#    WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
#
##

[Defines]
  PLATFORM_NAME           = AmdXnuSupportPkg
  PLATFORM_GUID           = F5849560-95C7-42D4-B24E-26527E3C3248
  PLATFORM_VERSION        = 1.0
  SUPPORTED_ARCHITECTURES = X64
  BUILD_TARGETS           = RELEASE|DEBUG|NOOPT
  SKUID_IDENTIFIER        = DEFAULT
  DSC_SPECIFICATION       = 0x00010006

[LibraryClasses]
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  CpuLib|MdePkg/Library/BaseCpuLib/BaseCpuLib.inf
  DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  DebugPrintErrorLevelLib|MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf

[LibraryClasses.common.DXE_DRIVER]
  DebugLib|OcSupportPkg/Library/OcDebugLogLib/OcDebugLogLib.inf
  HobLib|MdePkg/Library/DxeHobLib/DxeHobLib.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  UefiDriverEntryPoint|MdePkg/Library/UefiDriverEntryPoint/UefiDriverEntryPoint.inf
  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf

[Components]
  AmdXnuSupportPkg/AmdIntelEmu/Dxe/AmdIntelEmuDxe.inf
  AmdXnuSupportPkg/AmdIntelEmu/Runtime/AmdIntelEmuRuntime.inf {
    <LibraryClasses>
      DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  }

[PcdsFixedAtBuild]
!if $(TARGET) == DEBUG
  gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0xFF
  gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0xFFFFFFFF
  gEfiMdePkgTokenSpaceGuid.PcdFixedDebugPrintErrorLevel|0xFFFFFFFF
!else
  # DEBUG_PRINT_ENABLED
  gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0x02
  # DEBUG_ERROR | DEBUG_WARN
  gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0x80000002
  gEfiMdePkgTokenSpaceGuid.PcdFixedDebugPrintErrorLevel|0x80000002
!endif

[BuildOptions]
  DEFINE AMDXNUSUPPORTPKG_BUILD_OPTIONS_GEN = -D DISABLE_NEW_DEPRECATED_INTERFACES

  INTEL:*_*_*_CC_FLAGS        = $(AMDXNUSUPPORTPKG_BUILD_OPTIONS_GEN)
  GCC:*_*_*_CC_FLAGS          = $(AMDXNUSUPPORTPKG_BUILD_OPTIONS_GEN)
  MSFT:*_*_*_CC_FLAGS         = $(AMDXNUSUPPORTPKG_BUILD_OPTIONS_GEN)
  XCODE:*_*_*_CC_FLAGS        = $(AMDXNUSUPPORTPKG_BUILD_OPTIONS_GEN)
  XCODE:RELEASE_*_*_CC_FLAGS == $(AMDXNUSUPPORTPKG_BUILD_OPTIONS_GEN) -flto
  XCODE:DEBUG_*_*_CC_FLAGS   == $(AMDXNUSUPPORTPKG_BUILD_OPTIONS_GEN) -flto
