/** @file
  A UEFI tool for getting AMD GOP version.

  Copyright (c) 2019, Gavin Xue. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include "GopVer.h"


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
  EFI_STATUS                    Status;
  UINTN                         HandleCount;
  EFI_HANDLE                    *Handles = NULL;
  UINTN                         Index;
  EFI_COMPONENT_NAME2_PROTOCOL  *ComponentName;
  CHAR16                        *DriverName;

  Status = gBS->LocateHandleBuffer(
                  ByProtocol,
                  &gEfiDriverBindingProtocolGuid,
                  NULL,
                  &HandleCount,
                  &Handles
                  );
  if (EFI_ERROR (Status)) {
    Print (L"LocateHandleBuffer failed.\n");
    return Status;
  }

  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->OpenProtocol(
                    Handles[Index],
                    &gEfiComponentName2ProtocolGuid,
                    (VOID**)&ComponentName,
                    gImageHandle,
                    NULL,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
    if (!EFI_ERROR (Status)) {
      Status = ComponentName->GetDriverName (
                                ComponentName,
                                ComponentName->SupportedLanguages,
                                &DriverName
                                );
      if (EFI_ERROR (Status)) {
        Print (L"GetDriverName failed.\n");
        return Status;
      }

      if (!StrnCmp (DriverName, L"AMD GOP ", StrLen (L"AMD GOP "))) {
        Print (L"GOP Version: %s\n", DriverName);
        break;
      }
    }
  }

  if (Index >= HandleCount) {
    Print (L"Not found GOP.\n");
  }

  if (Handles != NULL) {
    FreePool (Handles);
  }

  return 0;
}
