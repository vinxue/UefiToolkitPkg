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

#ifndef _CONSOLE_PARAMETERS_H_
#define _CONSOLE_PARAMETERS_H_

typedef enum {
  Internal_Command,
  Script_File_Name,
  Efi_Application,
  File_Sys_Change,
  Unknown_Invalid
} SHELL_OPERATION_TYPES;

/**
  Return the next parameter from a command line string.

  This function moves the next parameter from Walker into TempParameter and moves
  Walker up past that parameter for recursive calling.  When the final parameter
  is moved *Walker will be set to NULL;

  Temp Parameter must be large enough to hold the parameter before calling this
  function.

  This will also remove all remaining ^ characters after processing.

  @param[in, out] Walker          pointer to string of command line.  Adjusted to
                                  reminaing command line on return
  @param[in, out] TempParameter   pointer to string of command line item extracted.
  @param[in]      Length          buffer size of TempParameter.
  @param[in]      StripQuotation  if TRUE then strip the quotation marks surrounding
                                  the parameters.

  @return   EFI_INALID_PARAMETER  A required parameter was NULL or pointed to a NULL or empty string.
  @return   EFI_NOT_FOUND         A closing " could not be found on the specified string
**/
EFI_STATUS
GetNextParameter (
  IN OUT CHAR16   **Walker,
  IN OUT CHAR16   **TempParameter,
  IN CONST UINTN  Length,
  IN BOOLEAN      StripQuotation
  );

/**
  function to populate Argc and Argv.

  This function parses the CommandLine and divides it into standard C style Argc/Argv
  parameters for inclusion in EFI_SHELL_PARAMETERS_PROTOCOL.  this supports space
  delimited and quote surrounded parameter definition.

  @param[in] CommandLine          String of command line to parse
  @param[in] StripQuotation       if TRUE then strip the quotation marks surrounding
                                  the parameters.
  @param[in, out] Argv            pointer to array of strings; one for each parameter
  @param[in, out] Argc            pointer to number of strings in Argv array

  @return EFI_SUCCESS             the operation was sucessful
  @return EFI_OUT_OF_RESOURCES    a memory allocation failed.
**/
EFI_STATUS
ParseCommandLineToArgs (
  IN CONST CHAR16 *CommandLine,
  IN BOOLEAN      StripQuotation,
  IN OUT CHAR16   ***Argv,
  IN OUT UINTN    *Argc
  );

/**
  Funcion will replace the current Argc and Argv in the ShellParameters protocol
  structure with Argv and Argc.  The current values are de-allocated and the
  OldArgv must not be deallocated by the caller.

  @param[in, out] ShellParameters       pointer to parameter structure to modify
  @param[in] OldArgv                    pointer to old list of parameters
  @param[in] OldArgc                    pointer to old number of items in Argv list
**/
VOID
RestoreArgcArgv (
  IN OUT EFI_SHELL_PARAMETERS_PROTOCOL  *ShellParameters,
  IN CHAR16                             ***OldArgv,
  IN UINTN                              *OldArgc
  );

/**
  Funcion will replace the current Argc and Argv in the ShellParameters protocol
  structure by parsing NewCommandLine.  The current values are returned to the
  user.

  @param[in, out] ShellParameters       pointer to parameter structure to modify
  @param[in] NewCommandLine             the new command line to parse and use
  @param[in] Type                       the type of operation.
  @param[out] OldArgv                   pointer to old list of parameters
  @param[out] OldArgc                   pointer to old number of items in Argv list

  @retval   EFI_SUCCESS                 operation was sucessful, Argv and Argc are valid
  @retval   EFI_OUT_OF_RESOURCES        a memory allocation failed.
**/
EFI_STATUS
UpdateArgcArgv (
  IN OUT EFI_SHELL_PARAMETERS_PROTOCOL  *ShellParameters,
  IN CONST CHAR16                       *NewCommandLine,
  IN SHELL_OPERATION_TYPES              Type,
  OUT CHAR16                            ***OldArgv,
  OUT UINTN                             *OldArgc
  );

/**
  Checks the command line arguments passed against the list of valid ones.
  Optionally removes NULL values first.

  If no initialization is required, then return RETURN_SUCCESS.

  @param[in] CheckList          The pointer to list of parameters to check.
  @param[out] CheckPackage      The package of checked values.
  @param[out] ProblemParam      Optional pointer to pointer to unicode string for
                                the paramater that caused failure.
  @param[in] AutoPageBreak      Will automatically set PageBreakEnabled.
  @param[in] AlwaysAllowNumbers Will never fail for number based flags.

  @retval EFI_SUCCESS           The operation completed sucessfully.
  @retval EFI_OUT_OF_RESOURCES  A memory allocation failed.
  @retval EFI_INVALID_PARAMETER A parameter was invalid.
  @retval EFI_VOLUME_CORRUPTED  The command line was corrupt.
  @retval EFI_DEVICE_ERROR      The commands contained 2 opposing arguments.  One
                                of the command line arguments was returned in
                                ProblemParam if provided.
  @retval EFI_NOT_FOUND         A argument required a value that was missing.
                                The invalid command line argument was returned in
                                ProblemParam if provided.
**/
EFI_STATUS
EFIAPI
ConsoleCommandLineParse (
  IN CONST SHELL_PARAM_ITEM     *CheckList,
  OUT LIST_ENTRY                **CheckPackage,
  OUT CHAR16                    **ProblemParam OPTIONAL,
  IN BOOLEAN                    AutoPageBreak
  );

/**
  Safely append with automatic string resizing given length of Destination and
  desired length of copy from Source.

  append the first D characters of Source to the end of Destination, where D is
  the lesser of Count and the StrLen() of Source. If appending those D characters
  will fit within Destination (whose Size is given as CurrentSize) and
  still leave room for a NULL terminator, then those characters are appended,
  starting at the original terminating NULL of Destination, and a new terminating
  NULL is appended.

  If appending D characters onto Destination will result in a overflow of the size
  given in CurrentSize the string will be grown such that the copy can be performed
  and CurrentSize will be updated to the new size.

  If Source is NULL, there is nothing to append, just return the current buffer in
  Destination.

  if Destination is NULL, then ASSERT()
  if Destination's current length (including NULL terminator) is already more then
  CurrentSize, then ASSERT()

  @param[in, out] Destination   The String to append onto
  @param[in, out] CurrentSize   on call the number of bytes in Destination.  On
                                return possibly the new size (still in bytes).  if NULL
                                then allocate whatever is needed.
  @param[in]      Source        The String to append from
  @param[in]      Count         Maximum number of characters to append.  if 0 then
                                all are appended.

  @return Destination           return the resultant string.
**/
CHAR16 *
EFIAPI
ConsoleStrnCatGrow (
  IN OUT CHAR16           **Destination,
  IN OUT UINTN            *CurrentSize,
  IN     CONST CHAR16     *Source,
  IN     UINTN            Count
  );

/**
  Check if a Unicode character is a hexadecimal character.

  This internal function checks if a Unicode character is a
  numeric character.  The valid hexadecimal characters are
  L'0' to L'9', L'a' to L'f', or L'A' to L'F'.

  @param  Char  The character to check against.

  @retval TRUE  If the Char is a hexadecmial character.
  @retval FALSE If the Char is not a hexadecmial character.

**/
BOOLEAN
EFIAPI
ConsoleIsHexaDecimalDigitCharacter (
  IN      CHAR16                    Char
  );

/**
  Check if a Unicode character is a decimal character.

  This internal function checks if a Unicode character is a
  decimal character.  The valid characters are
  L'0' to L'9'.


  @param  Char  The character to check against.

  @retval TRUE  If the Char is a hexadecmial character.
  @retval FALSE If the Char is not a hexadecmial character.

**/
BOOLEAN
EFIAPI
ConsoleIsDecimalDigitCharacter (
  IN      CHAR16                    Char
  );

/**
  Frees shell variable list that was returned from ShellCommandLineParse.

  This function will free all the memory that was used for the CheckPackage
  list of postprocessed shell arguments.

  this function has no return value.

  if CheckPackage is NULL, then return

  @param CheckPackage           the list to de-allocate
  **/
VOID
EFIAPI
ConsoleCommandLineFreeVarList (
  IN LIST_ENTRY                 *CheckPackage
  );

/**
  Frees shell variable list that was returned from ShellCommandLineParse.

  This function will free all the memory that was used for the CheckPackage
  list of postprocessed shell arguments.

  this function has no return value.

  if CheckPackage is NULL, then return

  @param CheckPackage           the list to de-allocate
  **/
VOID
EFIAPI
ConsoleCommandLineFreeVarList (
  IN LIST_ENTRY                 *CheckPackage
  );

/**
  Check if a Unicode character is a hexadecimal character.

  This internal function checks if a Unicode character is a
  numeric character.  The valid hexadecimal characters are
  L'0' to L'9', L'a' to L'f', or L'A' to L'F'.

  @param  Char  The character to check against.

  @retval TRUE  If the Char is a hexadecmial character.
  @retval FALSE If the Char is not a hexadecmial character.

**/
BOOLEAN
EFIAPI
ConsoleIsHexaDecimalDigitCharacter (
  IN      CHAR16                    Char
  );

/**
  Check if a Unicode character is a decimal character.

  This internal function checks if a Unicode character is a
  decimal character.  The valid characters are
  L'0' to L'9'.


  @param  Char  The character to check against.

  @retval TRUE  If the Char is a hexadecmial character.
  @retval FALSE If the Char is not a hexadecmial character.

**/
BOOLEAN
EFIAPI
ConsoleIsDecimalDigitCharacter (
  IN      CHAR16                    Char
  );

/**
  returns the number of command line value parameters that were parsed.

  this will not include flags.

  @param[in] CheckPackage       The package of parsed command line arguments.

  @retval (UINTN)-1     No parsing has ocurred
  @return other         The number of value parameters found
**/
UINTN
EFIAPI
ConsoleCommandLineGetCount (
  IN CONST LIST_ENTRY              *CheckPackage
  );

/**
  Returns value from command line argument.

  Value parameters are in the form of "-<Key> value" or "/<Key> value".

  If CheckPackage is NULL, then return NULL.

  @param[in] CheckPackage       The package of parsed command line arguments.
  @param[in] KeyString          The Key of the command line argument to check for.

  @retval NULL                  The flag is not on the command line.
  @retval !=NULL                The pointer to unicode string of the value.
**/
CONST CHAR16 *
EFIAPI
ConsoleCommandLineGetValue (
  IN CONST LIST_ENTRY           *CheckPackage,
  IN CHAR16                     *KeyString
  );

/**
  Returns raw value from command line argument.

  Raw value parameters are in the form of "value" in a specific position in the list.

  If CheckPackage is NULL, then return NULL.

  @param[in] CheckPackage       The package of parsed command line arguments.
  @param[in] Position           The position of the value.

  @retval NULL                  The flag is not on the command line.
  @retval !=NULL                The pointer to unicode string of the value.
  **/
CONST CHAR16 *
EFIAPI
ConsoleCommandLineGetRawValue (
  IN CONST LIST_ENTRY           *CONST CheckPackage,
  IN UINTN                      Position
  );

/**
  Function to determin if an entire string is a valid number.

  If Hex it must be preceeded with a 0x or has ForceHex, set TRUE.

  @param[in] String       The string to evaluate.
  @param[in] ForceHex     TRUE - always assume hex.
  @param[in] StopAtSpace  TRUE to halt upon finding a space, FALSE to keep going.

  @retval TRUE        It is all numeric (dec/hex) characters.
  @retval FALSE       There is a non-numeric character.
**/
BOOLEAN
EFIAPI
ConsoleIsHexOrDecimalNumber (
  IN CONST CHAR16   *String,
  IN CONST BOOLEAN  ForceHex,
  IN CONST BOOLEAN  StopAtSpace
  );

/**
  Function to verify and convert a string to its numerical value.

  If Hex it must be preceeded with a 0x, 0X, or has ForceHex set TRUE.

  @param[in] String       The string to evaluate.
  @param[out] Value       Upon a successful return the value of the conversion.
  @param[in] ForceHex     TRUE - always assume hex.
  @param[in] StopAtSpace  FALSE to skip spaces.

  @retval EFI_SUCCESS             The conversion was successful.
  @retval EFI_INVALID_PARAMETER   String contained an invalid character.
  @retval EFI_NOT_FOUND           String was a number, but Value was NULL.
**/
EFI_STATUS
EFIAPI
ConsoleConvertStringToUint64 (
  IN CONST CHAR16   *String,
     OUT   UINT64   *Value,
  IN CONST BOOLEAN  ForceHex,
  IN CONST BOOLEAN  StopAtSpace
  );

/**
  Checks for presence of a flag parameter

  flag arguments are in the form of "-<Key>" or "/<Key>", but do not have a value following the key

  if CheckPackage is NULL then return FALSE.
  if KeyString is NULL then ASSERT()

  @param CheckPackage           The package of parsed command line arguments
  @param KeyString              the Key of the command line argument to check for

  @retval TRUE                  the flag is on the command line
  @retval FALSE                 the flag is not on the command line
  **/
BOOLEAN
EFIAPI
ConsoleCommandLineGetFlag (
  IN CONST LIST_ENTRY          *CONST CheckPackage,
  IN CONST CHAR16              *CONST KeyString
  );

#endif
