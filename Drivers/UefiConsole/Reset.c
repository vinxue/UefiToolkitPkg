/** @file
  Main file for attrib shell level 2 function.

  (C) Copyright 2015 Hewlett-Packard Development Company, L.P.<BR>
  Copyright (c) 2009 - 2016, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "UefiConsole.h"

STATIC CONST SHELL_PARAM_ITEM ResetParamList[] = {
  {L"-w",    TypeValue},
  {L"-s",    TypeValue},
  {L"-c",    TypeValue},
  {L"-p",    TypeValue},
  {NULL,     TypeMax  }
};

/**
  Function for 'reset' command.

  @param[in] ImageHandle  Handle to the Image (NULL if Internal).
  @param[in] SystemTable  Pointer to the System Table (NULL if Internal).
**/
SHELL_STATUS
EFIAPI
ShellCommandRunReset (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS    Status;
  LIST_ENTRY    *Package;
  CONST CHAR16  *String;
  CHAR16        *ProblemParam;
  SHELL_STATUS  ShellStatus;

  ShellStatus = SHELL_SUCCESS;
  ProblemParam = NULL;

  //
  // parse the command line
  //
  Status = ConsoleCommandLineParse (ResetParamList, &Package, &ProblemParam, TRUE);
  if (EFI_ERROR (Status)) {
    if (Status == EFI_VOLUME_CORRUPTED && ProblemParam != NULL) {
      Print (L"reset: Unknown flag - '%s'\n");
      FreePool (ProblemParam);
      return (SHELL_INVALID_PARAMETER);
    } else {
      ASSERT (FALSE);
    }
  } else {
    //
    // check for "-?"
    //
    if (ConsoleCommandLineGetFlag (Package, L"-?")) {
      ASSERT (FALSE);
    } else if (ConsoleCommandLineGetRawValue (Package, 1) != NULL) {
      Print (L"reset:Too many arguments.\n");
      ShellStatus = SHELL_INVALID_PARAMETER;
    } else {
      //
      // check for warm reset flag, then shutdown reset flag, then cold (default) reset flag
      //
      if (ConsoleCommandLineGetFlag (Package, L"-w")) {
        if (ConsoleCommandLineGetFlag (Package, L"-s") || ConsoleCommandLineGetFlag (Package, L"-c") ||
            ConsoleCommandLineGetFlag (Package, L"-p")) {
          Print (L"reset:Too many arguments.\n");
          ShellStatus = SHELL_INVALID_PARAMETER;
        } else {
          String = ConsoleCommandLineGetValue (Package, L"-w");
          if (String != NULL) {
            gRT->ResetSystem (EfiResetWarm, EFI_SUCCESS, StrSize (String), (VOID *) String);
          } else {
            gRT->ResetSystem (EfiResetWarm, EFI_SUCCESS, 0, NULL);
          }
        }
      } else if (ConsoleCommandLineGetFlag (Package, L"-s")) {
        if (ConsoleCommandLineGetFlag (Package, L"-c") || ConsoleCommandLineGetFlag (Package, L"-p")) {
          Print (L"reset:Too many arguments.\n");
          ShellStatus = SHELL_INVALID_PARAMETER;
        } else {
          String = ConsoleCommandLineGetValue (Package, L"-s");
          if (String != NULL) {
            gRT->ResetSystem (EfiResetShutdown, EFI_SUCCESS, StrSize (String), (VOID *) String);
          } else {
            gRT->ResetSystem (EfiResetShutdown, EFI_SUCCESS, 0, NULL);
          }
        }
      } else if (ConsoleCommandLineGetFlag (Package, L"-p")) {
        if (ConsoleCommandLineGetFlag (Package, L"-c")) {
          Print (L"reset:Too many arguments.\n");
          ShellStatus = SHELL_INVALID_PARAMETER;
        } else {
          String = ConsoleCommandLineGetValue (Package, L"-p");
          if (String != NULL) {
            //
            // reset -p fastboot/bootloader/force-recovery
            //
            gRT->ResetSystem (EfiResetPlatformSpecific, EFI_SUCCESS, StrSize (String), (VOID *) String);
          } else {
            gRT->ResetSystem (EfiResetPlatformSpecific, EFI_SUCCESS, 0, NULL);
          }
        }
      } else {
        //
        // this is default so dont worry about flag...
        //
        String = ConsoleCommandLineGetValue (Package, L"-c");
        if (String != NULL) {
          gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, StrSize (String), (VOID *) String);
        } else {
          gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL);
        }
      }
    }
  }

  //
  // we should never get here... so the free and return are for formality more than use
  // as the ResetSystem function should not return...
  //

  //
  // free the command line package
  //
  ConsoleCommandLineFreeVarList (Package);

  //
  // return the status
  //
  return (ShellStatus);
}

