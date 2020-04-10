/** @file
  A UEFI tool for TSC frequency.

  Copyright (c) 2019 - 2020, Gavin Xue. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include "TscFreq.h"

EFI_STATUS
EFIAPI
GetTscFrequency (
  OUT UINT64         *TscFrequency
  )
{
  UINT64             TimeStampCounterStart;
  UINT64             TimeStampCounterEnd;

  if (TscFrequency == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Read timestamp counter.
  //
  TimeStampCounterStart = AsmReadTsc ();
  gBS->Stall (100);
  TimeStampCounterEnd   = AsmReadTsc ();

  //
  // Calculate TSC frequency.
  //
  *TscFrequency = DivU64x32Remainder (TimeStampCounterEnd - TimeStampCounterStart, 100, NULL);

  return EFI_SUCCESS;
}

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
  EFI_STATUS         Status;
  UINT64             TscFrequency;

  Status = GetTscFrequency (&TscFrequency);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Print (L"TSC Frequency: %d.%d GHz\n", TscFrequency / 1000, (TscFrequency % 1000) / 10);

  return 0;
}
