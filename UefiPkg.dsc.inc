## @file
# UefiPkg DSC include file for Platform DSC.
#
# Copyright (c) 2019 - 2021, Gavin Xue. All rights reserved.<BR>
# This program and the accompanying materials are licensed and made available under
# the terms and conditions of the BSD License that accompanies this distribution.
# The full text of the license may be found at
# http://opensource.org/licenses/bsd-license.php.
#
# THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
# WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
#
##

[LibraryClasses]
  PlatformFlashAccessLib|$(UEFI_PACKAGE)/Library/PlatformFlashAccessLib/PlatformFlashAccessLib.inf
  RamDebugLib|$(UEFI_PACKAGE)/Library/RamDebugLib/RamDebugLib.inf

[Components.X64]
  $(UEFI_PACKAGE)/Drivers/Dxe/PrintScreenLogger/PrintScreenLogger.inf
  $(UEFI_PACKAGE)/Drivers/Dxe/UefiConsole/UefiConsole.inf
  $(UEFI_PACKAGE)/Application/UefiTool/UefiTool.inf
  $(UEFI_PACKAGE)/Application/TcpTransport/TcpTransport.inf
  $(UEFI_PACKAGE)/Application/PartEdit/PartEdit.inf
  $(UEFI_PACKAGE)/Application/FlashTool/FlashTool.inf
  $(UEFI_PACKAGE)/Application/UefiAvb/UefiAvb.inf
  $(UEFI_PACKAGE)/Application/GptGen/GptGen.inf
  $(UEFI_PACKAGE)/Application/TscFreq/TscFreq.inf
  $(UEFI_PACKAGE)/Application/GopVer/GopVer.inf
  $(UEFI_PACKAGE)/Application/DumpLog/DumpLog.inf
  $(UEFI_PACKAGE)/Application/IshSlot/IshSlot.inf
  $(UEFI_PACKAGE)/Application/GenGuid/GenGuid.inf
  $(UEFI_PACKAGE)/Application/OtaUpdate/OtaUpdate.inf
  $(UEFI_PACKAGE)/Application/AmdCpuFreq/AmdCpuFreq.inf
  $(UEFI_PACKAGE)/Application/Smn/Smn.inf
  $(UEFI_PACKAGE)/Application/RamDiskApp/RamDiskApp.inf
  $(UEFI_PACKAGE)/Application/Cpuid/Cpuid.inf

[Components.IA32]
  $(UEFI_PACKAGE)/Drivers/Pei/RawDataInit/RawDataInit.inf
