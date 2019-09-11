/** @file
  Main file for exit shell level 1 function.

  (C) Copyright 2015 Hewlett-Packard Development Company, L.P.<BR>
  Copyright (c) 2009 - 2011, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "UefiConsole.h"

STATIC CONST SHELL_PARAM_ITEM ParamList[] = {
  {NULL, TypeMax}
};

/**
  Function for 'exit' command.

  @param[in] ImageHandle  Handle to the Image (NULL if Internal).
  @param[in] SystemTable  Pointer to the System Table (NULL if Internal).
**/
SHELL_STATUS
EFIAPI
ShellCommandRunExit (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS          Status;
  LIST_ENTRY          *Package;
  CHAR16              *ProblemParam;
  SHELL_STATUS        ShellStatus;
  UINT64              RetVal;
  CONST CHAR16        *Return;

  ShellStatus         = SHELL_SUCCESS;

  //
  // parse the command line
  //
  Status = ConsoleCommandLineParse (ParamList, &Package, &ProblemParam, TRUE);
  if (EFI_ERROR (Status)) {
    if (Status == EFI_VOLUME_CORRUPTED && ProblemParam != NULL) {
      Print (L"The argument '%s' is incorrect.\n", ProblemParam);
      FreePool (ProblemParam);
      ShellStatus = SHELL_INVALID_PARAMETER;
    } else {
      ASSERT (FALSE);
    }
  } else {

    //
    // return the specified error code
    //
    Return = ConsoleCommandLineGetRawValue (Package, 1);
    if (Return != NULL) {
      Status = ConsoleConvertStringToUint64 (Return, &RetVal, FALSE, FALSE);
      if (EFI_ERROR (Status)) {
        Print (L"exit: Invalid argument - '%s'\n", Return);
        ShellStatus = SHELL_INVALID_PARAMETER;
      } else {
        //
        // If we are in a batch file and /b then pass TRUE otherwise false...
        //
        ConsoleCommandRegisterExit (FALSE, RetVal);

        ShellStatus = SHELL_SUCCESS;
      }
    } else {
      // If we are in a batch file and /b then pass TRUE otherwise false...
      //
      ConsoleCommandRegisterExit (FALSE, 0);

      ShellStatus = SHELL_SUCCESS;
    }

    ConsoleCommandLineFreeVarList (Package);
  }
  return (ShellStatus);
}
