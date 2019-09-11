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

#define CONSOLE_KEY          L'r'

typedef struct {
  LIST_ENTRY    Link;
  VOID          *Buffer;
} BUFFER_LIST;

typedef struct {
  BUFFER_LIST                 CommandHistory;
  UINTN                       VisibleRowNumber;
  UINTN                       OriginalVisibleRowNumber;
  BOOLEAN                     InsertMode;           ///< Is the current typing mode insert (FALSE = overwrite).
} SHELL_VIEWING_SETTINGS;

SHELL_VIEWING_SETTINGS        ViewingSettings;
EFI_SHELL_PARAMETERS_PROTOCOL *NewShellParametersProtocol;

extern BOOLEAN                mExitRequested;

STATIC EFI_EVENT              mTimerEvent    = NULL;
STATIC VOID                   *NotifyHandle0 = NULL;
STATIC VOID                   *NotifyHandle1 = NULL;
STATIC VOID                   *NotifyHandle2 = NULL;
STATIC VOID                   *NotifyHandle3 = NULL;

//
// 1 seconds in 100ns intervals = 3 * ms in 1 second * us in 1 ms * 100ns in 1us
//
#define CONSOLE_REQUEST_DELAY  (1 * 1000 * 1000 * 10)

/**
  Add a buffer to the Line History List

  @param Buffer     The line buffer to add.

**/
VOID
AddLineToCommandHistory (
  IN CONST CHAR16 *Buffer
  )
{
  BUFFER_LIST *Node;
  BUFFER_LIST *Walker;
  UINT16       MaxHistoryCmdCount;
  UINT16       Count;

  Count = 0;
  MaxHistoryCmdCount = 0x20;

  if (MaxHistoryCmdCount == 0) {
    return ;
  }


  Node = AllocateZeroPool (sizeof (BUFFER_LIST));
  if (Node == NULL) {
    return;
  }

  Node->Buffer = AllocateCopyPool (StrSize (Buffer), Buffer);
  if (Node->Buffer == NULL) {
    FreePool (Node);
    return;
  }

  for (Walker = (BUFFER_LIST*) GetFirstNode (&ViewingSettings.CommandHistory.Link)
      ; !IsNull(&ViewingSettings.CommandHistory.Link, &Walker->Link)
      ; Walker = (BUFFER_LIST*) GetNextNode (&ViewingSettings.CommandHistory.Link, &Walker->Link)
   ) {
    Count++;
  }
  if (Count < MaxHistoryCmdCount) {
    InsertTailList (&ViewingSettings.CommandHistory.Link, &Node->Link);
  } else {
    Walker = (BUFFER_LIST *) GetFirstNode (&ViewingSettings.CommandHistory.Link);
    RemoveEntryList (&Walker->Link);
    if (Walker->Buffer != NULL) {
      FreePool (Walker->Buffer);
    }
    FreePool (Walker);
    InsertTailList (&ViewingSettings.CommandHistory.Link, &Node->Link);
  }
}

/**
  Move the cursor position one character backward.

  @param[in] LineLength       Length of a line. Get it by calling QueryMode
  @param[in, out] Column      Current column of the cursor position
  @param[in, out] Row         Current row of the cursor position
**/
VOID
MoveCursorBackward (
  IN     UINTN                   LineLength,
  IN OUT UINTN                   *Column,
  IN OUT UINTN                   *Row
  )
{
  //
  // If current column is 0, move to the last column of the previous line,
  // otherwise, just decrement column.
  //
  if (*Column == 0) {
    *Column = LineLength - 1;
    if (*Row > 0) {
      (*Row)--;
    }
    return;
  }
  (*Column)--;
}

/**
  Move the cursor position one character forward.

  @param[in] LineLength       Length of a line.
  @param[in] TotalRow         Total row of a screen
  @param[in, out] Column      Current column of the cursor position
  @param[in, out] Row         Current row of the cursor position
**/
VOID
MoveCursorForward (
  IN     UINTN                   LineLength,
  IN     UINTN                   TotalRow,
  IN OUT UINTN                   *Column,
  IN OUT UINTN                   *Row
  )
{
  //
  // Increment Column.
  // If this puts column past the end of the line, move to first column
  // of the next row.
  //
  (*Column)++;
  if (*Column >= LineLength) {
    (*Column) = 0;
    if ((*Row) < TotalRow - 1) {
      (*Row)++;
    }
  }
}

EFI_STATUS
EFIAPI
UefiConsoleInRead (
  IN OUT UINTN        *BufferSize,
  OUT VOID            *Buffer
  )
{
  CHAR16              *CurrentString;
  BOOLEAN             Done;
  UINTN               TabUpdatePos;   // Start index of the string updated by TAB stroke
  UINTN               Column;         // Column of current cursor
  UINTN               Row;            // Row of current cursor
  UINTN               StartColumn;    // Column at the beginning of the line
  UINTN               Update;         // Line index for update
  UINTN               Delete;         // Num of chars to delete from console after update
  UINTN               StringLen;      // Total length of the line
  UINTN               StringCurPos;   // Line index corresponding to the cursor
  UINTN               MaxStr;         // Maximum possible line length
  UINTN               TotalColumn;     // Num of columns in the console
  UINTN               TotalRow;       // Num of rows in the console
  UINTN               SkipLength;
  UINTN               OutputLength;   // Length of the update string
  UINTN               TailRow;        // Row of end of line
  UINTN               TailColumn;     // Column of end of line
  EFI_INPUT_KEY       Key;

  BUFFER_LIST         *LinePos;
  BUFFER_LIST         *NewPos;
  BOOLEAN             InScrolling;
  EFI_STATUS          Status;
  EFI_SHELL_FILE_INFO *TabCompleteList;
  EFI_SHELL_FILE_INFO *TabCurrent;
  UINTN               EventIndex;
  CHAR16              *TabOutputStr;

  //
  // If buffer is not large enough to hold a CHAR16, return minimum buffer size
  //
  if (*BufferSize < sizeof (CHAR16) * 2) {
    *BufferSize = sizeof (CHAR16) * 2;
    return (EFI_BUFFER_TOO_SMALL);
  }

  Done              = FALSE;
  CurrentString     = Buffer;
  StringLen         = 0;
  StringCurPos      = 0;
  OutputLength      = 0;
  Update            = 0;
  Delete            = 0;
  LinePos           = NewPos = (BUFFER_LIST *) (&ViewingSettings.CommandHistory);
  InScrolling       = FALSE;
  Status            = EFI_SUCCESS;
  TabOutputStr      = NULL;
  TabUpdatePos      = 0;
  TabCompleteList   = NULL;
  TabCurrent        = NULL;

  //
  // Get the screen setting and the current cursor location
  //
  Column      = StartColumn = gST->ConOut->Mode->CursorColumn;
  Row         = gST->ConOut->Mode->CursorRow;
  gST->ConOut->QueryMode (gST->ConOut, gST->ConOut->Mode->Mode, &TotalColumn, &TotalRow);

  //
  // Limit the line length to the buffer size or the minimun size of the
  // screen. (The smaller takes effect)
  //
  MaxStr = TotalColumn * (TotalRow - 1) - StartColumn;
  if (MaxStr > *BufferSize / sizeof (CHAR16)) {
    MaxStr = *BufferSize / sizeof (CHAR16);
  }
  ZeroMem (CurrentString, MaxStr * sizeof (CHAR16));
  do {
    //
    // Read a key
    //
    gBS->WaitForEvent (1, &gST->ConIn->WaitForKey, &EventIndex);
    Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);
    if (EFI_ERROR (Status)) {

      if (Status == EFI_NOT_READY) {
        continue;
      }

      ZeroMem (CurrentString, MaxStr * sizeof (CHAR16));
      StringLen = 0;
      break;
    }

    switch (Key.UnicodeChar) {
    case CHAR_CARRIAGE_RETURN:
      //
      // All done, print a newline at the end of the string
      //
      TailRow     = Row + (StringLen - StringCurPos + Column) / TotalColumn;
      TailColumn  = (StringLen - StringCurPos + Column) % TotalColumn;
      if (Column != -1 && Row != -1) {
        Status = gST->ConOut->SetCursorPosition (gST->ConOut, TailColumn, TailRow);
        Print (L"\n");
      }
      Done = TRUE;
      break;

    case CHAR_BACKSPACE:
      if (StringCurPos != 0) {
        //
        // If not move back beyond string beginning, move all characters behind
        // the current position one character forward
        //
        StringCurPos--;
        Update  = StringCurPos;
        Delete  = 1;
        CopyMem (CurrentString + StringCurPos, CurrentString + StringCurPos + 1, sizeof (CHAR16) * (StringLen - StringCurPos));

        //
        // Adjust the current column and row
        //
        MoveCursorBackward (TotalColumn, &Column, &Row);
      }
      break;

    default:
      if (Key.UnicodeChar >= ' ') {
        //
        // If we are at the buffer's end, drop the key
        //
        if (StringLen == MaxStr - 1 && (StringCurPos == StringLen)) {
          break;
        }

        CurrentString[StringCurPos] = Key.UnicodeChar;
        Update      = StringCurPos;

        StringCurPos += 1;
        OutputLength = 1;
      }
      break;

    case 0:
      switch (Key.ScanCode) {
      case SCAN_DELETE:
        //
        // Move characters behind current position one character forward
        //
        if (StringLen != 0) {
          Update  = StringCurPos;
          Delete  = 1;
          CopyMem (CurrentString + StringCurPos, CurrentString + StringCurPos + 1, sizeof (CHAR16) * (StringLen - StringCurPos));
        }
        break;

      case SCAN_UP:
        //
        // Prepare to print the previous command
        //
        NewPos = (BUFFER_LIST *)GetPreviousNode (&ViewingSettings.CommandHistory.Link, &LinePos->Link);
        if (IsNull (&ViewingSettings.CommandHistory.Link, &LinePos->Link)) {
          NewPos = (BUFFER_LIST *) GetPreviousNode (&ViewingSettings.CommandHistory.Link, &LinePos->Link);
        }
        break;

      case SCAN_DOWN:
        //
        // Prepare to print the next command
        //
        NewPos = (BUFFER_LIST *) GetNextNode (&ViewingSettings.CommandHistory.Link, &LinePos->Link);
        if (NewPos == (BUFFER_LIST *) (&ViewingSettings.CommandHistory)) {
          NewPos = (BUFFER_LIST *) GetNextNode (&ViewingSettings.CommandHistory.Link, &LinePos->Link);
        }
        break;

      case SCAN_LEFT:
        //
        // Adjust current cursor position
        //
        if (StringCurPos != 0) {
          --StringCurPos;
          MoveCursorBackward (TotalColumn, &Column, &Row);
        }
        break;

      case SCAN_RIGHT:
        //
        // Adjust current cursor position
        //
        if (StringCurPos < StringLen) {
          ++StringCurPos;
          MoveCursorForward (TotalColumn, TotalRow, &Column, &Row);
        }
        break;

      case SCAN_HOME:
        //
        // Move current cursor position to the beginning of the command line
        //
        Row -= (StringCurPos + StartColumn) / TotalColumn;
        Column  = StartColumn;
        StringCurPos  = 0;
        break;

      case SCAN_END:
        //
        // Move current cursor position to the end of the command line
        //
        TailRow       = Row + (StringLen - StringCurPos + Column) / TotalColumn;
        TailColumn    = (StringLen - StringCurPos + Column) % TotalColumn;
        Row           = TailRow;
        Column        = TailColumn;
        StringCurPos  = StringLen;
        break;

      case SCAN_ESC:
        //
        // Prepare to clear the current command line
        //
        CurrentString[0]  = 0;
        Update  = 0;
        Delete  = StringLen;
        Row -= (StringCurPos + StartColumn) / TotalColumn;
        Column        = StartColumn;
        OutputLength  = 0;
        break;
      }
    }

    if (Done) {
      break;
    }

    //
    // If we have a new position, we are preparing to print a previous or
    // next command.
    //
    if (NewPos != (BUFFER_LIST *) (&ViewingSettings.CommandHistory)) {
      Column = StartColumn;
      Row -= (StringCurPos + StartColumn) / TotalColumn;

      LinePos       = NewPos;
      NewPos        = (BUFFER_LIST *) (&ViewingSettings.CommandHistory);

      OutputLength  = StrLen (LinePos->Buffer) < MaxStr - 1 ? StrLen (LinePos->Buffer) : MaxStr - 1;
      CopyMem (CurrentString, LinePos->Buffer, OutputLength * sizeof (CHAR16));
      CurrentString[OutputLength] = CHAR_NULL;

      StringCurPos            = OutputLength;

      //
      // Draw new input string
      //
      Update = 0;
      if (StringLen > OutputLength) {
        //
        // If old string was longer, blank its tail
        //
        Delete = StringLen - OutputLength;
      }
    }
    //
    // If we need to update the output do so now
    //
    if (Update != (UINTN) - 1) {
      if (Column != -1 && Row != -1) {
        Status = gST->ConOut->SetCursorPosition (gST->ConOut, Column, Row);
        Print (L"%s%.*s", CurrentString + Update, Delete);
      }
      StringLen = StrLen (CurrentString);

      if (Delete != 0) {
        SetMem (CurrentString + StringLen, Delete * sizeof (CHAR16), CHAR_NULL);
      }

      if (StringCurPos > StringLen) {
        StringCurPos = StringLen;
      }

      Update = (UINTN) - 1;

      //
      // After using print to reflect newly updates, if we're not using
      // BACKSPACE and DELETE, we need to move the cursor position forward,
      // so adjust row and column here.
      //
      if (Key.UnicodeChar != CHAR_BACKSPACE && ! (Key.UnicodeChar == 0 && Key.ScanCode == SCAN_DELETE)) {
        //
        // Calulate row and column of the tail of current string
        //
        TailRow     = Row + (StringLen - StringCurPos + Column + OutputLength) / TotalColumn;
        TailColumn  = (StringLen - StringCurPos + Column + OutputLength) % TotalColumn;

        //
        // If the tail of string reaches screen end, screen rolls up, so if
        // Row does not equal TailRow, Row should be decremented
        //
        // (if we are recalling commands using UPPER and DOWN key, and if the
        // old command is too long to fit the screen, TailColumn must be 79.
        //
        if (TailColumn == 0 && TailRow >= TotalRow && Row != TailRow) {
          Row--;
        }
        //
        // Calculate the cursor position after current operation. If cursor
        // reaches line end, update both row and column, otherwise, only
        // column will be changed.
        //
        if (Column + OutputLength >= TotalColumn) {
          SkipLength = OutputLength - (TotalColumn - Column);

          Row += SkipLength / TotalColumn + 1;
          if (Row > TotalRow - 1) {
            Row = TotalRow - 1;
          }

          Column = SkipLength % TotalColumn;
        } else {
          Column += OutputLength;
        }
      }

      Delete = 0;
    }
    //
    // Set the cursor position for this key
    //
    gST->ConOut->SetCursorPosition (gST->ConOut, Column, Row);
  } while (!Done);

  if (CurrentString != NULL && StrLen (CurrentString) > 0) {
    //
    // add the line to the history buffer
    //
    AddLineToCommandHistory (CurrentString);
  }

  //
  // Return the data to the caller
  //
  *BufferSize = StringLen * sizeof (CHAR16);

  return Status;
}

/**
  Cleans off leading and trailing spaces and tabs.

  @param[in] String pointer to the string to trim them off.
**/
EFI_STATUS
TrimSpaces (
  IN CHAR16 **String
  )
{
  ASSERT (String  != NULL);
  ASSERT (*String != NULL);
  //
  // Remove any spaces and tabs at the beginning of the (*String).
  //
  while (((*String)[0] == L' ') || ((*String)[0] == L'\t')) {
    CopyMem ((*String), (*String) + 1, StrSize ((*String)) - sizeof ((*String)[0]));
  }

  //
  // Remove any spaces and tabs at the end of the (*String).
  //
  while ((StrLen (*String) > 0) && (((*String)[StrLen ((*String)) -1 ] == L' ') || ((*String)[StrLen ((*String)) -1 ] == L'\t'))) {
    (*String)[StrLen ((*String)) - 1] = CHAR_NULL;
  }

  return (EFI_SUCCESS);
}

/**
  Parse for the next instance of one string within another string. Can optionally make sure that
  the string was not escaped (^ character) per the shell specification.

  @param[in] SourceString             The string to search within
  @param[in] FindString               The string to look for
  @param[in] CheckForEscapeCharacter  TRUE to skip escaped instances of FinfString, otherwise will return even escaped instances
**/
CHAR16 *
FindNextInstance (
  IN CONST CHAR16   *SourceString,
  IN CONST CHAR16   *FindString,
  IN CONST BOOLEAN  CheckForEscapeCharacter
  )
{
  CHAR16 *Temp;
  if (SourceString == NULL) {
    return (NULL);
  }
  Temp = StrStr (SourceString, FindString);

  //
  // If nothing found, or we don't care about escape characters
  //
  if (Temp == NULL || !CheckForEscapeCharacter) {
    return (Temp);
  }

  //
  // If we found an escaped character, try again on the remainder of the string
  //
  if ((Temp > (SourceString)) && * (Temp - 1) == L'^') {
    return FindNextInstance (Temp + 1, FindString, CheckForEscapeCharacter);
  }

  //
  // we found the right character
  //
  return (Temp);
}

/**
  Return the pointer to the first occurrence of any character from a list of characters.

  @param[in] String           the string to parse
  @param[in] CharacterList    the list of character to look for
  @param[in] EscapeCharacter  An escape character to skip

  @return the location of the first character in the string
  @retval CHAR_NULL no instance of any character in CharacterList was found in String
**/
CONST CHAR16 *
FindFirstCharacter (
  IN CONST CHAR16 *String,
  IN CONST CHAR16 *CharacterList,
  IN CONST CHAR16 EscapeCharacter
  )
{
  UINT32 WalkChar;
  UINT32 WalkStr;

  for (WalkStr = 0; WalkStr < StrLen (String); WalkStr++) {
    if (String[WalkStr] == EscapeCharacter) {
      WalkStr++;
      continue;
    }
    for (WalkChar = 0; WalkChar < StrLen (CharacterList); WalkChar++) {
      if (String[WalkStr] == CharacterList[WalkChar]) {
        return (&String[WalkStr]);
      }
    }
  }
  return (String + StrLen (String));
}

/**
  Run an internal shell command.

  This API will update the shell's environment since these commands are libraries.

  @param[in] CmdLine          the command line to run.
  @param[in] FirstParameter   the first parameter on the command line
  @param[in] ParamProtocol    the shell parameters protocol pointer
  @param[out] CommandStatus   the status from the command line.

  @retval EFI_SUCCESS     The command was completed.
  @retval EFI_ABORTED     The command's operation was aborted.
**/
EFI_STATUS
RunInternalCommand (
  IN CONST CHAR16                   *CmdLine,
  IN       CHAR16                   *FirstParameter,
  IN EFI_SHELL_PARAMETERS_PROTOCOL  *ParamProtocol,
  OUT EFI_STATUS                    *CommandStatus
  )
{
  EFI_STATUS                Status;
  UINTN                     Argc;
  CHAR16                    **Argv;
  SHELL_STATUS              CommandReturnedStatus;
  BOOLEAN                   LastError;
  CHAR16                    *Walker;
  CHAR16                    *NewCmdLine;

  NewCmdLine = AllocateCopyPool (StrSize (CmdLine), CmdLine);
  if (NewCmdLine == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  for (Walker = NewCmdLine; Walker != NULL && *Walker != CHAR_NULL ; Walker++) {
    if (*Walker == L'^' && * (Walker + 1) == L'#') {
      CopyMem (Walker, Walker + 1, StrSize (Walker) - sizeof (Walker[0]));
    }
  }

  //
  // get the argc and argv updated for internal commands
  //
  Status = UpdateArgcArgv (ParamProtocol, NewCmdLine, Internal_Command, &Argv, &Argc);
  if (!EFI_ERROR (Status)) {
    //
    // Run the internal command.
    //
    Status = ConsoleCommandRunCommandHandler (FirstParameter, &CommandReturnedStatus, &LastError);

    if (!EFI_ERROR (Status)) {
      if (CommandStatus != NULL) {
        if (CommandReturnedStatus != SHELL_SUCCESS) {
          *CommandStatus = (EFI_STATUS) (CommandReturnedStatus | MAX_BIT);
        } else {
          *CommandStatus = EFI_SUCCESS;
        }
      }

      //
      // Pass thru the exitcode from the app.
      //
      if (ConsoleCommandGetExit ()) {
        //
        // An Exit was requested ("exit" command), pass its value up.
        //
        Status = CommandReturnedStatus;
      }
    }
  }

  //
  // This is guaranteed to be called after UpdateArgcArgv no matter what else happened.
  // This is safe even if the update API failed.  In this case, it may be a no-op.
  //
  RestoreArgcArgv (ParamProtocol, &Argv, &Argc);

  FreePool (NewCmdLine);
  return (Status);
}

/**
  Function will process and run a command line.

  This will determine if the command line represents an internal shell
  command or dispatch an external application.

  @param[in] CmdLine      The command line to parse.
  @param[out] CommandStatus   The status from the command line.

  @retval EFI_SUCCESS     The command was completed.
  @retval EFI_ABORTED     The command's operation was aborted.
**/
EFI_STATUS
RunShellCommand (
  IN CONST CHAR16   *CmdLine,
  OUT EFI_STATUS    *CommandStatus
  )
{
  EFI_STATUS                Status;
  CHAR16                    *CleanOriginal;
  CHAR16                    *FirstParameter;
  CHAR16                    *TempWalker;
  SHELL_OPERATION_TYPES     Type;

  ASSERT (CmdLine != NULL);
  if (StrLen (CmdLine) == 0) {
    return (EFI_SUCCESS);
  }

  Status              = EFI_SUCCESS;
  CleanOriginal       = NULL;

  CleanOriginal = ConsoleStrnCatGrow (&CleanOriginal, NULL, CmdLine, 0);
  if (CleanOriginal == NULL) {
    return (EFI_OUT_OF_RESOURCES);
  }

  TrimSpaces (&CleanOriginal);

  //
  // NULL out comments (leveraged from RunScriptFileHandle() ).
  // The # character on a line is used to denote that all characters on the same line
  // and to the right of the # are to be ignored by the shell.
  // Afterwards, again remove spaces, in case any were between the last command-parameter and '#'.
  //
  for (TempWalker = CleanOriginal; TempWalker != NULL && *TempWalker != CHAR_NULL; TempWalker++) {
    if (*TempWalker == L'^') {
      if (* (TempWalker + 1) == L'#') {
        TempWalker++;
      }
    } else if (*TempWalker == L'#') {
      *TempWalker = CHAR_NULL;
    }
  }

  TrimSpaces (&CleanOriginal);

  //
  // Handle case that passed in command line is just 1 or more " " characters.
  //
  if (StrLen (CleanOriginal) == 0) {
    SHELL_FREE_NON_NULL (CleanOriginal);
    return (EFI_SUCCESS);
  }

  //
  // We need the first parameter information so we can determine the operation type
  //
  FirstParameter = AllocateZeroPool (StrSize (CleanOriginal));
  if (FirstParameter == NULL) {
    SHELL_FREE_NON_NULL (CleanOriginal);
    return (EFI_OUT_OF_RESOURCES);
  }

  TempWalker = CleanOriginal;
  if (!EFI_ERROR (GetNextParameter (&TempWalker, &FirstParameter, StrSize (CleanOriginal), TRUE))) {
    Type = Internal_Command;
    Status = RunInternalCommand (CleanOriginal, FirstParameter, NewShellParametersProtocol, CommandStatus);
  }

  SHELL_FREE_NON_NULL (CleanOriginal);
  SHELL_FREE_NON_NULL (FirstParameter);

  return (Status);
}

/**
  Function will process and run a command line.

  This will determine if the command line represents an internal shell
  command or dispatch an external application.

  @param[in] CmdLine      The command line to parse.

  @retval EFI_SUCCESS     The command was completed.
  @retval EFI_ABORTED     The command's operation was aborted.
**/
EFI_STATUS
RunCommand (
  IN CONST CHAR16   *CmdLine
  )
{
  return (RunShellCommand (CmdLine, NULL));
}

EFI_STATUS
EFIAPI
DoConsolePrompt (
  VOID
  )
{
  UINTN         Column;
  UINTN         Row;
  CHAR16        *CmdLine;
  CHAR16        *CurDir;
  UINTN         BufferSize;
  EFI_STATUS    Status;

  CurDir  = L"Console:/ # ";

  //
  // Get screen setting to decide size of the command line buffer
  //
  gST->ConOut->QueryMode (gST->ConOut, gST->ConOut->Mode->Mode, &Column, &Row);
  BufferSize  = Column * Row * sizeof (CHAR16);
  CmdLine     = AllocateZeroPool (BufferSize);
  if (CmdLine == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Prompt for input
  //
  gST->ConOut->SetCursorPosition (gST->ConOut, 0, gST->ConOut->Mode->CursorRow);

  if (CurDir != NULL && StrLen (CurDir) > 1) {
    gST->ConOut->OutputString (gST->ConOut, CurDir);
  }

  //
  // Read a line from the console
  //
  Status = UefiConsoleInRead (&BufferSize, CmdLine);

  //
  // Null terminate the string and parse it
  //
  if (!EFI_ERROR (Status)) {
    CmdLine[BufferSize / sizeof (CHAR16)] = CHAR_NULL;

    Status = RunCommand (CmdLine);
  }

  //
  // Done with this command
  //
  FreePool (CmdLine);
  return Status;
}

/**
  Notification function for keystrokes.

  @param[in] KeyData    The key that was pressed.

  @retval EFI_SUCCESS   The operation was successful.

**/
EFI_STATUS
EFIAPI
NotificationFunction (
  IN EFI_KEY_DATA *KeyData
  )
{
  EFI_STATUS          Status;

  if (((KeyData->Key.UnicodeChar == CONSOLE_KEY) &&
       (KeyData->KeyState.KeyShiftState == (EFI_SHIFT_STATE_VALID | EFI_LEFT_CONTROL_PRESSED) ||
        KeyData->KeyState.KeyShiftState  == (EFI_SHIFT_STATE_VALID | EFI_RIGHT_CONTROL_PRESSED))) ||
      (KeyData->Key.UnicodeChar == CONSOLE_KEY - L'a' + 1)) {

    Status = gBS->CheckEvent (mTimerEvent);
    if (Status == EFI_NOT_READY) {
      DEBUG ((DEBUG_INFO, "Console request ignored\n"));
      return EFI_SUCCESS;
    }

    gST->ConOut->ClearScreen (gST->ConOut);
    gST->ConOut->OutputString (gST->ConOut, L"\nWelcome to UEFI Console\n\n");

    mExitRequested = FALSE;

    do {
      //
      // Display Prompt
      //
      Status = DoConsolePrompt ();

    } while (!ConsoleCommandGetExit ());

    //
    // Ignore future Console requests for some period.
    //
    Status = gBS->SetTimer (mTimerEvent, TimerRelative, CONSOLE_REQUEST_DELAY);
  }

  return EFI_SUCCESS;
}

/**
  Function to start monitoring for CTRL-r using SimpleTextInputEx.

  @retval EFI_SUCCESS           The feature is enabled.
  @retval EFI_OUT_OF_RESOURCES  There is not enough memory available.

**/
EFI_STATUS
EFIAPI
UefiConsoleStartMonitor (
  VOID
  )
{
  EFI_STATUS                        Status;
  EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *SimpleEx;
  EFI_KEY_DATA                      KeyData;

  NotifyHandle0 = NULL;
  NotifyHandle1 = NULL;
  NotifyHandle2 = NULL;
  NotifyHandle3 = NULL;

  Status = gBS->OpenProtocol (
                  gST->ConsoleInHandle,
                  &gEfiSimpleTextInputExProtocolGuid,
                  (VOID **) &SimpleEx,
                  gImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR(Status)) {
    Print (L"No SimpleTextInputEx was found.\n");
    return Status;
  }

  KeyData.KeyState.KeyToggleState = 0;
  KeyData.Key.ScanCode            = 0;
  KeyData.KeyState.KeyShiftState  = EFI_SHIFT_STATE_VALID | EFI_LEFT_CONTROL_PRESSED;
  KeyData.Key.UnicodeChar         = CONSOLE_KEY;

  Status = SimpleEx->RegisterKeyNotify (
                       SimpleEx,
                       &KeyData,
                       NotificationFunction,
                       &NotifyHandle0
                       );

  KeyData.KeyState.KeyShiftState  = EFI_SHIFT_STATE_VALID | EFI_RIGHT_CONTROL_PRESSED;
  if (!EFI_ERROR (Status)) {
    Status = SimpleEx->RegisterKeyNotify (
                         SimpleEx,
                         &KeyData,
                         NotificationFunction,
                         &NotifyHandle1
                         );
  }

  KeyData.KeyState.KeyShiftState  = EFI_SHIFT_STATE_VALID | EFI_LEFT_CONTROL_PRESSED;
  KeyData.Key.UnicodeChar         = CONSOLE_KEY - L'a' + 1;
  if (!EFI_ERROR (Status)) {
    Status = SimpleEx->RegisterKeyNotify (
                         SimpleEx,
                         &KeyData,
                         NotificationFunction,
                         &NotifyHandle2
                         );
  }

  KeyData.KeyState.KeyShiftState  = EFI_SHIFT_STATE_VALID | EFI_RIGHT_CONTROL_PRESSED;
  if (!EFI_ERROR (Status)) {
    Status = SimpleEx->RegisterKeyNotify (
                         SimpleEx,
                         &KeyData,
                         NotificationFunction,
                         &NotifyHandle3
                         );
  }

  if (!EFI_ERROR (Status)) {
    //
    // Create the key hold off timer
    //
    Status = gBS->CreateEvent (
                    EVT_TIMER,
                    0,
                    NULL,
                    NULL,
                    &mTimerEvent
                    );
    if (!EFI_ERROR (Status)) {
      //
      // Place event into the signaled state indicating Console is active.
      //
      Status = gBS->SignalEvent (mTimerEvent);
    }
  }

  return Status;
}

/**
  Function to stop monitoring for CTRL-r using SimpleTextInputEx.

  @retval EFI_SUCCESS           The feature is enabled.
  @retval EFI_OUT_OF_RESOURCES  There is not enough memory available.

**/
EFI_STATUS
EFIAPI
UefiConsoleStopMonitor (
  VOID
  )
{
  EFI_STATUS                        Status;
  EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *SimpleEx;

  Status = gBS->OpenProtocol (
                  gST->ConsoleInHandle,
                  &gEfiSimpleTextInputExProtocolGuid,
                  (VOID **) &SimpleEx,
                  gImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (!EFI_ERROR (Status)) {
    Status = SimpleEx->UnregisterKeyNotify (SimpleEx, NotifyHandle0);
    Status = SimpleEx->UnregisterKeyNotify (SimpleEx, NotifyHandle1);
    Status = SimpleEx->UnregisterKeyNotify (SimpleEx, NotifyHandle2);
    Status = SimpleEx->UnregisterKeyNotify (SimpleEx, NotifyHandle3);
  }

  if (mTimerEvent != NULL) {
    gBS->SetTimer (mTimerEvent, TimerCancel, 0);
    gBS->CloseEvent (mTimerEvent);
  }

  return (Status);
}

/**
  The entry point for UEFI console feature.

  @param[in] ImageHandle  The firmware allocated handle for the EFI image.
  @param[in] SystemTable  A pointer to the EFI System Table.

  @retval EFI_SUCCESS     The entry point is executed successfully.
  @retval other           Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
UefiConsoleEntry (
  IN EFI_HANDLE           ImageHandle,
  IN EFI_SYSTEM_TABLE     *SystemTable
  )
{
  EFI_STATUS                    Status;

  ConsoleCommandsConstructor (ImageHandle, SystemTable);

  ZeroMem (&ViewingSettings, sizeof (SHELL_VIEWING_SETTINGS));
  ViewingSettings.InsertMode = FALSE;
  InitializeListHead (&ViewingSettings.CommandHistory.Link);

  //
  // Allocate the new structure
  //
  NewShellParametersProtocol = AllocateZeroPool (sizeof (EFI_SHELL_PARAMETERS_PROTOCOL));
  if (NewShellParametersProtocol == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = UefiConsoleStartMonitor ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "UefiConsoleStartMonitor failed\n"));
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Free any resources for UEFI console feature.

  @param[in] VOID

  @retval EFI_SUCCESS     The routine is executed successfully.
  @retval other           Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
UefiConsoleUnload (
  VOID
  )
{
  EFI_STATUS              Status;

  if (NewShellParametersProtocol != NULL) {
    FreePool (NewShellParametersProtocol);
  }

  Status = UefiConsoleStopMonitor ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Console monitor stop failed."));
    return Status;
  }

  Status = ConsoleCommandsDestructor ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Console command free failed."));
    return Status;
  }

  return EFI_SUCCESS;
}
