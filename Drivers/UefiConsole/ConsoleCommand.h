/** @file
  function definitions for internal to UEFI console functions.

  Copyright (c) 2019, Gavin Xue. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#ifndef _CONSOLE_COMMAND_H_
#define _CONSOLE_COMMAND_H_

/**
  Dump some hexadecimal data to the screen.

  @param[in] Indent     How many spaces to indent the output.
  @param[in] Offset     The offset of the printing.
  @param[in] DataSize   The size in bytes of UserData.
  @param[in] UserData   The data to print out.

**/
VOID
EFIAPI
DumpHex (
  IN UINTN        Indent,
  IN UINTN        Offset,
  IN UINTN        DataSize,
  IN VOID         *UserData
  );

/**
  Checks if a command string has been registered for CommandString and if so it runs
  the previously registered handler for that command with the command line.

  If CommandString is NULL, then ASSERT().

  If Sections is specified, then each section name listed will be compared in a casesensitive
  manner, to the section names described in Appendix B UEFI Shell 2.0 spec. If the section exists,
  it will be appended to the returned help text. If the section does not exist, no
  information will be returned. If Sections is NULL, then all help text information
  available will be returned.

  @param[in]  CommandString          Pointer to the command name.  This is the name
                                     found on the command line in the shell.
  @param[in, out] RetVal             Pointer to the return vaule from the command handler.

  @param[in, out]  CanAffectLE       indicates whether this command's return value
                                     needs to be placed into LASTERROR environment variable.

  @retval RETURN_SUCCESS            The handler was run.
  @retval RETURN_NOT_FOUND          The CommandString did not match a registered
                                    command name.
  @sa SHELL_RUN_COMMAND
**/
RETURN_STATUS
EFIAPI
ConsoleCommandRunCommandHandler (
  IN CONST CHAR16               *CommandString,
  IN OUT SHELL_STATUS           *RetVal,
  IN OUT BOOLEAN                *CanAffectLE OPTIONAL
  );

/**
  Indicate that the current shell or script should exit.

  @param[in] ScriptOnly   TRUE if exiting a script; FALSE otherwise.
  @param[in] ErrorCode    The 64 bit error code to return.
**/
VOID
EFIAPI
ConsoleCommandRegisterExit (
  IN BOOLEAN      ScriptOnly,
  IN CONST UINT64 ErrorCode
  );

/**
  Retrieve the Exit indicator.

  @retval TRUE      Exit was indicated.
  @retval FALSE     Exis was not indicated.
**/
BOOLEAN
EFIAPI
ConsoleCommandGetExit (
  VOID
  );

/**
  Destructor for the function, free any resources.

  @param VOID

  @retval EFI_SUCCESS    This function always returns success

**/
EFI_STATUS
EFIAPI
ConsoleCommandsDestructor (
  VOID
  );

/**
  Constructor for the Console Commands.

  @param ImageHandle         The image handle of the process
  @param SystemTable         The EFI System Table pointer

  @retval EFI_SUCCESS        The shell command handlers were installed sucessfully
  @retval EFI_UNSUPPORTED    The shell level required was not found.

**/
EFI_STATUS
EFIAPI
ConsoleCommandsConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  );

/**
  Returns the help MAN fileName for a given shell command.

  @retval !NULL   The unicode string of the MAN filename.
  @retval NULL    An error ocurred.

**/
typedef
CONST CHAR16 *
(EFIAPI *SHELL_GET_MAN_FILENAME) (
  VOID
  );

/**
  Runs a shell command on a given command line.

  The specific operation of a given shell command is specified in the UEFI Shell
  Specification 2.0, or in the source of the given command.

  Upon completion of the command run the shell protocol and environment variables
  may have been updated due to the operation.

  @param[in] ImageHandle              The ImageHandle to the app, or NULL if
                                      the command built into shell.
  @param[in] SystemTable              The pointer to the system table.

  @retval  RETURN_SUCCESS             The shell command was sucessful.
  @retval  RETURN_UNSUPPORTED         The command is not supported.
**/
typedef
SHELL_STATUS
(EFIAPI *SHELL_RUN_COMMAND) (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  );

typedef struct {
  LIST_ENTRY                  Link;
  CHAR16                      *CommandString;
  SHELL_GET_MAN_FILENAME      GetManFileName;
  SHELL_RUN_COMMAND           CommandHandler;
  BOOLEAN                     LastError;
  EFI_HANDLE                  HiiHandle;
  EFI_STRING_ID               ManFormatHelp;
} SHELL_COMMAND_INTERNAL_LIST_ENTRY;

/**
  Function for 'mem' command.

  @param[in] ImageHandle  Handle to the Image (NULL if Internal).
  @param[in] SystemTable  Pointer to the System Table (NULL if Internal).
**/
SHELL_STATUS
EFIAPI
ShellCommandRunMem (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  );

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
  );

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
  );

#endif
