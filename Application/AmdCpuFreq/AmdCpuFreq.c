/** @file
  A UEFI tool for AMD CPU Frequency.

  Copyright (c) 2020, Gavin Xue. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include "AmdCpuFreq.h"


/**
  UEFI application entry point which has an interface similar to a
  standard C main function.

  The ShellCEntryLib library instance wrappers the actual UEFI application
  entry point and calls this ShellAppMain function.

  @param[in]  Argc  The number of parameters.
  @param[in]  Argv  The array of pointers to parameters.

  @retval  0               The application exited normally.
  @retval  Other           An error occurred.

**/
INTN
EFIAPI
ShellAppMain (
  IN UINTN                     Argc,
  IN CHAR16                    **Argv
  )
{
  HW_PSTATE_STATUS_MSR         HwPstateSts;
  UINT64                       CpuFreq;

  HwPstateSts.Value = AsmReadMsr64 (MSR_HW_PSTATE_STATUS);
  CpuFreq = DivU64x32 (MultU64x64 (200, HwPstateSts.Field.CurCpuFid), (UINT32) HwPstateSts.Field.CurCpuDfsId);

  Print (L"CPU Frequency is: %d MHz\n", CpuFreq);

  return 0;
}
