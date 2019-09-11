/** @file
  A UEFI tool for console.

  Copyright (c) 2019, Gavin Xue. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include "UefiConsole.h"

extern EFI_SHELL_PARAMETERS_PROTOCOL *NewShellParametersProtocol;

/**
  Return the next parameter's end from a command line string.

  @param[in] String        the string to parse
**/
CONST CHAR16 *
FindEndOfParameter (
  IN CONST CHAR16 *String
  )
{
  CONST CHAR16 *First;
  CONST CHAR16 *CloseQuote;

  First = FindFirstCharacter (String, L" \"", L'^');

  //
  // nothing, all one parameter remaining
  //
  if (*First == CHAR_NULL) {
    return (First);
  }

  //
  // If space before a quote (or neither found, i.e. both CHAR_NULL),
  // then that's the end.
  //
  if (*First == L' ') {
    return (First);
  }

  CloseQuote = FindFirstCharacter (First + 1, L"\"", L'^');

  //
  // We did not find a terminator...
  //
  if (*CloseQuote == CHAR_NULL) {
    return (NULL);
  }

  return (FindEndOfParameter (CloseQuote + 1));
}

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
  )
{
  CONST CHAR16 *NextDelim;

  if (Walker           == NULL
    ||*Walker          == NULL
    ||TempParameter    == NULL
    ||*TempParameter   == NULL
    ) {
    return (EFI_INVALID_PARAMETER);
  }


  //
  // make sure we dont have any leading spaces
  //
  while ((*Walker)[0] == L' ') {
    (*Walker)++;
  }

  //
  // make sure we still have some params now...
  //
  if (StrLen(*Walker) == 0) {
DEBUG_CODE_BEGIN();
    *Walker        = NULL;
DEBUG_CODE_END();
    return (EFI_INVALID_PARAMETER);
  }

  NextDelim = FindEndOfParameter (*Walker);

  if (NextDelim == NULL){
DEBUG_CODE_BEGIN();
    *Walker        = NULL;
DEBUG_CODE_END();
    return (EFI_NOT_FOUND);
  }

  StrnCpyS (*TempParameter, Length / sizeof (CHAR16), (*Walker), NextDelim - *Walker);

  //
  // Add a CHAR_NULL if we didnt get one via the copy
  //
  if (*NextDelim != CHAR_NULL) {
    (*TempParameter)[NextDelim - *Walker] = CHAR_NULL;
  }

  //
  // Update Walker for the next iteration through the function
  //
  *Walker = (CHAR16 *)NextDelim;

  //
  // Remove any non-escaped quotes in the string
  // Remove any remaining escape characters in the string
  //
  for (NextDelim = FindFirstCharacter (*TempParameter, L"\"^", CHAR_NULL)
    ; *NextDelim != CHAR_NULL
    ; NextDelim = FindFirstCharacter (NextDelim, L"\"^", CHAR_NULL)
    ) {
    if (*NextDelim == L'^') {

      //
      // eliminate the escape ^
      //
      CopyMem ((CHAR16 *) NextDelim, NextDelim + 1, StrSize (NextDelim + 1));
      NextDelim++;
    } else if (*NextDelim == L'\"') {

      //
      // eliminate the unescaped quote
      //
      if (StripQuotation) {
        CopyMem ((CHAR16 *) NextDelim, NextDelim + 1, StrSize (NextDelim + 1));
      } else {
        NextDelim++;
      }
    }
  }

  return EFI_SUCCESS;
}

/**
  Function to populate Argc and Argv.

  This function parses the CommandLine and divides it into standard C style Argc/Argv
  parameters for inclusion in EFI_SHELL_PARAMETERS_PROTOCOL.  this supports space
  delimited and quote surrounded parameter definition.

  All special character processing (alias, environment variable, redirection,
  etc... must be complete before calling this API.

  @param[in] CommandLine          String of command line to parse
  @param[in] StripQuotation       if TRUE then strip the quotation marks surrounding
                                  the parameters.
  @param[in, out] Argv            pointer to array of strings; one for each parameter
  @param[in, out] Argc            pointer to number of strings in Argv array

  @return EFI_SUCCESS           the operation was sucessful
  @return EFI_OUT_OF_RESOURCES  a memory allocation failed.
**/
EFI_STATUS
ParseCommandLineToArgs (
  IN CONST CHAR16 *CommandLine,
  IN BOOLEAN      StripQuotation,
  IN OUT CHAR16   ***Argv,
  IN OUT UINTN    *Argc
  )
{
  UINTN       Count;
  CHAR16      *TempParameter;
  CHAR16      *Walker;
  CHAR16      *NewParam;
  CHAR16      *NewCommandLine;
  UINTN       Size;
  EFI_STATUS  Status;

  ASSERT (Argc != NULL);
  ASSERT (Argv != NULL);

  if (CommandLine == NULL || StrLen (CommandLine) == 0) {
    (*Argc) = 0;
    (*Argv) = NULL;
    return (EFI_SUCCESS);
  }

  NewCommandLine = AllocateCopyPool (StrSize (CommandLine), CommandLine);
  if (NewCommandLine == NULL) {
    return (EFI_OUT_OF_RESOURCES);
  }

  TrimSpaces (&NewCommandLine);
  Size = StrSize (NewCommandLine);
  TempParameter = AllocateZeroPool (Size);
  if (TempParameter == NULL) {
    SHELL_FREE_NON_NULL (NewCommandLine);
    return (EFI_OUT_OF_RESOURCES);
  }

  for ( Count = 0
      , Walker = (CHAR16 *) NewCommandLine
      ; Walker != NULL && *Walker != CHAR_NULL
      ; Count++
      ) {
    if (EFI_ERROR (GetNextParameter (&Walker, &TempParameter, Size, TRUE))) {
      break;
    }
  }

  //
  // lets allocate the pointer array
  //
  (*Argv) = AllocateZeroPool ((Count) * sizeof (CHAR16 *));
  if (*Argv == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  *Argc = 0;
  Walker = (CHAR16 *)NewCommandLine;
  while (Walker != NULL && *Walker != CHAR_NULL) {
    SetMem16 (TempParameter, Size, CHAR_NULL);
    if (EFI_ERROR (GetNextParameter (&Walker, &TempParameter, Size, StripQuotation))) {
      Status = EFI_INVALID_PARAMETER;
      goto Done;
    }

    NewParam = AllocateCopyPool (StrSize (TempParameter), TempParameter);
    if (NewParam == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto Done;
    }
    ((CHAR16 **) (*Argv))[ (*Argc)] = NewParam;
    (*Argc)++;
  }
  ASSERT (Count >= (*Argc));
  Status = EFI_SUCCESS;

Done:
  SHELL_FREE_NON_NULL (TempParameter);
  SHELL_FREE_NON_NULL (NewCommandLine);
  return (Status);
}

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
  )
{
  UINTN LoopCounter;
  ASSERT (ShellParameters != NULL);
  ASSERT (OldArgv         != NULL);
  ASSERT (OldArgc         != NULL);

  if (ShellParameters->Argv != NULL) {
    for ( LoopCounter = 0
        ; LoopCounter < ShellParameters->Argc
        ; LoopCounter++
       ) {
      FreePool(ShellParameters->Argv[LoopCounter]);
    }
    FreePool (ShellParameters->Argv);
  }
  ShellParameters->Argv = *OldArgv;
  *OldArgv = NULL;
  ShellParameters->Argc = *OldArgc;
  *OldArgc = 0;
}

/**
  Funcion will replace the current Argc and Argv in the ShellParameters protocol
  structure by parsing NewCommandLine.  The current values are returned to the
  user.

  If OldArgv or OldArgc is NULL then that value is not returned.

  @param[in, out] ShellParameters        Pointer to parameter structure to modify.
  @param[in] NewCommandLine              The new command line to parse and use.
  @param[in] Type                        The type of operation.
  @param[out] OldArgv                    Pointer to old list of parameters.
  @param[out] OldArgc                    Pointer to old number of items in Argv list.

  @retval   EFI_SUCCESS                 Operation was sucessful, Argv and Argc are valid.
  @retval   EFI_OUT_OF_RESOURCES        A memory allocation failed.
**/
EFI_STATUS
UpdateArgcArgv (
  IN OUT EFI_SHELL_PARAMETERS_PROTOCOL  *ShellParameters,
  IN CONST CHAR16                       *NewCommandLine,
  IN SHELL_OPERATION_TYPES              Type,
  OUT CHAR16                            ***OldArgv OPTIONAL,
  OUT UINTN                             *OldArgc OPTIONAL
  )
{
  BOOLEAN                 StripParamQuotation;

  ASSERT (ShellParameters != NULL);
  StripParamQuotation = TRUE;

  if (OldArgc != NULL) {
    *OldArgc = ShellParameters->Argc;
  }
  if (OldArgc != NULL) {
    *OldArgv = ShellParameters->Argv;
  }

  if (Type == Script_File_Name) {
    StripParamQuotation = FALSE;
  }

  return ParseCommandLineToArgs( NewCommandLine,
                                 StripParamQuotation,
                                 &(ShellParameters->Argv),
                                 &(ShellParameters->Argc)
                                );
}

/**
  Function to determin if an entire string is a valid number.

  If Hex it must be preceeded with a 0x or has ForceHex, set TRUE.

  @param[in] String       The string to evaluate.
  @param[in] ForceHex     TRUE - always assume hex.
  @param[in] StopAtSpace  TRUE to halt upon finding a space, FALSE to keep going.
  @param[in] TimeNumbers        TRUE to allow numbers with ":", FALSE otherwise.

  @retval TRUE        It is all numeric (dec/hex) characters.
  @retval FALSE       There is a non-numeric character.
**/
BOOLEAN
ConsoleInternalShellIsHexOrDecimalNumber (
  IN CONST CHAR16   *String,
  IN CONST BOOLEAN  ForceHex,
  IN CONST BOOLEAN  StopAtSpace,
  IN CONST BOOLEAN  TimeNumbers
  )
{
  BOOLEAN Hex;
  BOOLEAN LeadingZero;

  if (String == NULL) {
    return FALSE;
  }

  //
  // chop off a single negative sign
  //
  if (*String == L'-') {
    String++;
  }

  if (*String == CHAR_NULL) {
    return FALSE;
  }

  //
  // chop leading zeroes
  //
  LeadingZero = FALSE;
  while (*String == L'0') {
    String++;
    LeadingZero = TRUE;
  }
  //
  // allow '0x' or '0X', but not 'x' or 'X'
  //
  if (*String == L'x' || *String == L'X') {
    if (!LeadingZero) {
      //
      // we got an x without a preceeding 0
      //
      return (FALSE);
    }
    String++;
    Hex = TRUE;
  } else if (ForceHex) {
    Hex = TRUE;
  } else {
    Hex = FALSE;
  }

  //
  // loop through the remaining characters and use the lib function
  //
  for (; *String != CHAR_NULL && !(StopAtSpace && *String == L' ') ; String++) {
    if (TimeNumbers && (String[0] == L':')) {
      continue;
    }
    if (Hex) {
      if (!ConsoleIsHexaDecimalDigitCharacter (*String)) {
        return (FALSE);
      }
    } else {
      if (!ConsoleIsDecimalDigitCharacter (*String)) {
        return (FALSE);
      }
    }
  }

  return (TRUE);
}

typedef struct {
  LIST_ENTRY     Link;
  CHAR16         *Name;
  SHELL_PARAM_TYPE      Type;
  CHAR16         *Value;
  UINTN          OriginalPosition;
} SHELL_PARAM_PACKAGE;

/**
  Checks the list of valid arguments and returns TRUE if the item was found.  If the
  return value is TRUE then the type parameter is set also.

  if CheckList is NULL then ASSERT();
  if Name is NULL then ASSERT();
  if Type is NULL then ASSERT();

  @param Name                   pointer to Name of parameter found
  @param CheckList              List to check against
  @param Type                   pointer to type of parameter if it was found

  @retval TRUE                  the Parameter was found.  Type is valid.
  @retval FALSE                 the Parameter was not found.  Type is not valid.
**/
BOOLEAN
ConsoleInternalIsOnCheckList (
  IN CONST CHAR16               *Name,
  IN CONST SHELL_PARAM_ITEM     *CheckList,
  OUT SHELL_PARAM_TYPE          *Type
  )
{
  SHELL_PARAM_ITEM              *TempListItem;
  CHAR16                        *TempString;

  //
  // ASSERT that all 3 pointer parameters aren't NULL
  //
  ASSERT (CheckList  != NULL);
  ASSERT (Type       != NULL);
  ASSERT (Name       != NULL);

  //
  // question mark and page break mode are always supported
  //
  if ((StrCmp (Name, L"-?") == 0) ||
      (StrCmp (Name, L"-b") == 0)
     ) {
    *Type = TypeFlag;
    return (TRUE);
  }

  //
  // Enumerate through the list
  //
  for (TempListItem = (SHELL_PARAM_ITEM *)CheckList ; TempListItem->Name != NULL ; TempListItem++) {
    //
    // If the Type is TypeStart only check the first characters of the passed in param
    // If it matches set the type and return TRUE
    //
    if (TempListItem->Type == TypeStart) {
      if (StrnCmp (Name, TempListItem->Name, StrLen (TempListItem->Name)) == 0) {
        *Type = TempListItem->Type;
        return (TRUE);
      }
      TempString = NULL;
      TempString = ConsoleStrnCatGrow (&TempString, NULL, Name, StrLen (TempListItem->Name));
      if (TempString != NULL) {
        if (StringNoCaseCompare (&TempString, &TempListItem->Name) == 0) {
          *Type = TempListItem->Type;
          FreePool (TempString);
          return (TRUE);
        }
        FreePool (TempString);
      }
    } else if (StringNoCaseCompare (&Name, &TempListItem->Name) == 0) {
      *Type = TempListItem->Type;
      return (TRUE);
    }
  }

  return (FALSE);
}
/**
  Checks the string for indicators of "flag" status.  this is a leading '/', '-', or '+'

  @param[in] Name               pointer to Name of parameter found
  @param[in] AlwaysAllowNumbers TRUE to allow numbers, FALSE to not.
  @param[in] TimeNumbers        TRUE to allow numbers with ":", FALSE otherwise.

  @retval TRUE                  the Parameter is a flag.
  @retval FALSE                 the Parameter not a flag.
**/
BOOLEAN
ConsoleInternalIsFlag (
  IN CONST CHAR16               *Name,
  IN CONST BOOLEAN              AlwaysAllowNumbers,
  IN CONST BOOLEAN              TimeNumbers
  )
{
  //
  // ASSERT that Name isn't NULL
  //
  ASSERT(Name != NULL);

  //
  // If we accept numbers then dont return TRUE. (they will be values)
  //
  if (((Name[0] == L'-' || Name[0] == L'+') && ConsoleInternalShellIsHexOrDecimalNumber (Name+1, FALSE, FALSE, TimeNumbers)) && AlwaysAllowNumbers) {
    return (FALSE);
  }

  //
  // If the Name has a /, +, or - as the first character return TRUE
  //
  if ((Name[0] == L'/') ||
      (Name[0] == L'-') ||
      (Name[0] == L'+')
     ) {
    return (TRUE);
  }
  return (FALSE);
}

/**
  Checks the command line arguments passed against the list of valid ones.

  If no initialization is required, then return RETURN_SUCCESS.

  @param[in] CheckList          pointer to list of parameters to check
  @param[out] CheckPackage      pointer to pointer to list checked values
  @param[out] ProblemParam      optional pointer to pointer to unicode string for
                                the paramater that caused failure.  If used then the
                                caller is responsible for freeing the memory.
  @param[in] AutoPageBreak      will automatically set PageBreakEnabled for "b" parameter
  @param[in] Argv               pointer to array of parameters
  @param[in] Argc               Count of parameters in Argv
  @param[in] AlwaysAllowNumbers TRUE to allow numbers always, FALSE otherwise.

  @retval EFI_SUCCESS           The operation completed sucessfully.
  @retval EFI_OUT_OF_RESOURCES  A memory allocation failed
  @retval EFI_INVALID_PARAMETER A parameter was invalid
  @retval EFI_VOLUME_CORRUPTED  the command line was corrupt.  an argument was
                                duplicated.  the duplicated command line argument
                                was returned in ProblemParam if provided.
  @retval EFI_NOT_FOUND         a argument required a value that was missing.
                                the invalid command line argument was returned in
                                ProblemParam if provided.
**/
EFI_STATUS
ConsoleInternalCommandLineParse (
  IN CONST SHELL_PARAM_ITEM     *CheckList,
  OUT LIST_ENTRY                **CheckPackage,
  OUT CHAR16                    **ProblemParam OPTIONAL,
  IN BOOLEAN                    AutoPageBreak,
  IN CONST CHAR16               **Argv,
  IN UINTN                      Argc,
  IN BOOLEAN                    AlwaysAllowNumbers
  )
{
  UINTN                         LoopCounter;
  SHELL_PARAM_TYPE              CurrentItemType;
  SHELL_PARAM_PACKAGE           *CurrentItemPackage;
  UINTN                         GetItemValue;
  UINTN                         ValueSize;
  UINTN                         Count;
  CONST CHAR16                  *TempPointer;
  UINTN                         CurrentValueSize;
  CHAR16                        *NewValue;

  CurrentItemPackage = NULL;
  GetItemValue = 0;
  ValueSize = 0;
  Count = 0;

  //
  // If there is only 1 item we dont need to do anything
  //
  if (Argc < 1) {
    *CheckPackage = NULL;
    return (EFI_SUCCESS);
  }

  //
  // ASSERTs
  //
  ASSERT (CheckList  != NULL);
  ASSERT (Argv       != NULL);

  //
  // initialize the linked list
  //
  *CheckPackage = (LIST_ENTRY *)AllocateZeroPool (sizeof (LIST_ENTRY));
  if (*CheckPackage == NULL) {
    return (EFI_OUT_OF_RESOURCES);
  }

  InitializeListHead (*CheckPackage);

  //
  // loop through each of the arguments
  //
  for (LoopCounter = 0 ; LoopCounter < Argc ; ++LoopCounter) {
    if (Argv[LoopCounter] == NULL) {
      //
      // do nothing for NULL argv
      //
    } else if (ConsoleInternalIsOnCheckList (Argv[LoopCounter], CheckList, &CurrentItemType)) {
      //
      // We might have leftover if last parameter didnt have optional value
      //
      if (GetItemValue != 0) {
        GetItemValue = 0;
        InsertHeadList (*CheckPackage, &CurrentItemPackage->Link);
      }
      //
      // this is a flag
      //
      CurrentItemPackage = AllocateZeroPool (sizeof (SHELL_PARAM_PACKAGE));
      if (CurrentItemPackage == NULL) {
        ConsoleCommandLineFreeVarList (*CheckPackage);
        *CheckPackage = NULL;
        return (EFI_OUT_OF_RESOURCES);
      }
      CurrentItemPackage->Name  = AllocateCopyPool (StrSize (Argv[LoopCounter]), Argv[LoopCounter]);
      if (CurrentItemPackage->Name == NULL) {
        ConsoleCommandLineFreeVarList (*CheckPackage);
        *CheckPackage = NULL;
        return (EFI_OUT_OF_RESOURCES);
      }
      CurrentItemPackage->Type  = CurrentItemType;
      CurrentItemPackage->OriginalPosition = (UINTN) (-1);
      CurrentItemPackage->Value = NULL;

      //
      // Does this flag require a value
      //
      switch (CurrentItemPackage->Type) {
        //
        // possibly trigger the next loop(s) to populate the value of this item
        //
        case TypeValue:
        case TypeTimeValue:
          GetItemValue = 1;
          ValueSize = 0;
          break;
        case TypeDoubleValue:
          GetItemValue = 2;
          ValueSize = 0;
          break;
        case TypeMaxValue:
          GetItemValue = (UINTN)(-1);
          ValueSize = 0;
          break;
        default:
          //
          // this item has no value expected; we are done
          //
          InsertHeadList(*CheckPackage, &CurrentItemPackage->Link);
          ASSERT(GetItemValue == 0);
          break;
      }
    } else if (GetItemValue != 0 && CurrentItemPackage != NULL && !ConsoleInternalIsFlag(Argv[LoopCounter], AlwaysAllowNumbers, (BOOLEAN)(CurrentItemPackage->Type == TypeTimeValue))) {
      //
      // get the item VALUE for a previous flag
      //
      CurrentValueSize = ValueSize + StrSize(Argv[LoopCounter]) + sizeof(CHAR16);
      NewValue = ReallocatePool(ValueSize, CurrentValueSize, CurrentItemPackage->Value);
      if (NewValue == NULL) {
        SHELL_FREE_NON_NULL (CurrentItemPackage->Value);
        SHELL_FREE_NON_NULL (CurrentItemPackage);
        ConsoleCommandLineFreeVarList (*CheckPackage);
        *CheckPackage = NULL;
        return EFI_OUT_OF_RESOURCES;
      }
      CurrentItemPackage->Value = NewValue;
      if (ValueSize == 0) {
        StrCpyS( CurrentItemPackage->Value,
                  CurrentValueSize/sizeof(CHAR16),
                  Argv[LoopCounter]
                  );
      } else {
        StrCatS( CurrentItemPackage->Value,
                  CurrentValueSize/sizeof(CHAR16),
                  L" "
                  );
        StrCatS( CurrentItemPackage->Value,
                  CurrentValueSize/sizeof(CHAR16),
                  Argv[LoopCounter]
                  );
      }
      ValueSize += StrSize (Argv[LoopCounter]) + sizeof (CHAR16);

      GetItemValue--;
      if (GetItemValue == 0) {
        InsertHeadList (*CheckPackage, &CurrentItemPackage->Link);
      }
    } else if (!ConsoleInternalIsFlag (Argv[LoopCounter], AlwaysAllowNumbers, FALSE)) {
      //
      // add this one as a non-flag
      //

      TempPointer = Argv[LoopCounter];
      if ((*TempPointer == L'^' && *(TempPointer+1) == L'-')
       || (*TempPointer == L'^' && *(TempPointer+1) == L'/')
       || (*TempPointer == L'^' && *(TempPointer+1) == L'+')
      ) {
        TempPointer++;
      }
      CurrentItemPackage = AllocateZeroPool (sizeof (SHELL_PARAM_PACKAGE));
      if (CurrentItemPackage == NULL) {
        ConsoleCommandLineFreeVarList (*CheckPackage);
        *CheckPackage = NULL;
        return (EFI_OUT_OF_RESOURCES);
      }
      CurrentItemPackage->Name  = NULL;
      CurrentItemPackage->Type  = TypePosition;
      CurrentItemPackage->Value = AllocateCopyPool (StrSize (TempPointer), TempPointer);
      if (CurrentItemPackage->Value == NULL) {
        ConsoleCommandLineFreeVarList (*CheckPackage);
        *CheckPackage = NULL;
        return (EFI_OUT_OF_RESOURCES);
      }
      CurrentItemPackage->OriginalPosition = Count++;
      InsertHeadList (*CheckPackage, &CurrentItemPackage->Link);
    } else {
      //
      // this was a non-recognised flag... error!
      //
      if (ProblemParam != NULL) {
        *ProblemParam = AllocateCopyPool (StrSize (Argv[LoopCounter]), Argv[LoopCounter]);
      }
      ConsoleCommandLineFreeVarList (*CheckPackage);
      *CheckPackage = NULL;
      return (EFI_VOLUME_CORRUPTED);
    }
  }
  if (GetItemValue != 0) {
    GetItemValue = 0;
    InsertHeadList (*CheckPackage, &CurrentItemPackage->Link);
  }
  //
  // support for AutoPageBreak
  //
  /* if (AutoPageBreak && ConsoleCommandLineGetFlag(*CheckPackage, L"-b")) {
    ShellSetPageBreakMode(TRUE);
  } */
  return (EFI_SUCCESS);
}

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
  )
{
  //
  // ASSERT that CheckList and CheckPackage aren't NULL
  //
  ASSERT (CheckList    != NULL);
  ASSERT (CheckPackage != NULL);

  //
  // Check for UEFI Shell 2.0 protocols
  //
  if (NewShellParametersProtocol != NULL) {
    return (ConsoleInternalCommandLineParse (CheckList,
                                               CheckPackage,
                                               ProblemParam,
                                               AutoPageBreak,
                                               (CONST CHAR16**) NewShellParametersProtocol->Argv,
                                               NewShellParametersProtocol->Argc,
                                               FALSE));
  }

  return EFI_SUCCESS;
}

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
  )
{
  LIST_ENTRY                    *Node;

  //
  // check for CheckPackage == NULL
  //
  if (CheckPackage == NULL) {
    return;
  }

  //
  // for each node in the list
  //
  for (Node = GetFirstNode(CheckPackage)
      ; !IsListEmpty(CheckPackage)
      ; Node = GetFirstNode(CheckPackage)
     ) {
    //
    // Remove it from the list
    //
    RemoveEntryList (Node);

    //
    // if it has a name free the name
    //
    if (((SHELL_PARAM_PACKAGE *)Node)->Name != NULL) {
      FreePool (((SHELL_PARAM_PACKAGE *)Node)->Name);
    }

    //
    // if it has a value free the value
    //
    if (((SHELL_PARAM_PACKAGE *)Node)->Value != NULL) {
      FreePool (((SHELL_PARAM_PACKAGE *)Node)->Value);
    }

    //
    // free the node structure
    //
    FreePool ((SHELL_PARAM_PACKAGE *)Node);
  }
  //
  // free the list head node
  //
  FreePool (CheckPackage);
}
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
  )
{
  LIST_ENTRY                    *Node;
  CHAR16                        *TempString;

  //
  // return FALSE for no package or KeyString is NULL
  //
  if (CheckPackage == NULL || KeyString == NULL) {
    return (FALSE);
  }

  //
  // enumerate through the list of parametrs
  //
  for ( Node = GetFirstNode (CheckPackage)
      ; !IsNull (CheckPackage, Node)
      ; Node = GetNextNode (CheckPackage, Node)
      ){
    //
    // If the Name matches, return TRUE (and there may be NULL name)
    //
    if (((SHELL_PARAM_PACKAGE *)Node)->Name != NULL) {
      //
      // If Type is TypeStart then only compare the begining of the strings
      //
      if (((SHELL_PARAM_PACKAGE *)Node)->Type == TypeStart) {
        if (StrnCmp (KeyString, ((SHELL_PARAM_PACKAGE *)Node)->Name, StrLen (KeyString)) == 0) {
          return (TRUE);
        }
        TempString = NULL;
        TempString = ConsoleStrnCatGrow (&TempString, NULL, KeyString, StrLen (((SHELL_PARAM_PACKAGE *)Node)->Name));
        if (TempString != NULL) {
          if (StringNoCaseCompare (&KeyString, & ((SHELL_PARAM_PACKAGE *)Node)->Name) == 0) {
            FreePool (TempString);
            return (TRUE);
          }
          FreePool (TempString);
        }
      } else if (StringNoCaseCompare (&KeyString, & ((SHELL_PARAM_PACKAGE *)Node)->Name) == 0) {
        return (TRUE);
      }
    }
  }
  return (FALSE);
}
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
  )
{
  LIST_ENTRY                    *Node;
  CHAR16                        *TempString;

  //
  // return NULL for no package or KeyString is NULL
  //
  if (CheckPackage == NULL || KeyString == NULL) {
    return (NULL);
  }

  //
  // enumerate through the list of parametrs
  //
  for ( Node = GetFirstNode (CheckPackage)
      ; !IsNull (CheckPackage, Node)
      ; Node = GetNextNode (CheckPackage, Node)
      ) {
    //
    // If the Name matches, return TRUE (and there may be NULL name)
    //
    if (((SHELL_PARAM_PACKAGE*)Node)->Name != NULL) {
      //
      // If Type is TypeStart then only compare the begining of the strings
      //
      if (((SHELL_PARAM_PACKAGE*)Node)->Type == TypeStart) {
        if (StrnCmp(KeyString, ((SHELL_PARAM_PACKAGE*)Node)->Name, StrLen (KeyString)) == 0) {
          return (((SHELL_PARAM_PACKAGE*)Node)->Name + StrLen (KeyString));
        }
        TempString = NULL;
        TempString = ConsoleStrnCatGrow (&TempString, NULL, KeyString, StrLen (((SHELL_PARAM_PACKAGE*)Node)->Name));
        if (TempString != NULL) {
          if (StringNoCaseCompare (&KeyString, &((SHELL_PARAM_PACKAGE*)Node)->Name) == 0) {
            FreePool(TempString);
            return (((SHELL_PARAM_PACKAGE*)Node)->Name + StrLen (KeyString));
          }
          FreePool (TempString);
        }
      } else if (StringNoCaseCompare (&KeyString, &((SHELL_PARAM_PACKAGE*)Node)->Name) == 0) {
        return (((SHELL_PARAM_PACKAGE*)Node)->Value);
      }
    }
  }
  return (NULL);
}

/**
  Returns raw value from command line argument.

  Raw value parameters are in the form of "value" in a specific position in the list.

  If CheckPackage is NULL, then return NULL.

  @param[in] CheckPackage       The package of parsed command line arguments.
  @param[in] Position           The position of the value.

  @retval NULL                  The flag is not on the command line.
  @retval !=NULL                The pointer to unicode string of the value.
  **/
CONST CHAR16*
EFIAPI
ConsoleCommandLineGetRawValue (
  IN CONST LIST_ENTRY           *CONST CheckPackage,
  IN UINTN                      Position
  )
{
  LIST_ENTRY                    *Node;

  //
  // check for CheckPackage == NULL
  //
  if (CheckPackage == NULL) {
    return (NULL);
  }

  //
  // enumerate through the list of parametrs
  //
  for ( Node = GetFirstNode (CheckPackage)
      ; !IsNull (CheckPackage, Node)
      ; Node = GetNextNode (CheckPackage, Node)
     ){
    //
    // If the position matches, return the value
    //
    if (((SHELL_PARAM_PACKAGE *)Node)->OriginalPosition == Position) {
      return (((SHELL_PARAM_PACKAGE *)Node)->Value);
    }
  }
  return (NULL);
}

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
  )
{
  LIST_ENTRY  *Node1;
  UINTN       Count;

  if (CheckPackage == NULL) {
    return (0);
  }
  for (Node1 = GetFirstNode (CheckPackage), Count = 0
      ; !IsNull (CheckPackage, Node1)
      ; Node1 = GetNextNode (CheckPackage, Node1)
     ) {
    if (((SHELL_PARAM_PACKAGE *)Node1)->Name == NULL) {
      Count++;
    }
  }
  return (Count);
}

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
  )
{
  UINTN DestinationStartSize;
  UINTN NewSize;

  //
  // ASSERTs
  //
  ASSERT (Destination != NULL);

  //
  // If there's nothing to do then just return Destination
  //
  if (Source == NULL) {
    return (*Destination);
  }

  //
  // allow for un-initialized pointers, based on size being 0
  //
  if (CurrentSize != NULL && *CurrentSize == 0) {
    *Destination = NULL;
  }

  //
  // allow for NULL pointers address as Destination
  //
  if (*Destination != NULL) {
    ASSERT (CurrentSize != 0);
    DestinationStartSize = StrSize (*Destination);
    ASSERT (DestinationStartSize <= *CurrentSize);
  } else {
    DestinationStartSize = 0;
    //    ASSERT(*CurrentSize == 0);
  }

  //
  // Append all of Source?
  //
  if (Count == 0) {
    Count = StrLen (Source);
  }

  //
  // Test and grow if required
  //
  if (CurrentSize != NULL) {
    NewSize = *CurrentSize;
    if (NewSize < DestinationStartSize + (Count * sizeof (CHAR16))) {
      while (NewSize < (DestinationStartSize + (Count * sizeof (CHAR16)))) {
        NewSize += 2 * Count * sizeof (CHAR16);
      }
      *Destination = ReallocatePool (*CurrentSize, NewSize, *Destination);
      *CurrentSize = NewSize;
    }
  } else {
    NewSize = (Count + 1) * sizeof (CHAR16);
    *Destination = AllocateZeroPool (NewSize);
  }

  //
  // Now use standard StrnCat on a big enough buffer
  //
  if (*Destination == NULL) {
    return (NULL);
  }

  StrnCatS (*Destination, NewSize / sizeof (CHAR16), Source, Count);
  return *Destination;
}

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
  )
{
  return (BOOLEAN) ((Char >= L'0' && Char <= L'9') || (Char >= L'A' && Char <= L'F') || (Char >= L'a' && Char <= L'f'));
}

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
  )
{
  return (BOOLEAN) (Char >= L'0' && Char <= L'9');
}

/**
  Convert a Unicode character to upper case only if
  it maps to a valid small-case ASCII character.

  This internal function only deal with Unicode character
  which maps to a valid small-case ASCII character, i.e.
  L'a' to L'z'. For other Unicode character, the input character
  is returned directly.

  @param  Char  The character to convert.

  @retval LowerCharacter   If the Char is with range L'a' to L'z'.
  @retval Unchanged        Otherwise.

**/
CHAR16
InternalConsoleCharToUpper (
  IN      CHAR16                    Char
  )
{
  if (Char >= L'a' && Char <= L'z') {
    return (CHAR16) (Char - (L'a' - L'A'));
  }

  return Char;
}

/**
  Convert a Unicode character to numerical value.

  This internal function only deal with Unicode character
  which maps to a valid hexadecimal ASII character, i.e.
  L'0' to L'9', L'a' to L'f' or L'A' to L'F'. For other
  Unicode character, the value returned does not make sense.

  @param  Char  The character to convert.

  @return The numerical value converted.

**/
UINTN
InternalConsoleHexCharToUintn (
  IN      CHAR16                    Char
  )
{
  if (ConsoleIsDecimalDigitCharacter (Char)) {
    return Char - L'0';
  }

  return (10 + InternalConsoleCharToUpper (Char) - L'A');
}

/**
  Convert a Null-terminated Unicode hexadecimal string to a value of type UINT64.

  This function returns a value of type UINT64 by interpreting the contents
  of the Unicode string specified by String as a hexadecimal number.
  The format of the input Unicode string String is:

                  [spaces][zeros][x][hexadecimal digits].

  The valid hexadecimal digit character is in the range [0-9], [a-f] and [A-F].
  The prefix "0x" is optional. Both "x" and "X" is allowed in "0x" prefix.
  If "x" appears in the input string, it must be prefixed with at least one 0.
  The function will ignore the pad space, which includes spaces or tab characters,
  before [zeros], [x] or [hexadecimal digit]. The running zero before [x] or
  [hexadecimal digit] will be ignored. Then, the decoding starts after [x] or the
  first valid hexadecimal digit. Then, the function stops at the first character that is
  a not a valid hexadecimal character or NULL, whichever one comes first.

  If String has only pad spaces, then zero is returned.
  If String has no leading pad spaces, leading zeros or valid hexadecimal digits,
  then zero is returned.

  @param[in]  String      A pointer to a Null-terminated Unicode string.
  @param[out] Value       Upon a successful return the value of the conversion.
  @param[in] StopAtSpace  FALSE to skip spaces.

  @retval EFI_SUCCESS             The conversion was successful.
  @retval EFI_INVALID_PARAMETER   A parameter was NULL or invalid.
  @retval EFI_DEVICE_ERROR        An overflow occured.
**/
EFI_STATUS
InternalConsoleStrHexToUint64 (
  IN CONST CHAR16   *String,
     OUT   UINT64   *Value,
  IN CONST BOOLEAN  StopAtSpace
  )
{
  UINT64    Result;

  if (String == NULL || StrSize (String) == 0 || Value == NULL) {
    return (EFI_INVALID_PARAMETER);
  }

  //
  // Ignore the pad spaces (space or tab)
  //
  while ((*String == L' ') || (*String == L'\t')) {
    String++;
  }

  //
  // Ignore leading Zeros after the spaces
  //
  while (*String == L'0') {
    String++;
  }

  if (InternalConsoleCharToUpper (*String) == L'X') {
    if (*(String - 1) != L'0') {
      return 0;
    }
    //
    // Skip the 'X'
    //
    String++;
  }

  Result = 0;

  //
  // there is a space where there should't be
  //
  if (*String == L' ') {
    return (EFI_INVALID_PARAMETER);
  }

  while (ConsoleIsHexaDecimalDigitCharacter (*String)) {
    //
    // If the Hex Number represented by String overflows according
    // to the range defined by UINT64, then return EFI_DEVICE_ERROR.
    //
    if (!(Result <= (RShiftU64 ((((UINT64) ~0) - InternalConsoleHexCharToUintn (*String)), 4)))) {
//    if (!(Result <= ((((UINT64) ~0) - InternalConsoleHexCharToUintn (*String)) >> 4))) {
      return (EFI_DEVICE_ERROR);
    }

    Result = (LShiftU64 (Result, 4));
    Result += InternalConsoleHexCharToUintn (*String);
    String++;

    //
    // stop at spaces if requested
    //
    if (StopAtSpace && *String == L' ') {
      break;
    }
  }

  *Value = Result;
  return (EFI_SUCCESS);
}

/**
  Convert a Null-terminated Unicode decimal string to a value of
  type UINT64.

  This function returns a value of type UINT64 by interpreting the contents
  of the Unicode string specified by String as a decimal number. The format
  of the input Unicode string String is:

                  [spaces] [decimal digits].

  The valid decimal digit character is in the range [0-9]. The
  function will ignore the pad space, which includes spaces or
  tab characters, before [decimal digits]. The running zero in the
  beginning of [decimal digits] will be ignored. Then, the function
  stops at the first character that is a not a valid decimal character
  or a Null-terminator, whichever one comes first.

  If String has only pad spaces, then 0 is returned.
  If String has no pad spaces or valid decimal digits,
  then 0 is returned.

  @param[in]  String      A pointer to a Null-terminated Unicode string.
  @param[out] Value       Upon a successful return the value of the conversion.
  @param[in] StopAtSpace  FALSE to skip spaces.

  @retval EFI_SUCCESS             The conversion was successful.
  @retval EFI_INVALID_PARAMETER   A parameter was NULL or invalid.
  @retval EFI_DEVICE_ERROR        An overflow occured.
**/
EFI_STATUS
InternalConsoleStrDecimalToUint64 (
  IN CONST CHAR16 *String,
     OUT   UINT64 *Value,
  IN CONST BOOLEAN  StopAtSpace
  )
{
  UINT64     Result;

  if (String == NULL || StrSize (String) == 0 || Value == NULL) {
    return (EFI_INVALID_PARAMETER);
  }

  //
  // Ignore the pad spaces (space or tab)
  //
  while ((*String == L' ') || (*String == L'\t')) {
    String++;
  }

  //
  // Ignore leading Zeros after the spaces
  //
  while (*String == L'0') {
    String++;
  }

  Result = 0;

  //
  // Stop upon space if requested
  // (if the whole value was 0)
  //
  if (StopAtSpace && *String == L' ') {
    *Value = Result;
    return (EFI_SUCCESS);
  }

  while (ConsoleIsDecimalDigitCharacter (*String)) {
    //
    // If the number represented by String overflows according
    // to the range defined by UINT64, then return EFI_DEVICE_ERROR.
    //

    if (!(Result <= (DivU64x32 ((((UINT64) ~0) - (*String - L'0')),10)))) {
      return (EFI_DEVICE_ERROR);
    }

    Result = MultU64x32 (Result, 10) + (*String - L'0');
    String++;

    //
    // Stop at spaces if requested
    //
    if (StopAtSpace && *String == L' ') {
      break;
    }
  }

  *Value = Result;

  return (EFI_SUCCESS);
}

/**
  Function to determin if an entire string is a valid number.

  If Hex it must be preceeded with a 0x or has ForceHex, set TRUE.

  @param[in] String       The string to evaluate.
  @param[in] ForceHex     TRUE - always assume hex.
  @param[in] StopAtSpace  TRUE to halt upon finding a space, FALSE to keep going.
  @param[in] TimeNumbers        TRUE to allow numbers with ":", FALSE otherwise.

  @retval TRUE        It is all numeric (dec/hex) characters.
  @retval FALSE       There is a non-numeric character.
**/
BOOLEAN
InternalConsoleIsHexOrDecimalNumber (
  IN CONST CHAR16   *String,
  IN CONST BOOLEAN  ForceHex,
  IN CONST BOOLEAN  StopAtSpace,
  IN CONST BOOLEAN  TimeNumbers
  )
{
  BOOLEAN Hex;
  BOOLEAN LeadingZero;

  if (String == NULL) {
    return FALSE;
  }

  //
  // chop off a single negative sign
  //
  if (*String == L'-') {
    String++;
  }

  if (*String == CHAR_NULL) {
    return FALSE;
  }

  //
  // chop leading zeroes
  //
  LeadingZero = FALSE;
  while (*String == L'0'){
    String++;
    LeadingZero = TRUE;
  }
  //
  // allow '0x' or '0X', but not 'x' or 'X'
  //
  if (*String == L'x' || *String == L'X') {
    if (!LeadingZero) {
      //
      // we got an x without a preceeding 0
      //
      return (FALSE);
    }
    String++;
    Hex = TRUE;
  } else if (ForceHex) {
    Hex = TRUE;
  } else {
    Hex = FALSE;
  }

  //
  // loop through the remaining characters and use the lib function
  //
  for ( ; *String != CHAR_NULL && !(StopAtSpace && *String == L' ') ; String++) {
    if (TimeNumbers && (String[0] == L':')) {
      continue;
    }
    if (Hex) {
      if (!ConsoleIsHexaDecimalDigitCharacter (*String)) {
        return (FALSE);
      }
    } else {
      if (!ConsoleIsDecimalDigitCharacter (*String)) {
        return (FALSE);
      }
    }
  }

  return (TRUE);
}

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
  )
{
  UINT64        RetVal;
  CONST CHAR16  *Walker;
  EFI_STATUS    Status;
  BOOLEAN       Hex;

  Hex = ForceHex;

  if (!InternalConsoleIsHexOrDecimalNumber (String, Hex, StopAtSpace, FALSE)) {
    if (!Hex) {
      Hex = TRUE;
      if (!InternalConsoleIsHexOrDecimalNumber (String, Hex, StopAtSpace, FALSE)) {
        return (EFI_INVALID_PARAMETER);
      }
    } else {
      return (EFI_INVALID_PARAMETER);
    }
  }

  //
  // Chop off leading spaces
  //
  for (Walker = String; Walker != NULL && *Walker != CHAR_NULL && *Walker == L' '; Walker++);

  //
  // make sure we have something left that is numeric.
  //
  if (Walker == NULL || *Walker == CHAR_NULL || !InternalConsoleIsHexOrDecimalNumber (Walker, Hex, StopAtSpace, FALSE)) {
    return (EFI_INVALID_PARAMETER);
  }

  //
  // do the conversion.
  //
  if (Hex || StrnCmp(Walker, L"0x", 2) == 0 || StrnCmp (Walker, L"0X", 2) == 0){
    Status = InternalConsoleStrHexToUint64 (Walker, &RetVal, StopAtSpace);
  } else {
    Status = InternalConsoleStrDecimalToUint64 (Walker, &RetVal, StopAtSpace);
  }

  if (Value == NULL && !EFI_ERROR (Status)) {
    return (EFI_NOT_FOUND);
  }

  if (Value != NULL) {
    *Value = RetVal;
  }

  return (Status);
}

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
  )
{
  if (ConsoleConvertStringToUint64 (String, NULL, ForceHex, StopAtSpace) == EFI_NOT_FOUND) {
    return (TRUE);
  }
  return (FALSE);
}
