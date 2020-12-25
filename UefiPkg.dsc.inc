## @file
# UefiPubPkg DSC include file for Platform DSC.
#
# Copyright (c) 2019 - 2020, Gavin Xue. All rights reserved.<BR>
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
  PlatformFlashAccessLib|UefiPubPkg/Library/PlatformFlashAccessLib/PlatformFlashAccessLib.inf
  RamDebugLib|UefiPubPkg/Library/RamDebugLib/RamDebugLib.inf

[Components.X64]
  UefiPubPkg/Drivers/Dxe/PrintScreenLogger/PrintScreenLogger.inf
  UefiPubPkg/Drivers/Dxe/UefiConsole/UefiConsole.inf
  UefiPubPkg/Application/UefiTool/UefiTool.inf
  UefiPubPkg/Application/TcpTransport/TcpTransport.inf
  UefiPubPkg/Application/PartEdit/PartEdit.inf
  UefiPubPkg/Application/FlashTool/FlashTool.inf
  UefiPubPkg/Application/UefiAvb/UefiAvb.inf
  UefiPubPkg/Application/GptGen/GptGen.inf
  UefiPubPkg/Application/TscFreq/TscFreq.inf
  UefiPubPkg/Application/GopVer/GopVer.inf
  UefiPubPkg/Application/DumpLog/DumpLog.inf
  UefiPubPkg/Application/IshSlot/IshSlot.inf
  UefiPubPkg/Application/GenGuid/GenGuid.inf
  UefiPubPkg/Application/OtaUpdate/OtaUpdate.inf
  UefiPubPkg/Application/AmdCpuFreq/AmdCpuFreq.inf

[Components.IA32]
  UefiPubPkg/Drivers/Pei/RawDataInit/RawDataInit.inf
