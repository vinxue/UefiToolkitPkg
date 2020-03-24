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

STATIC SHELL_COMMAND_INTERNAL_LIST_ENTRY  mCommandList;
STATIC CHAR16                             *mProfileList;
STATIC UINTN                              mProfileListSize;
STATIC UINT64                             mExitCode;
BOOLEAN                                   mExitRequested;

EFI_UNICODE_COLLATION_PROTOCOL           *gUnicodeCollation = NULL;

STATIC CONST CHAR8 Hex[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

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
  )
{
  UINT8           *Data;
  CHAR8           Val[50];
  CHAR8           Str[20];
  UINT8           TempByte;
  UINTN           Size;
  UINTN           Index;

  Data = UserData;
  while (DataSize != 0) {
    Size = 16;
    if (Size > DataSize) {
      Size = DataSize;
    }

    for (Index = 0; Index < Size; Index += 1) {
      TempByte            = Data[Index];
      Val[Index * 3 + 0]  = Hex[TempByte >> 4];
      Val[Index * 3 + 1]  = Hex[TempByte & 0xF];
      Val[Index * 3 + 2]  = (CHAR8) ((Index == 7) ? '-' : ' ');
      Str[Index]          = (CHAR8) ((TempByte < ' ' || TempByte > '~') ? '.' : TempByte);
    }

    Val[Index * 3]  = 0;
    Str[Index]      = 0;
    Print (L"%*a%08X: %-48a *%a*\r\n", Indent, "", Offset, Val, Str);

    Data += Size;
    Offset += Size;
    DataSize -= Size;
  }
}

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
  )
{
  SHELL_COMMAND_INTERNAL_LIST_ENTRY   *Node;

  //
  // assert for NULL parameters
  //
  ASSERT (CommandString != NULL);

  //
  // check for the command
  //
  for ( Node = (SHELL_COMMAND_INTERNAL_LIST_ENTRY *)GetFirstNode (&mCommandList.Link)
      ; !IsNull (&mCommandList.Link, &Node->Link)
      ; Node = (SHELL_COMMAND_INTERNAL_LIST_ENTRY *)GetNextNode (&mCommandList.Link, &Node->Link)
     ){
    ASSERT (Node->CommandString != NULL);
    if (gUnicodeCollation->StriColl (
          gUnicodeCollation,
          (CHAR16 *) CommandString,
          Node->CommandString) == 0
      ) {
      if (CanAffectLE != NULL) {
        *CanAffectLE = Node->LastError;
      }
      if (RetVal != NULL) {
        *RetVal = Node->CommandHandler (NULL, gST);
      } else {
        Node->CommandHandler (NULL, gST);
      }
      return (RETURN_SUCCESS);
    }
  }

  return (RETURN_NOT_FOUND);
}

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
  )
{
  mExitRequested = (BOOLEAN) (!mExitRequested);
  mExitCode = ErrorCode;
}

/**
  Retrieve the Exit indicator.

  @retval TRUE      Exit was indicated.
  @retval FALSE     Exis was not indicated.
**/
BOOLEAN
EFIAPI
ConsoleCommandGetExit (
  VOID
  )
{
  return (mExitRequested);
}

/**
  Retrieve the Exit code.

  If ConsoleCommandGetExit returns FALSE than the return from this is undefined.

  @return the value passed into RegisterExit.
**/
UINT64
EFIAPI
ShellCommandGetExitCode (
  VOID
  )
{
  return (mExitCode);
}

/**
  Function to make sure that the global protocol pointers are valid.
  must be called after constructor before accessing the pointers.
**/
EFI_STATUS
EFIAPI
CommandInit (
  VOID
  )
{
  UINTN                           NumHandles;
  EFI_HANDLE                      *Handles;
  EFI_UNICODE_COLLATION_PROTOCOL  *Uc;
  CHAR8                           *BestLanguage;
  UINTN                           Index;
  EFI_STATUS                      Status;
  CHAR8                           *PlatformLang;

  GetEfiGlobalVariable2 (EFI_PLATFORM_LANG_VARIABLE_NAME, (VOID **)&PlatformLang, NULL);
  if (PlatformLang == NULL) {
    return EFI_UNSUPPORTED;
  }

  if (gUnicodeCollation == NULL) {
    Status = gBS->LocateHandleBuffer (
                    ByProtocol,
                    &gEfiUnicodeCollation2ProtocolGuid,
                    NULL,
                    &NumHandles,
                    &Handles
                    );
    if (EFI_ERROR (Status)) {
      NumHandles = 0;
      Handles    = NULL;
    }
    for (Index = 0; Index < NumHandles; Index++) {
      //
      // Open Unicode Collation Protocol
      //
      Status = gBS->OpenProtocol (
                      Handles[Index],
                      &gEfiUnicodeCollation2ProtocolGuid,
                      (VOID **) &Uc,
                      gImageHandle,
                      NULL,
                      EFI_OPEN_PROTOCOL_GET_PROTOCOL
                      );
      if (EFI_ERROR (Status)) {
        continue;
      }

      //
      // Find the best matching matching language from the supported languages
      // of Unicode Collation2 protocol.
      //
      BestLanguage = GetBestLanguage (
                       Uc->SupportedLanguages,
                       FALSE,
                       PlatformLang,
                       NULL
                       );
      if (BestLanguage != NULL) {
        FreePool (BestLanguage);
        gUnicodeCollation = Uc;
        break;
      }
    }
    if (Handles != NULL) {
      FreePool (Handles);
    }
    FreePool (PlatformLang);
  }

  return (gUnicodeCollation == NULL) ? EFI_UNSUPPORTED : EFI_SUCCESS;
}

/**
  Checks if a command is already on the internal command list.

  @param[in] CommandString        The command string to check for on the list.
**/
BOOLEAN
ShellCommandIsCommandOnInternalList (
  IN CONST  CHAR16 *CommandString
  )
{
  SHELL_COMMAND_INTERNAL_LIST_ENTRY *Node;

  //
  // assert for NULL parameter
  //
  ASSERT (CommandString != NULL);

  //
  // check for the command
  //
  for ( Node = (SHELL_COMMAND_INTERNAL_LIST_ENTRY *) GetFirstNode (&mCommandList.Link)
      ; !IsNull (&mCommandList.Link, &Node->Link)
      ; Node = (SHELL_COMMAND_INTERNAL_LIST_ENTRY *) GetNextNode (&mCommandList.Link, &Node->Link)
     ) {
    ASSERT(Node->CommandString != NULL);
    if (gUnicodeCollation->StriColl (
          gUnicodeCollation,
          (CHAR16 *) CommandString,
          Node->CommandString) == 0
       ) {
      return (TRUE);
    }
  }
  return (FALSE);
}

/**
  Checks if a command exists, either internally or through the dynamic command protocol.

  @param[in] CommandString        The command string to check for on the list.
**/
BOOLEAN
EFIAPI
ShellCommandIsCommandOnList (
  IN CONST  CHAR16                      *CommandString
  )
{
  if (ShellCommandIsCommandOnInternalList (CommandString)) {
    return TRUE;
  }

  return FALSE;
}

/**
  Registers handlers of type SHELL_RUN_COMMAND and
  SHELL_GET_MAN_FILENAME for each shell command.

  If the ShellSupportLevel is greater than the value of the
  PcdShellSupportLevel then return RETURN_UNSUPPORTED.

  Registers the handlers specified by GetHelpInfoHandler and CommandHandler
  with the command specified by CommandString. If the command named by
  CommandString has already been registered, then return
  RETURN_ALREADY_STARTED.

  If there are not enough resources available to register the handlers then
  RETURN_OUT_OF_RESOURCES is returned.

  If CommandString is NULL, then ASSERT().
  If GetHelpInfoHandler is NULL, then ASSERT().
  If CommandHandler is NULL, then ASSERT().
  If ProfileName is NULL, then ASSERT().

  @param[in]  CommandString         Pointer to the command name.  This is the
                                    name to look for on the command line in
                                    the shell.
  @param[in]  CommandHandler        Pointer to a function that runs the
                                    specified command.
  @param[in]  GetManFileName        Pointer to a function that provides man
                                    filename.
  @param[in]  ShellMinSupportLevel  minimum Shell Support Level which has this
                                    function.
  @param[in]  ProfileName           profile name to require for support of this
                                    function.
  @param[in]  CanAffectLE           indicates whether this command's return value
                                    can change the LASTERROR environment variable.
  @param[in]  HiiHandle             Handle of this command's HII entry.
  @param[in]  ManFormatHelp         HII locator for the help text.

  @retval  RETURN_SUCCESS           The handlers were registered.
  @retval  RETURN_OUT_OF_RESOURCES  There are not enough resources available to
                                    register the shell command.
  @retval RETURN_UNSUPPORTED        the ShellMinSupportLevel was higher than the
                                    currently allowed support level.
  @retval RETURN_ALREADY_STARTED    The CommandString represents a command that
                                    is already registered.  Only 1 handler set for
                                    a given command is allowed.
  @sa SHELL_GET_MAN_FILENAME
  @sa SHELL_RUN_COMMAND
**/
RETURN_STATUS
EFIAPI
ConsoleCommandRegisterCommandName (
  IN CONST  CHAR16                      *CommandString,
  IN        SHELL_RUN_COMMAND           CommandHandler,
  IN        SHELL_GET_MAN_FILENAME      GetManFileName,
  IN        UINT32                      ShellMinSupportLevel,
  IN CONST  CHAR16                      *ProfileName,
  IN CONST  BOOLEAN                     CanAffectLE,
  IN CONST  EFI_HANDLE                  HiiHandle,
  IN CONST  EFI_STRING_ID               ManFormatHelp
  )
{
  SHELL_COMMAND_INTERNAL_LIST_ENTRY *Node;
  SHELL_COMMAND_INTERNAL_LIST_ENTRY *Command;
  SHELL_COMMAND_INTERNAL_LIST_ENTRY *PrevCommand;
  INTN LexicalMatchValue;

  //
  // Initialize local variables.
  //
  Command = NULL;
  PrevCommand = NULL;
  LexicalMatchValue = 0;

  //
  // ASSERTs for NULL parameters
  //
  ASSERT (CommandString  != NULL);
  ASSERT (GetManFileName != NULL);
  ASSERT (CommandHandler != NULL);
  ASSERT (ProfileName    != NULL);

  //
  // check for already on the list
  //
  if (ShellCommandIsCommandOnList (CommandString)) {
    return (RETURN_ALREADY_STARTED);
  }

  //
  // allocate memory for new struct
  //
  Node = AllocateZeroPool (sizeof (SHELL_COMMAND_INTERNAL_LIST_ENTRY));
  if (Node == NULL) {
    return RETURN_OUT_OF_RESOURCES;
  }
  Node->CommandString = AllocateCopyPool (StrSize (CommandString), CommandString);
  if (Node->CommandString == NULL) {
    FreePool (Node);
    return RETURN_OUT_OF_RESOURCES;
  }

  Node->GetManFileName  = GetManFileName;
  Node->CommandHandler  = CommandHandler;
  Node->LastError       = CanAffectLE;
  Node->HiiHandle       = HiiHandle;
  Node->ManFormatHelp   = ManFormatHelp;

  if (StrLen (ProfileName) > 0
    && ((mProfileList != NULL
    && StrStr(mProfileList, ProfileName) == NULL) || mProfileList == NULL)
   ) {
    ASSERT ((mProfileList == NULL && mProfileListSize == 0) || (mProfileList != NULL));
    if (mProfileList == NULL) {
      //
      // If this is the first make a leading ';'
      //
      ConsoleStrnCatGrow (&mProfileList, &mProfileListSize, L";", 0);
    }
    ConsoleStrnCatGrow (&mProfileList, &mProfileListSize, ProfileName, 0);
    ConsoleStrnCatGrow (&mProfileList, &mProfileListSize, L";", 0);
  }

  //
  // Insert a new entry on top of the list
  //
  InsertHeadList (&mCommandList.Link, &Node->Link);

  //
  // Move a new registered command to its sorted ordered location in the list
  //
  for (Command = (SHELL_COMMAND_INTERNAL_LIST_ENTRY *) GetFirstNode (&mCommandList.Link),
        PrevCommand = (SHELL_COMMAND_INTERNAL_LIST_ENTRY *) GetFirstNode (&mCommandList.Link)
        ; !IsNull (&mCommandList.Link, &Command->Link)
        ; Command = (SHELL_COMMAND_INTERNAL_LIST_ENTRY *) GetNextNode (&mCommandList.Link, &Command->Link)) {

    //
    // Get Lexical Comparison Value between PrevCommand and Command list entry
    //
    LexicalMatchValue = gUnicodeCollation->StriColl (
                                             gUnicodeCollation,
                                             PrevCommand->CommandString,
                                             Command->CommandString
                                             );

    //
    // Swap PrevCommand and Command list entry if PrevCommand list entry
    // is alphabetically greater than Command list entry
    //
    if (LexicalMatchValue > 0) {
      Command = (SHELL_COMMAND_INTERNAL_LIST_ENTRY *) SwapListEntries (&PrevCommand->Link, &Command->Link);
    } else if (LexicalMatchValue < 0) {
      //
      // PrevCommand entry is lexically lower than Command entry
      //
      break;
    }
  }

  return (RETURN_SUCCESS);
}

STATIC CONST CHAR16 mFileName[] = L"DebugCommands";

/**
  Gets the debug file name.  This will be used if HII is not working.

  @retval NULL    No file is available.
  @return         The NULL-terminated filename to get help from.
**/
CONST CHAR16 *
EFIAPI
ShellCommandGetManFileNameDebug (
  VOID
  )
{
  return (mFileName);
}

/**
  Destructor for the function, free any resources.

  @param VOID

  @retval EFI_SUCCESS    This function always returns success

**/
EFI_STATUS
EFIAPI
ConsoleCommandsDestructor (
  VOID
  )
{
  SHELL_COMMAND_INTERNAL_LIST_ENTRY *Node;

  //
  // Enumerate throught the list and free all the memory
  //
  while (!IsListEmpty (&mCommandList.Link)) {
    Node = (SHELL_COMMAND_INTERNAL_LIST_ENTRY *) GetFirstNode (&mCommandList.Link);
    RemoveEntryList (&Node->Link);
    SHELL_FREE_NON_NULL (Node->CommandString);
    FreePool (Node);
    DEBUG_CODE (Node = NULL;);
  }

  if (mProfileList != NULL) {
    FreePool (mProfileList);
  }

  gUnicodeCollation            = NULL;

  return EFI_SUCCESS;
}

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
  IN EFI_HANDLE            ImageHandle,
  IN EFI_SYSTEM_TABLE      *SystemTable
  )
{
  EFI_STATUS        Status;

  InitializeListHead (&mCommandList.Link);

  mExitRequested    = FALSE;
  mProfileListSize  = 0;
  mProfileList      = NULL;

  Status = CommandInit ();
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  //
  // Install our console command handlers that are always installed
  //
  ConsoleCommandRegisterCommandName (L"mem",    ShellCommandRunMem     , ShellCommandGetManFileNameDebug, 0, L"Debug", TRUE, NULL, 0 );
  ConsoleCommandRegisterCommandName (L"exit",   ShellCommandRunExit    , ShellCommandGetManFileNameDebug, 0, L"Debug", TRUE, NULL, 0 );
  ConsoleCommandRegisterCommandName (L"reset",  ShellCommandRunReset   , ShellCommandGetManFileNameDebug, 0, L"Debug", TRUE, NULL, 0 );

  return EFI_SUCCESS;
}
