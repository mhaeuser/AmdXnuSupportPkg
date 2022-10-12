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
  CpuExceptionHandlerLib|MdeModulePkg/Library/CpuExceptionHandlerLibNull/CpuExceptionHandlerLibNull.inf
  DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  DebugPrintErrorLevelLib|MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  IoLib|MdePkg/Library/BaseIoLibIntrinsic/BaseIoLibIntrinsic.inf
  LocalApicLib|UefiCpuPkg/Library/BaseXApicX2ApicLib/BaseXApicX2ApicLib.inf

  CacheMaintenanceLib|MdePkg/Library/BaseCacheMaintenanceLib/BaseCacheMaintenanceLib.inf
  PeCoffExtraActionLib|MdePkg/Library/BasePeCoffExtraActionLibNull/BasePeCoffExtraActionLibNull.inf

[LibraryClasses.common.DXE_DRIVER]
  HobLib|MdePkg/Library/DxeHobLib/DxeHobLib.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  DxeServicesTableLib|MdePkg/Library/DxeServicesTableLib/DxeServicesTableLib.inf
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  UefiDriverEntryPoint|MdePkg/Library/UefiDriverEntryPoint/UefiDriverEntryPoint.inf
  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  
  CpuExceptionHandlerLib|UefiCpuPkg/Library/CpuExceptionHandlerLib/DxeCpuExceptionHandlerLib.inf
 
  #DebugLib|OcSupportPkg/Library/OcDebugLogLib/OcDebugLogLib.inf
  #SerialPortLib|MdePkg/Library/UefiSerialPortLibConOut/BaseSerialPortLibNull.inf
  DebugLib|MdePkg/Library/BaseDebugLibSerialPort/BaseDebugLibSerialPort.inf
  SerialPortLib|MdeModulePkg/Library/BaseSerialPortLib16550/BaseSerialPortLib16550.inf

  PlatformHookLib|MdeModulePkg/Library/BasePlatformHookLibNull/BasePlatformHookLibNull.inf
  PciLib|MdePkg/Library/BasePciLibCf8/BasePciLibCf8.inf
  PciCf8Lib|MdePkg/Library/BasePciCf8Lib/BasePciCf8Lib.inf

  SynchronizationLib|MdePkg/Library/BaseSynchronizationLib/BaseSynchronizationLib.inf
  PeCoffLib|MdePkg/Library/BasePeCoffLib/BasePeCoffLib.inf
  PeCoffGetEntryPointLib|MdePkg/Library/BasePeCoffGetEntryPointLib/BasePeCoffGetEntryPointLib.inf
  TimerLib|UefiCpuPkg/Library/SecPeiDxeTimerLibUefiCpu/SecPeiDxeTimerLibUefiCpu.inf

  DebugAgentLib|MdeModulePkg/Library/DebugAgentLibNull/DebugAgentLibNull.inf
  UefiCpuLib|UefiCpuPkg/Library/BaseUefiCpuLib/BaseUefiCpuLib.inf
  MtrrLib|UefiCpuPkg/Library/MtrrLib/MtrrLib.inf

  #MpInitLib|UefiCpuPkg/Library/MpInitLib/DxeMpInitLib.inf
  MpInitLib|AmdXnuSupportPkg/Library/DxeMpInitLibMpService/DxeMpInitLibMpService.inf

[PcdsFixedAtBuild]
  gEfiMdePkgTokenSpaceGuid.PcdFSBClock|200000000

[Components]
  AmdXnuSupportPkg/AmdIntelEmu/Dxe/AmdIntelEmuDxe.inf
  AmdXnuSupportPkg/AmdIntelEmu/Runtime/AmdIntelEmuRuntime.inf

[PcdsFixedAtBuild]
!if $(TARGET) == DEBUG
  gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0xEF
  gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0xFFFFFFFF
  gEfiMdePkgTokenSpaceGuid.PcdFixedDebugPrintErrorLevel|0xFFFFFFFF
!else
  # DEBUG_PRINT_ENABLED
  gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0x02
  # DEBUG_ERROR | DEBUG_WARN
  gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0x80000002
  gEfiMdePkgTokenSpaceGuid.PcdFixedDebugPrintErrorLevel|0x80000002
!endif

  gEfiMdeModulePkgTokenSpaceGuid.PcdSerialUseHardwareFlowControl|TRUE

[BuildOptions]
  INTEL:*_*_*_CC_FLAGS       = /DDISABLE_NEW_DEPRECATED_INTERFACES
  MSFT:*_*_*_CC_FLAGS        = /D DISABLE_NEW_DEPRECATED_INTERFACES
  GCC:*_*_*_CC_FLAGS         = -D DISABLE_NEW_DEPRECATED_INTERFACES
  XCODE:*_*_*_CC_FLAGS       = -D DISABLE_NEW_DEPRECATED_INTERFACES
  XCODE:RELEASE_*_*_CC_FLAGS = -flto
  XCODE:DEBUG_*_*_CC_FLAGS   = -flto
