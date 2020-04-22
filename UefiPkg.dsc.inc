## @file
# UefiPkg DSC include file for Platform DSC.
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
  PlatformFlashAccessLib|UefiPkg/Library/PlatformFlashAccessLib/PlatformFlashAccessLib.inf
  RamDebugLib|UefiPkg/Library/RamDebugLib/RamDebugLib.inf
  TeeDevLib|UefiPkg/Library/TeeDevLib/TeeDevLib.inf
  TeeDrvLib|UefiPkg/Library/TeeDrvLib/TeeDrvLib.inf
  TeeClientApiLib|UefiPkg/Library/TeeClientApiLib/TeeClientApiLib.inf

[Components.X64]
  UefiPkg/Drivers/Dxe/PrintScreenLogger/PrintScreenLogger.inf
  UefiPkg/Drivers/Dxe/UefiConsole/UefiConsole.inf
  UefiPkg/Application/UefiTool/UefiTool.inf
  UefiPkg/Application/TcpTransport/TcpTransport.inf
  UefiPkg/Application/PartEdit/PartEdit.inf
  UefiPkg/Application/FlashTool/FlashTool.inf
  UefiPkg/Application/UefiAvb/UefiAvb.inf
  UefiPkg/Application/GptGen/GptGen.inf
  UefiPkg/Application/TscFreq/TscFreq.inf
  UefiPkg/Application/GopVer/GopVer.inf
  UefiPkg/Application/DumpLog/DumpLog.inf
  UefiPkg/Application/IshSlot/IshSlot.inf
  UefiPkg/Application/GenGuid/GenGuid.inf
  UefiPkg/Application/TeeApp/TeeApp.inf

[Components.IA32]
  UefiPkg/Drivers/Pei/RawDataInit/RawDataInit.inf
