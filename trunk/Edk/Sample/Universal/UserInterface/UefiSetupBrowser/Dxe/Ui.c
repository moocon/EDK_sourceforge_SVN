/*++

Copyright (c) 2004 - 2009, Intel Corporation
All rights reserved. This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

Module Name:

  Ui.c

Abstract:

  Implementation for UI.

Revision History

--*/

#include "Ui.h"
#include "Setup.h"

EFI_LIST_ENTRY      Menu;
EFI_LIST_ENTRY      gMenuList = INITIALIZE_LIST_HEAD_VARIABLE (gMenuList);
MENU_REFRESH_ENTRY  *gMenuRefreshHead;

//
// Search table for UiDisplayMenu()
//
SCAN_CODE_TO_SCREEN_OPERATION     gScanCodeToOperation[] = {
  SCAN_UP,
  UiUp,
  SCAN_DOWN,
  UiDown,
  SCAN_PAGE_UP,
  UiPageUp,
  SCAN_PAGE_DOWN,
  UiPageDown,
  SCAN_ESC,
  UiReset,
  SCAN_LEFT,
  UiLeft,
  SCAN_RIGHT,
  UiRight,
  SCAN_F9,
  UiDefault,
  SCAN_F10,
  UiSave
};

SCREEN_OPERATION_T0_CONTROL_FLAG  gScreenOperationToControlFlag[] = {
  UiNoOperation,
  CfUiNoOperation,
  UiDefault,
  CfUiDefault,
  UiSelect,
  CfUiSelect,
  UiUp,
  CfUiUp,
  UiDown,
  CfUiDown,
  UiLeft,
  CfUiLeft,
  UiRight,
  CfUiRight,
  UiReset,
  CfUiReset,
  UiSave,
  CfUiSave,
  UiPageUp,
  CfUiPageUp,
  UiPageDown,
  CfUiPageDown
};

VOID
SetUnicodeMem (
  IN VOID   *Buffer,
  IN UINTN  Size,
  IN CHAR16 Value
  )
/*++

Routine Description:
  Set Buffer to Value for Size bytes.

Arguments:
  Buffer  - Memory to set.
  Size    - Number of bytes to set
  Value   - Value of the set operation.

Returns:
  None

--*/
{
  CHAR16  *Ptr;

  Ptr = Buffer;
  while (Size--) {
    *(Ptr++) = Value;
  }
}

VOID
StrnCpy (
  IN CHAR16   *Dst,
  IN CHAR16   *Src,
  IN UINTN    Length
  )
/*++

Routine Description:
  Copy n character from Src to Dst.

Arguments:
  Dst    - Destination string.
  Src    - Source string.
  Length - Number of character to copy.

Returns:
  None

--*/
{
  while (*Src && Length) {
    *(Dst++) = *(Src++);
    Length--;
  }

  *Dst = L'\0';
}

VOID
UiInitMenu (
  VOID
  )
/*++

Routine Description:
  Initialize Menu option list.

Arguments:
  None.

Returns:
  None.

--*/
{
  InitializeListHead (&Menu);
}

VOID
UiFreeMenu (
  VOID
  )
/*++

Routine Description:
  Free Menu option linked list.

Arguments:
  None.

Returns:
  None.

--*/
{
  UI_MENU_OPTION  *MenuOption;

  while (!IsListEmpty (&Menu)) {
    MenuOption = MENU_OPTION_FROM_LINK (Menu.ForwardLink);
    RemoveEntryList (&MenuOption->Link);

    //
    // We allocated space for this description when we did a GetToken, free it here
    //
    if (MenuOption->Skip != 0) {
      //
      // For date/time, MenuOption->Description is shared by three Menu Options
      // Data format :      [01/02/2004]      [11:22:33]
      // Line number :        0  0    1         0  0  1
      //
      gBS->FreePool (MenuOption->Description);
    }
    gBS->FreePool (MenuOption);
  }
}

UI_MENU_LIST *
UiAddMenuList (
  IN OUT UI_MENU_LIST     *Parent,
  IN EFI_GUID             *FormSetGuid,
  IN UINT16               FormId
  )
/*++

Routine Description:
  Create a menu with specified formset GUID and form ID, and add it as a child
  of the given parent menu.

Arguments:
  Parent      - The parent of this menu.
  FormSetGuid - The formset Guid of this menu.
  FormId      - The form ID of this menu.

Returns:
  Pointer to the newly added menu
  NULL - insufficient memory

--*/
{
  UI_MENU_LIST  *MenuList;

  MenuList = EfiLibAllocateZeroPool (sizeof (UI_MENU_LIST));
  if (MenuList == NULL) {
    return NULL;
  }

  MenuList->Signature = UI_MENU_LIST_SIGNATURE;
  InitializeListHead (&MenuList->ChildListHead);

  EfiCopyMem (&MenuList->FormSetGuid, FormSetGuid, sizeof (EFI_GUID));
  MenuList->FormId = FormId;
  MenuList->Parent = Parent;

  if (Parent == NULL) {
    //
    // The parent is not specified, so this is the root Form of a Formset
    //
    InsertTailList (&gMenuList, &MenuList->Link);
  } else {
    InsertTailList (&Parent->ChildListHead, &MenuList->Link);
  }

  return MenuList;
}

UI_MENU_LIST *
UiFindChildMenuList (
  IN UI_MENU_LIST         *Parent,
  IN UINT16               FormId
  )
/*++

Routine Description:
  Search Menu with given FormId in the parent menu and all its child menu.

Arguments:
  Parent    - The parent menu.
  FormId    - The FormId of the menu to search.

Returns:
  Pointer to the menu with this FormId
  NULL - menu not found

--*/
{
  EFI_LIST_ENTRY  *Link;
  UI_MENU_LIST    *Child;
  UI_MENU_LIST    *MenuList;

  if (Parent->FormId == FormId) {
    return Parent;
  }

  Link = GetFirstNode (&Parent->ChildListHead);
  while (!IsNull (&Parent->ChildListHead, Link)) {
    Child = UI_MENU_LIST_FROM_LINK (Link);

    MenuList = UiFindChildMenuList (Child, FormId);
    if (MenuList != NULL) {
      return MenuList;
    }

    Link = GetNextNode (&Parent->ChildListHead, Link);
  }

  return NULL;
}

UI_MENU_LIST *
UiFindMenuList (
  IN EFI_GUID             *FormSetGuid,
  IN UINT16               FormId
  )
/*++

Routine Description:
  Search Menu with given FormSetGuid and FormId in all menu list.

Arguments:
  FormSetGuid - The formset GUID of the menu to search.
  FormId      - The FormId of the menu to search.

Returns:
  Pointer to the menu found
  NULL - menu not found

--*/
{
  EFI_LIST_ENTRY  *Link;
  UI_MENU_LIST    *MenuList;
  UI_MENU_LIST    *Child;

  Link = GetFirstNode (&gMenuList);
  while (!IsNull (&gMenuList, Link)) {
    MenuList = UI_MENU_LIST_FROM_LINK (Link);

    if (EfiCompareGuid (FormSetGuid, &MenuList->FormSetGuid)) {
      //
      // This is the formset we are looking for, find the form in this formset
      //
      Child = UiFindChildMenuList (MenuList, FormId);
      if (Child != NULL) {
        return Child;
      }
    }

    Link = GetNextNode (&gMenuList, Link);
  }

  return NULL;
}

VOID
UiFreeRefreshList (
  VOID
  )
/*++

Routine Description:
  Free Menu option linked list.

Arguments:
  None.

Returns:
  None.

--*/
{
  MENU_REFRESH_ENTRY  *OldMenuRefreshEntry;

  while (gMenuRefreshHead != NULL) {
    OldMenuRefreshEntry = gMenuRefreshHead->Next;
    gBS->FreePool (gMenuRefreshHead);
    gMenuRefreshHead = OldMenuRefreshEntry;
  }

  gMenuRefreshHead = NULL;
}

EFI_STATUS
RefreshForm (
  VOID
  )
/*++

Routine Description:
  Refresh screen.

Arguments:
  None.

Returns:
  None.

--*/
{
  CHAR16                          *OptionString;
  MENU_REFRESH_ENTRY              *MenuRefreshEntry;
  UINTN                           Index;
  EFI_STATUS                      Status;
  UI_MENU_SELECTION               *Selection;
  FORM_BROWSER_STATEMENT          *Question;
  EFI_HII_CONFIG_ACCESS_PROTOCOL  *ConfigAccess;
  EFI_HII_VALUE                   *HiiValue;
  EFI_BROWSER_ACTION_REQUEST      ActionRequest;
  BOOLEAN                         ValueChanged;

  if (gMenuRefreshHead != NULL) {

    ValueChanged = FALSE;
    MenuRefreshEntry = gMenuRefreshHead;

    //
    // Reset FormPackage update flag
    //
    mHiiPackageListUpdated = FALSE;

    do {
      gST->ConOut->SetAttribute (gST->ConOut, MenuRefreshEntry->CurrentAttribute);

      Selection = MenuRefreshEntry->Selection;
      Question = MenuRefreshEntry->MenuOption->ThisTag;

      Status = GetQuestionValue (Selection->FormSet, Selection->Form, Question, FALSE);
      if (!EFI_ERROR (Status)) {
        OptionString = NULL;
        ProcessOptions (Selection, MenuRefreshEntry->MenuOption, FALSE, &OptionString);

        if (OptionString != NULL) {
          //
          // If leading spaces on OptionString - remove the spaces
          //
          for (Index = 0; OptionString[Index] == L' '; Index++)
            ;

          PrintStringAt (MenuRefreshEntry->CurrentColumn, MenuRefreshEntry->CurrentRow, &OptionString[Index]);
          gBS->FreePool (OptionString);
        }
      }

      //
      // Question value may be changed, need invoke its Callback()
      //
      ConfigAccess = Selection->FormSet->ConfigAccess;
      if ((Question->QuestionFlags & EFI_IFR_FLAG_CALLBACK) && (ConfigAccess != NULL)) {
        ActionRequest = EFI_BROWSER_ACTION_REQUEST_NONE;

        HiiValue = &Question->HiiValue;
        if (HiiValue->Type == EFI_IFR_TYPE_STRING) {
          //
          // Create String in HII database for Configuration Driver to retrieve
          //
          HiiValue->Value.string = NewString ((CHAR16 *) Question->BufferValue, Selection->FormSet->HiiHandle);
        }

        Status = ConfigAccess->Callback (
                                 ConfigAccess,
                                 EFI_BROWSER_ACTION_CHANGING,
                                 Question->QuestionId,
                                 HiiValue->Type,
                                 &HiiValue->Value,
                                 &ActionRequest
                                 );

        if (HiiValue->Type == EFI_IFR_TYPE_STRING) {
          //
          // Clean the String in HII Database
          //
          DeleteString (HiiValue->Value.string, Selection->FormSet->HiiHandle);
        }

        if (!EFI_ERROR (Status)) {
          switch (ActionRequest) {
          case EFI_BROWSER_ACTION_REQUEST_RESET:
            gResetRequired = TRUE;
            break;

          case EFI_BROWSER_ACTION_REQUEST_SUBMIT:
            SubmitForm (Selection->FormSet, Selection->Form);
            break;

          case EFI_BROWSER_ACTION_REQUEST_EXIT:
            Selection->Action = UI_ACTION_EXIT;
            gNvUpdateRequired = FALSE;
            break;

          default:
            break;
          }
        }
      }

      if ((Question->Operand != EFI_IFR_DATE_OP) && (Question->Operand != EFI_IFR_TIME_OP)) {
        //
        // Ignore the change for EFI_IFR_DATE and EFI_IFR_TIME
        //
        ValueChanged = TRUE;
      }

      MenuRefreshEntry = MenuRefreshEntry->Next;

    } while (MenuRefreshEntry != NULL);

    if (mHiiPackageListUpdated) {
      //
      // Package list is updated, force to reparse IFR binary of target Formset
      //
      mHiiPackageListUpdated = FALSE;
      Selection->Action = UI_ACTION_REFRESH_FORMSET;
      return EFI_SUCCESS;
    }

    if (ValueChanged) {
      //
      // Question's value may be changed, need refresh the Form
      //
      Selection->Action = UI_ACTION_REFRESH_FORM;
      return EFI_SUCCESS;
    }
  }

  return EFI_TIMEOUT;
}

EFI_STATUS
UiWaitForSingleEvent (
  IN EFI_EVENT                Event,
  IN UINT64                   Timeout, OPTIONAL
  IN UINT8                    RefreshInterval OPTIONAL
  )
/*++

Routine Description:
  Wait for a given event to fire, or for an optional timeout to expire.

Arguments:
  Event            - The event to wait for
  Timeout          - An optional timeout value in 100 ns units.
  RefreshInterval  - Menu refresh interval (in seconds).

Returns:
  EFI_SUCCESS      - Event fired before Timeout expired.
  EFI_TIME_OUT     - Timout expired before Event fired.

--*/
{
  EFI_STATUS  Status;
  UINTN       Index;
  EFI_EVENT   TimerEvent;
  EFI_EVENT   WaitList[2];

  if (Timeout) {
    //
    // Create a timer event
    //
    Status = gBS->CreateEvent (EFI_EVENT_TIMER, 0, NULL, NULL, &TimerEvent);
    if (!EFI_ERROR (Status)) {
      //
      // Set the timer event
      //
      gBS->SetTimer (
            TimerEvent,
            TimerRelative,
            Timeout
            );

      //
      // Wait for the original event or the timer
      //
      WaitList[0] = Event;
      WaitList[1] = TimerEvent;
      Status      = gBS->WaitForEvent (2, WaitList, &Index);
      gBS->CloseEvent (TimerEvent);

      //
      // If the timer expired, change the return to timed out
      //
      if (!EFI_ERROR (Status) && Index == 1) {
        Status = EFI_TIMEOUT;
      }
    }
  } else {
    //
    // Update screen every second
    //
    if (RefreshInterval == 0) {
      Timeout = ONE_SECOND;
    } else {
      Timeout = RefreshInterval * ONE_SECOND;
    }

    do {
      Status = gBS->CreateEvent (EFI_EVENT_TIMER, 0, NULL, NULL, &TimerEvent);

      //
      // Set the timer event
      //
      gBS->SetTimer (
            TimerEvent,
            TimerRelative,
            Timeout
            );

      //
      // Wait for the original event or the timer
      //
      WaitList[0] = Event;
      WaitList[1] = TimerEvent;
      Status      = gBS->WaitForEvent (2, WaitList, &Index);

      //
      // If the timer expired, update anything that needs a refresh and keep waiting
      //
      if (!EFI_ERROR (Status) && Index == 1) {
        Status = EFI_TIMEOUT;
        if (RefreshInterval != 0) {
          Status = RefreshForm ();
        }
      }

      gBS->CloseEvent (TimerEvent);
    } while (Status == EFI_TIMEOUT);
  }

  return Status;
}

UI_MENU_OPTION *
UiAddMenuOption (
  IN CHAR16                  *String,
  IN EFI_HII_HANDLE          Handle,
  IN FORM_BROWSER_STATEMENT  *Statement,
  IN UINT16                  NumberOfLines,
  IN UINT16                  MenuItemCount
  )
/*++

Routine Description:
  Add one menu option by specified description and context.

Arguments:
  String        - String description for this option.
  Handle        - Hii handle for the package list.
  Statement     - Statement of this Menu Option.
  NumberOfLines - Display lines for this Menu Option.
  MenuItemCount - The index for this Option in the Menu.

Returns:
  None.

--*/
{
  UI_MENU_OPTION  *MenuOption;
  UINTN           Index;
  UINTN           Count;

  Count = 1;
  MenuOption = NULL;

  if (Statement->Operand == EFI_IFR_DATE_OP || Statement->Operand == EFI_IFR_TIME_OP) {
    //
    // Add three MenuOptions for Date/Time
    // Data format :      [01/02/2004]      [11:22:33]
    // Line number :        0  0    1         0  0  1
    //
    NumberOfLines = 0;
    Count = 3;

    if (Statement->Storage == NULL) {
      //
      // For RTC type of date/time, set default refresh interval to be 1 second
      //
      if (Statement->RefreshInterval == 0) {
        Statement->RefreshInterval = 1;
      }
    }
  }

  for (Index = 0; Index < Count; Index++) {
    MenuOption = EfiLibAllocateZeroPool (sizeof (UI_MENU_OPTION));
    ASSERT (MenuOption);

    MenuOption->Signature   = UI_MENU_OPTION_SIGNATURE;
    MenuOption->Description = String;
    MenuOption->Handle      = Handle;
    MenuOption->ThisTag     = Statement;
    MenuOption->EntryNumber = MenuItemCount;

    if (Index == 2) {
      //
      // Override LineNumber for the MenuOption in Date/Time sequence
      //
      MenuOption->Skip = 1;
    } else {
      MenuOption->Skip = NumberOfLines;
    }
    MenuOption->Sequence = Index;

    if (Statement->GrayOutExpression != NULL) {
      MenuOption->GrayOut = Statement->GrayOutExpression->Result.Value.b;
    }

    switch (Statement->Operand) {
    case EFI_IFR_ORDERED_LIST_OP:
    case EFI_IFR_ONE_OF_OP:
    case EFI_IFR_NUMERIC_OP:
    case EFI_IFR_TIME_OP:
    case EFI_IFR_DATE_OP:
    case EFI_IFR_CHECKBOX_OP:
    case EFI_IFR_PASSWORD_OP:
    case EFI_IFR_STRING_OP:
      //
      // User could change the value of these items
      //
      MenuOption->IsQuestion = TRUE;
      break;

    default:
      MenuOption->IsQuestion = FALSE;
      break;
    }

    if ((Statement->ValueExpression != NULL) ||
        (Statement->QuestionFlags & EFI_IFR_FLAG_READ_ONLY)) {
      MenuOption->ReadOnly = TRUE;
    }

    InsertTailList (&Menu, &MenuOption->Link);
  }

  return MenuOption;
}

EFI_STATUS
CreateDialog (
  IN  UINTN                       NumberOfLines,
  IN  BOOLEAN                     HotKey,
  IN  UINTN                       MaximumStringSize,
  OUT CHAR16                      *StringBuffer,
  OUT EFI_INPUT_KEY               *KeyValue,
  IN  CHAR16                      *String,
  ...
  )
/*++

Routine Description:
  Routine used to abstract a generic dialog interface and return the selected key or string

Arguments:
  NumberOfLines     - The number of lines for the dialog box
  HotKey            - Defines whether a single character is parsed (TRUE) and returned in KeyValue
                      or a string is returned in StringBuffer.  Two special characters are considered when entering a string, a SCAN_ESC and
                      an CHAR_CARRIAGE_RETURN.  SCAN_ESC terminates string input and returns
  MaximumStringSize - The maximum size in bytes of a typed in string (each character is a CHAR16) and the minimum string returned is two bytes
  StringBuffer      - The passed in pointer to the buffer which will hold the typed in string if HotKey is FALSE
  KeyValue          - The EFI_KEY value returned if HotKey is TRUE..
  String            - Pointer to the first string in the list
  ...               - A series of (quantity == NumberOfLines) text strings which will be used to construct the dialog box

Returns:
  EFI_SUCCESS           - Displayed dialog and received user interaction
  EFI_INVALID_PARAMETER - One of the parameters was invalid (e.g. (StringBuffer == NULL) && (HotKey == FALSE))
  EFI_DEVICE_ERROR      - User typed in an ESC character to exit the routine

--*/
{
  VA_LIST       Marker;
  UINTN         Count;
  EFI_INPUT_KEY Key;
  UINTN         LargestString;
  CHAR16        *TempString;
  CHAR16        *BufferedString;
  CHAR16        *StackString;
  CHAR16        KeyPad[2];
  UINTN         Start;
  UINTN         Top;
  UINTN         Index;
  EFI_STATUS    Status;
  BOOLEAN       SelectionComplete;
  UINTN         InputOffset;
  UINTN         CurrentAttribute;
  UINTN         DimensionsWidth;
  UINTN         DimensionsHeight;
  BOOLEAN       CursorVisible;

  DimensionsWidth   = gScreenDimensions.RightColumn - gScreenDimensions.LeftColumn;
  DimensionsHeight  = gScreenDimensions.BottomRow - gScreenDimensions.TopRow;

  SelectionComplete = FALSE;
  InputOffset       = 0;
  TempString        = EfiLibAllocateZeroPool (MaximumStringSize * 2);
  BufferedString    = EfiLibAllocateZeroPool (MaximumStringSize * 2);
  CurrentAttribute  = gST->ConOut->Mode->Attribute;

  ASSERT (TempString);
  ASSERT (BufferedString);

  VA_START (Marker, String);

  //
  // Zero the outgoing buffer
  //
  EfiZeroMem (StringBuffer, MaximumStringSize);

  if (HotKey) {
    if (KeyValue == NULL) {
      return EFI_INVALID_PARAMETER;
    }
  } else {
    if (StringBuffer == NULL) {
      return EFI_INVALID_PARAMETER;
    }
  }
  //
  // Disable cursor
  //
  CursorVisible = gST->ConOut->Mode->CursorVisible;
  gST->ConOut->EnableCursor (gST->ConOut, FALSE);

  LargestString = (GetStringWidth (String) / 2);

  if (*String == L' ') {
    InputOffset = 1;
  }
  //
  // Determine the largest string in the dialog box
  // Notice we are starting with 1 since String is the first string
  //
  for (Count = 1; Count < NumberOfLines; Count++) {
    StackString = VA_ARG (Marker, CHAR16 *);

    if (StackString[0] == L' ') {
      InputOffset = Count + 1;
    }

    if ((GetStringWidth (StackString) / 2) > LargestString) {
      //
      // Size of the string visually and subtract the width by one for the null-terminator
      //
      LargestString = (GetStringWidth (StackString) / 2);
    }
  }

  Start = (DimensionsWidth - LargestString - 2) / 2 + gScreenDimensions.LeftColumn + 1;
  Top   = ((DimensionsHeight - NumberOfLines - 2) / 2) + gScreenDimensions.TopRow - 1;

  Count = 0;

  //
  // Display the Popup
  //
  CreateSharedPopUp (LargestString, NumberOfLines, &String);

  //
  // Take the first key typed and report it back?
  //
  if (HotKey) {
    Status = WaitForKeyStroke (&Key);
    EfiCopyMem (KeyValue, &Key, sizeof (EFI_INPUT_KEY));

  } else {
    do {
      Status = WaitForKeyStroke (&Key);

      switch (Key.UnicodeChar) {
      case CHAR_NULL:
        switch (Key.ScanCode) {
        case SCAN_ESC:
          gBS->FreePool (TempString);
          gBS->FreePool (BufferedString);
          gST->ConOut->SetAttribute (gST->ConOut, CurrentAttribute);
          gST->ConOut->EnableCursor (gST->ConOut, CursorVisible);
          return EFI_DEVICE_ERROR;

        default:
          break;
        }

        break;

      case CHAR_CARRIAGE_RETURN:
        SelectionComplete = TRUE;
        gBS->FreePool (TempString);
        gBS->FreePool (BufferedString);
        gST->ConOut->SetAttribute (gST->ConOut, CurrentAttribute);
        gST->ConOut->EnableCursor (gST->ConOut, CursorVisible);
        return EFI_SUCCESS;
        break;

      case CHAR_BACKSPACE:
        if (StringBuffer[0] != CHAR_NULL) {
          for (Index = 0; StringBuffer[Index] != CHAR_NULL; Index++) {
            TempString[Index] = StringBuffer[Index];
          }
          //
          // Effectively truncate string by 1 character
          //
          TempString[Index - 1] = CHAR_NULL;
          EfiStrCpy (StringBuffer, TempString);
        }

      default:
        //
        // If it is the beginning of the string, don't worry about checking maximum limits
        //
        if ((StringBuffer[0] == CHAR_NULL) && (Key.UnicodeChar != CHAR_BACKSPACE)) {
          StrnCpy (StringBuffer, &Key.UnicodeChar, 1);
          StrnCpy (TempString, &Key.UnicodeChar, 1);
        } else if ((GetStringWidth (StringBuffer) < MaximumStringSize) && (Key.UnicodeChar != CHAR_BACKSPACE)) {
          KeyPad[0] = Key.UnicodeChar;
          KeyPad[1] = CHAR_NULL;
          EfiStrCat (StringBuffer, KeyPad);
          EfiStrCat (TempString, KeyPad);
        }
        //
        // If the width of the input string is now larger than the screen, we nee to
        // adjust the index to start printing portions of the string
        //
        SetUnicodeMem (BufferedString, LargestString, L' ');

        PrintStringAt (Start + 1, Top + InputOffset, BufferedString);

        if ((GetStringWidth (StringBuffer) / 2) > (DimensionsWidth - 2)) {
          Index = (GetStringWidth (StringBuffer) / 2) - DimensionsWidth + 2;
        } else {
          Index = 0;
        }

        for (Count = 0; Index + 1 < GetStringWidth (StringBuffer) / 2; Index++, Count++) {
          BufferedString[Count] = StringBuffer[Index];
        }

        PrintStringAt (Start + 1, Top + InputOffset, BufferedString);
        break;
      }
    } while (!SelectionComplete);
  }

  gST->ConOut->SetAttribute (gST->ConOut, CurrentAttribute);
  gST->ConOut->EnableCursor (gST->ConOut, CursorVisible);
  return EFI_SUCCESS;
}

VOID
CreateSharedPopUp (
  IN  UINTN                       RequestedWidth,
  IN  UINTN                       NumberOfLines,
  IN  CHAR16                      **ArrayOfStrings
  )
{
  UINTN   Index;
  UINTN   Count;
  CHAR16  Character;
  UINTN   Start;
  UINTN   End;
  UINTN   Top;
  UINTN   Bottom;
  CHAR16  *String;
  UINTN   DimensionsWidth;
  UINTN   DimensionsHeight;

  DimensionsWidth   = gScreenDimensions.RightColumn - gScreenDimensions.LeftColumn;
  DimensionsHeight  = gScreenDimensions.BottomRow - gScreenDimensions.TopRow;

  Count = 0;

  gST->ConOut->SetAttribute (gST->ConOut, POPUP_TEXT | POPUP_BACKGROUND);

  if ((RequestedWidth + 2) > DimensionsWidth) {
    RequestedWidth = DimensionsWidth - 2;
  }

  //
  // Subtract the PopUp width from total Columns, allow for one space extra on
  // each end plus a border.
  //
  Start     = (DimensionsWidth - RequestedWidth - 2) / 2 + gScreenDimensions.LeftColumn + 1;
  End       = Start + RequestedWidth + 1;

  Top       = ((DimensionsHeight - NumberOfLines - 2) / 2) + gScreenDimensions.TopRow - 1;
  Bottom    = Top + NumberOfLines + 2;

  Character = BOXDRAW_DOWN_RIGHT;
  PrintCharAt (Start, Top, Character);
  Character = BOXDRAW_HORIZONTAL;
  for (Index = Start; Index + 2 < End; Index++) {
    PrintChar (Character);
  }

  Character = BOXDRAW_DOWN_LEFT;
  PrintChar (Character);
  Character = BOXDRAW_VERTICAL;
  for (Index = Top; Index + 2 < Bottom; Index++) {
    String = ArrayOfStrings[Count];
    Count++;

    //
    // This will clear the background of the line - we never know who might have been
    // here before us.  This differs from the next clear in that it used the non-reverse
    // video for normal printing.
    //
    if (GetStringWidth (String) / 2 > 1) {
      ClearLines (Start, End, Index + 1, Index + 1, POPUP_TEXT | POPUP_BACKGROUND);
    }

    //
    // Passing in a space results in the assumption that this is where typing will occur
    //
    if (String[0] == L' ') {
      ClearLines (Start + 1, End - 1, Index + 1, Index + 1, POPUP_INVERSE_TEXT | POPUP_INVERSE_BACKGROUND);
    }

    //
    // Passing in a NULL results in a blank space
    //
    if (String[0] == CHAR_NULL) {
      ClearLines (Start, End, Index + 1, Index + 1, POPUP_TEXT | POPUP_BACKGROUND);
    }

    PrintStringAt (
      ((DimensionsWidth - GetStringWidth (String) / 2) / 2) + gScreenDimensions.LeftColumn + 1,
      Index + 1,
      String
      );
    gST->ConOut->SetAttribute (gST->ConOut, POPUP_TEXT | POPUP_BACKGROUND);
    PrintCharAt (Start, Index + 1, Character);
    PrintCharAt (End - 1, Index + 1, Character);
  }

  Character = BOXDRAW_UP_RIGHT;
  PrintCharAt (Start, Bottom - 1, Character);
  Character = BOXDRAW_HORIZONTAL;
  for (Index = Start; Index + 2 < End; Index++) {
    PrintChar (Character);
  }

  Character = BOXDRAW_UP_LEFT;
  PrintChar (Character);
}

VOID
CreatePopUp (
  IN  UINTN                       RequestedWidth,
  IN  UINTN                       NumberOfLines,
  IN  CHAR16                      *ArrayOfStrings,
  ...
  )
{
  CreateSharedPopUp (RequestedWidth, NumberOfLines, &ArrayOfStrings);
}

VOID
UpdateStatusBar (
  IN  UINTN                       MessageType,
  IN  UINT8                       Flags,
  IN  BOOLEAN                     State
  )
/*++

Routine Description:
  Update status bar on the bottom of menu.

Arguments:
  MessageType - The type of message to be shown.
  Flags       - The flags in Question header.
  State       - Set or clear.

Returns:
  None.

--*/
{
  UINTN           Index;
  STATIC BOOLEAN  InputError;
  CHAR16          *NvUpdateMessage;
  CHAR16          *InputErrorMessage;

  NvUpdateMessage   = GetToken (STRING_TOKEN (NV_UPDATE_MESSAGE), gHiiHandle);
  InputErrorMessage = GetToken (STRING_TOKEN (INPUT_ERROR_MESSAGE), gHiiHandle);

  switch (MessageType) {
  case INPUT_ERROR:
    if (State) {
      gST->ConOut->SetAttribute (gST->ConOut, ERROR_TEXT);
      PrintStringAt (
        gScreenDimensions.LeftColumn + gPromptBlockWidth,
        gScreenDimensions.BottomRow - 1,
        InputErrorMessage
        );
      InputError = TRUE;
    } else {
      gST->ConOut->SetAttribute (gST->ConOut, FIELD_TEXT_HIGHLIGHT);
      for (Index = 0; Index < (GetStringWidth (InputErrorMessage) - 2) / 2; Index++) {
        PrintAt (gScreenDimensions.LeftColumn + gPromptBlockWidth + Index, gScreenDimensions.BottomRow - 1, L"  ");
      }

      InputError = FALSE;
    }
    break;

  case NV_UPDATE_REQUIRED:
    if ((gClassOfVfr & FORMSET_CLASS_FRONT_PAGE) == 0) {
      if (State) {
        gST->ConOut->SetAttribute (gST->ConOut, INFO_TEXT);
        PrintStringAt (
          gScreenDimensions.LeftColumn + gPromptBlockWidth + gOptionBlockWidth,
          gScreenDimensions.BottomRow - 1,
          NvUpdateMessage
          );
        gResetRequired    = (BOOLEAN) (gResetRequired | ((Flags & EFI_IFR_FLAG_RESET_REQUIRED) == EFI_IFR_FLAG_RESET_REQUIRED));

        gNvUpdateRequired = TRUE;
      } else {
        gST->ConOut->SetAttribute (gST->ConOut, FIELD_TEXT_HIGHLIGHT);
        for (Index = 0; Index < (GetStringWidth (NvUpdateMessage) - 2) / 2; Index++) {
          PrintAt (
            (gScreenDimensions.LeftColumn + gPromptBlockWidth + gOptionBlockWidth + Index),
            gScreenDimensions.BottomRow - 1,
            L"  "
            );
        }

        gNvUpdateRequired = FALSE;
      }
    }
    break;

  case REFRESH_STATUS_BAR:
    if (InputError) {
      UpdateStatusBar (INPUT_ERROR, Flags, TRUE);
    }

    if (gNvUpdateRequired) {
      UpdateStatusBar (NV_UPDATE_REQUIRED, Flags, TRUE);
    }
    break;

  default:
    break;
  }

  gBS->FreePool (InputErrorMessage);
  gBS->FreePool (NvUpdateMessage);
  return ;
}

UINT16
GetWidth (
  IN FORM_BROWSER_STATEMENT        *Statement,
  IN EFI_HII_HANDLE                 Handle
  )
/*++

Routine Description:
  Get the supported width for a particular op-code

Arguments:
  Statement - The FORM_BROWSER_STATEMENT structure passed in.
  Handle    - The handle in the HII database being used

Returns:
  Returns the number of CHAR16 characters that is support.

--*/
{
  CHAR16  *String;
  UINTN   Size;
  UINT16  Width;

  Size = 0;

  //
  // See if the second text parameter is really NULL
  //
  if ((Statement->Operand == EFI_IFR_TEXT_OP) && (Statement->TextTwo != 0)) {
    String = GetToken (Statement->TextTwo, Handle);
    Size   = EfiStrLen (String);
    gBS->FreePool (String);
  }

  if ((Statement->Operand == EFI_IFR_SUBTITLE_OP) ||
      (Statement->Operand == EFI_IFR_REF_OP) ||
      (Statement->Operand == EFI_IFR_PASSWORD_OP) ||
      (Statement->Operand == EFI_IFR_ACTION_OP) ||
      (Statement->Operand == EFI_IFR_RESET_BUTTON_OP) ||
      //
      // Allow a wide display if text op-code and no secondary text op-code
      //
      ((Statement->Operand == EFI_IFR_TEXT_OP) && (Size == 0))
      ) {
    Width = (UINT16) (gPromptBlockWidth + gOptionBlockWidth);
  } else {
    Width = (UINT16) gPromptBlockWidth;
  }

  if (Statement->InSubtitle) {
    Width -= SUBTITLE_INDENT;
  }

  return Width;
}

UINT16
GetLineByWidth (
  IN      CHAR16                      *InputString,
  IN      UINT16                      LineWidth,
  IN OUT  UINTN                       *Index,
  OUT     CHAR16                      **OutputString
  )
/*++

Routine Description:
  Will copy LineWidth amount of a string in the OutputString buffer and return the
  number of CHAR16 characters that were copied into the OutputString buffer.

Arguments:
  InputString - String description for this option.
  LineWidth - Width of the desired string to extract in CHAR16 characters
  Index - Where in InputString to start the copy process
  OutputString - Buffer to copy the string into

Returns:
  Returns the number of CHAR16 characters that were copied into the OutputString buffer.

--*/
{
  static BOOLEAN  Finished;
  UINT16          Count;
  UINT16          Count2;

  if (Finished) {
    Finished = FALSE;
    return (UINT16) 0;
  }

  Count         = LineWidth;
  Count2        = 0;

  *OutputString = EfiLibAllocateZeroPool (((UINTN) (LineWidth + 1) * 2));

  //
  // Ensure we have got a valid buffer
  //
  if (*OutputString != NULL) {

    //
    //NARROW_CHAR can not be printed in screen, so if a line only contain  the two CHARs: 'NARROW_CHAR + CHAR_CARRIAGE_RETURN' , it is a empty line  in Screen.
    //To avoid displaying this  empty line in screen,  just skip  the two CHARs here.
    //
   if ((InputString[*Index] == NARROW_CHAR) && (InputString[*Index + 1] == CHAR_CARRIAGE_RETURN)) {
     *Index = *Index + 2;
   }

    //
    // Fast-forward the string and see if there is a carriage-return in the string
    //
    for (; (InputString[*Index + Count2] != CHAR_CARRIAGE_RETURN) && (Count2 != LineWidth); Count2++)
      ;

    //
    // Copy the desired LineWidth of data to the output buffer.
    // Also make sure that we don't copy more than the string.
    // Also make sure that if there are linefeeds, we account for them.
    //
    if ((EfiStrSize (&InputString[*Index]) <= ((UINTN) (LineWidth + 1) * 2)) &&
        (EfiStrSize (&InputString[*Index]) <= ((UINTN) (Count2 + 1) * 2))
        ) {
      //
      // Convert to CHAR16 value and show that we are done with this operation
      //
      LineWidth = (UINT16) ((EfiStrSize (&InputString[*Index]) - 2) / 2);
      if (LineWidth != 0) {
        Finished = TRUE;
      }
    } else {
      if (Count2 == LineWidth) {
        //
        // Rewind the string from the maximum size until we see a space to break the line
        //
        for (; (InputString[*Index + LineWidth] != CHAR_SPACE) && (LineWidth != 0); LineWidth--)
          ;
        if (LineWidth == 0) {
          LineWidth = Count;
        }
      } else {
        LineWidth = Count2;
      }
    }

    EfiCopyMem (*OutputString, &InputString[*Index], LineWidth * 2);

    //
    // If currently pointing to a space, increment the index to the first non-space character
    //
    for (;
         (InputString[*Index + LineWidth] == CHAR_SPACE) || (InputString[*Index + LineWidth] == CHAR_CARRIAGE_RETURN);
         (*Index)++
        )
      ;
    *Index = (UINT16) (*Index + LineWidth);
    return LineWidth;
  } else {
    return (UINT16) 0;
  }
}

VOID
UpdateOptionSkipLines (
  IN UI_MENU_SELECTION            *Selection,
  IN UI_MENU_OPTION               *MenuOption,
  IN CHAR16                       **OptionalString,
  IN UINTN                        SkipValue
  )
/*++

Routine Description:
  Update display lines for a Menu Option.

Arguments:
  MenuOption - The MenuOption to be checked.

Returns:
  TRUE   - This Menu Option is selectable.
  FALSE  - This Menu Option could not be selected.

--*/
{
  UINTN   Index;
  UINT16  Width;
  UINTN   Row;
  UINTN   OriginalRow;
  CHAR16  *OutputString;
  CHAR16  *OptionString;

  Row           = 0;
  OptionString  = *OptionalString;
  OutputString  = NULL;

  ProcessOptions (Selection, MenuOption, FALSE, &OptionString);

  if (OptionString != NULL) {
    Width               = (UINT16) gOptionBlockWidth;

    OriginalRow         = Row;

    for (Index = 0; GetLineByWidth (OptionString, Width, &Index, &OutputString) != 0x0000;) {
      //
      // If there is more string to process print on the next row and increment the Skip value
      //
      if (EfiStrLen (&OptionString[Index])) {
        if (SkipValue == 0) {
          Row++;
          //
          // Since the Number of lines for this menu entry may or may not be reflected accurately
          // since the prompt might be 1 lines and option might be many, and vice versa, we need to do
          // some testing to ensure we are keeping this in-sync.
          //
          // If the difference in rows is greater than or equal to the skip value, increase the skip value
          //
          if ((Row - OriginalRow) >= MenuOption->Skip) {
            MenuOption->Skip++;
          }
        }
      }

      gBS->FreePool (OutputString);
      if (SkipValue != 0) {
        SkipValue--;
      }
    }

    Row = OriginalRow;
  }

  *OptionalString = OptionString;
}

STATIC
BOOLEAN
IsSelectable (
  UI_MENU_OPTION   *MenuOption
  )
/*++

Routine Description:
  Check whether this Menu Option could be highlighted.

Arguments:
  MenuOption - The MenuOption to be checked.

Returns:
  TRUE   - This Menu Option is selectable.
  FALSE  - This Menu Option could not be selected.

--*/
{
  if ((MenuOption->ThisTag->Operand == EFI_IFR_SUBTITLE_OP) ||
      MenuOption->GrayOut) {
    return FALSE;
  } else {
    return TRUE;
  }
}

STATIC
BOOLEAN
ValueIsScroll (
  IN  BOOLEAN                     Direction,
  IN  EFI_LIST_ENTRY              *CurrentPos
  )
/*++

Routine Description:
  Determine if the menu is the last menu that can be selected.

Arguments:
  Direction - the scroll direction. False is down. True is up.

Returns:
  FALSE -- the menu isn't the last menu that can be selected.
  TRUE  -- the menu is the last menu that can be selected.
--*/
{
  EFI_LIST_ENTRY  *Temp;
  UI_MENU_OPTION  *MenuOption;

  Temp = Direction ? CurrentPos->BackLink : CurrentPos->ForwardLink;

  if (Temp == &Menu) {
    return TRUE;
  }

  for (; Temp != &Menu; Temp = Direction ? Temp->BackLink : Temp->ForwardLink) {
    MenuOption = MENU_OPTION_FROM_LINK (Temp);
    if (IsSelectable (MenuOption)) {
      return FALSE;
    }
  }

  return TRUE;
}

STATIC
INTN
MoveToNextStatement (
  IN     BOOLEAN                   GoUp,
  IN OUT EFI_LIST_ENTRY            **CurrentPosition
  )
/*++

Routine Description:
  Move to next selectable statement.

Arguments:
  GoUp             - The navigation direction. TRUE: up, FALSE: down.
  CurrentPosition  - Current position.

Returns:
  The row distance from current MenuOption to next selectable MenuOption.

--*/
{
  INTN             Distance;
  EFI_LIST_ENTRY   *Pos;
  BOOLEAN          HitEnd;
  UI_MENU_OPTION   *NextMenuOption;

  Distance = 0;
  Pos      = *CurrentPosition;
  HitEnd   = FALSE;

  while (TRUE) {
    NextMenuOption = MENU_OPTION_FROM_LINK (Pos);
    if (IsSelectable (NextMenuOption)) {
      break;
    }
    if ((GoUp ? Pos->BackLink : Pos->ForwardLink) == &Menu) {
      HitEnd = TRUE;
      break;
    }
    Distance += NextMenuOption->Skip;
    Pos = (GoUp ? Pos->BackLink : Pos->ForwardLink);
  }

  if (HitEnd) {
    //
    // If we hit end there is still no statement can be focused,
    // we go backwards to find the statement can be focused.
    //
    Distance = 0;
    Pos = *CurrentPosition;

    while (TRUE) {
      NextMenuOption = MENU_OPTION_FROM_LINK (Pos);
      if (IsSelectable (NextMenuOption)) {
        break;
      }
      if ((!GoUp ? Pos->BackLink : Pos->ForwardLink) == &Menu) {
        ASSERT (FALSE);
        break;
      }
      Distance -= NextMenuOption->Skip;
      Pos = (!GoUp ? Pos->BackLink : Pos->ForwardLink);
    }
  }

  *CurrentPosition = &NextMenuOption->Link;
  return Distance;
}

STATIC
UINTN
AdjustDateAndTimePosition (
  IN     BOOLEAN                     DirectionUp,
  IN OUT EFI_LIST_ENTRY              **CurrentPosition
  )
/*++
Routine Description:
  Adjust Data and Time position accordingly.
  Data format :      [01/02/2004]      [11:22:33]
  Line number :        0  0    1         0  0  1

Arguments:
  DirectionUp     - the up or down direction. False is down. True is up.
  CurrentPosition - Current position.
                    On return: Point to the last Option (Year or Second) if up;
                               Point to the first Option (Month or Hour) if down.

Returns:
  Return line number to pad. It is possible that we stand on a zero-advance
  data or time opcode, so pad one line when we judge if we are going to scroll outside.
--*/
{
  UINTN           Count;
  EFI_LIST_ENTRY  *NewPosition;
  UI_MENU_OPTION  *MenuOption;
  UINTN           PadLineNumber;

  PadLineNumber = 0;
  NewPosition   = *CurrentPosition;
  MenuOption    = MENU_OPTION_FROM_LINK (NewPosition);

  if ((MenuOption->ThisTag->Operand == EFI_IFR_DATE_OP) ||
      (MenuOption->ThisTag->Operand == EFI_IFR_TIME_OP)) {
    //
    // Calculate the distance from current position to the last Date/Time MenuOption
    //
    Count = 0;
    while (MenuOption->Skip == 0) {
      Count++;
      NewPosition   = NewPosition->ForwardLink;
      MenuOption    = MENU_OPTION_FROM_LINK (NewPosition);
      PadLineNumber = 1;
    }

    NewPosition = *CurrentPosition;
    if (DirectionUp) {
      //
      // Since the behavior of hitting the up arrow on a Date/Time MenuOption is intended
      // to be one that back to the previous set of MenuOptions, we need to advance to the first
      // Date/Time MenuOption and leave the remaining logic in CfUiUp intact so the appropriate
      // checking can be done.
      //
      while (Count++ < 2) {
        NewPosition = NewPosition->BackLink;
      }
    } else {
      //
      // Since the behavior of hitting the down arrow on a Date/Time MenuOption is intended
      // to be one that progresses to the next set of MenuOptions, we need to advance to the last
      // Date/Time MenuOption and leave the remaining logic in CfUiDown intact so the appropriate
      // checking can be done.
      //
      while (Count-- > 0) {
        NewPosition = NewPosition->ForwardLink;
      }
    }

    *CurrentPosition = NewPosition;
  }

  return PadLineNumber;
}

EFI_STATUS
UiDisplayMenu (
  IN OUT UI_MENU_SELECTION           *Selection
  )
/*++

Routine Description:
  Display menu and wait for user to select one menu option, then return it.
  If AutoBoot is enabled, then if user doesn't select any option,
  after period of time, it will automatically return the first menu option.

Arguments:

Returns:
  Return the pointer of the menu which selected,
  otherwise return NULL.

--*/
{
  INTN                            SkipValue;
  INTN                            Difference;
  INTN                            OldSkipValue;
  UINTN                           DistanceValue;
  UINTN                           Row;
  UINTN                           Col;
  UINTN                           Temp;
  UINTN                           Temp2;
  UINTN                           TopRow;
  UINTN                           BottomRow;
  UINTN                           OriginalRow;
  UINTN                           Index;
  UINT32                          Count;
  UINT16                          Width;
  CHAR16                          *StringPtr;
  CHAR16                          *OptionString;
  CHAR16                          *OutputString;
  CHAR16                          *FormattedString;
  CHAR16                          YesResponse;
  CHAR16                          NoResponse;
  BOOLEAN                         NewLine;
  BOOLEAN                         Repaint;
  BOOLEAN                         SavedValue;
  EFI_STATUS                      Status;
  EFI_INPUT_KEY                   Key;
  EFI_LIST_ENTRY                  *Link;
  EFI_LIST_ENTRY                  *NewPos;
  EFI_LIST_ENTRY                  *TopOfScreen;
  EFI_LIST_ENTRY                  *SavedListEntry;
  UI_MENU_OPTION                  *MenuOption;
  UI_MENU_OPTION                  *NextMenuOption;
  UI_MENU_OPTION                  *SavedMenuOption;
  UI_MENU_OPTION                  *PreviousMenuOption;
  UI_CONTROL_FLAG                 ControlFlag;
  EFI_SCREEN_DESCRIPTOR           LocalScreen;
  MENU_REFRESH_ENTRY              *MenuRefreshEntry;
  UI_SCREEN_OPERATION             ScreenOperation;
  UINT8                           MinRefreshInterval;
  UINTN                           BufferSize;
  UINT16                          DefaultId;
  EFI_DEVICE_PATH_PROTOCOL        *DevicePath;
  FORM_BROWSER_STATEMENT          *Statement;
  UI_MENU_LIST                    *CurrentMenu;
  UI_MENU_LIST                    *MenuList;

  EfiCopyMem (&LocalScreen, &gScreenDimensions, sizeof (EFI_SCREEN_DESCRIPTOR));

  Status              = EFI_SUCCESS;
  FormattedString     = NULL;
  OptionString        = NULL;
  ScreenOperation     = UiNoOperation;
  NewLine             = TRUE;
  MinRefreshInterval  = 0;
  DefaultId           = 0;

  OutputString        = NULL;
  gUpArrow            = FALSE;
  gDownArrow          = FALSE;
  SkipValue           = 0;
  OldSkipValue        = 0;
  MenuRefreshEntry    = gMenuRefreshHead;

  NextMenuOption      = NULL;
  PreviousMenuOption  = NULL;
  SavedMenuOption     = NULL;

  EfiZeroMem (&Key, sizeof (EFI_INPUT_KEY));

  if ((gClassOfVfr & FORMSET_CLASS_FRONT_PAGE) != 0) {
    TopRow  = LocalScreen.TopRow + FRONT_PAGE_HEADER_HEIGHT + SCROLL_ARROW_HEIGHT;
    Row     = LocalScreen.TopRow + FRONT_PAGE_HEADER_HEIGHT + SCROLL_ARROW_HEIGHT;
  } else {
    TopRow  = LocalScreen.TopRow + NONE_FRONT_PAGE_HEADER_HEIGHT + SCROLL_ARROW_HEIGHT;
    Row     = LocalScreen.TopRow + NONE_FRONT_PAGE_HEADER_HEIGHT + SCROLL_ARROW_HEIGHT;
  }

  Col = LocalScreen.LeftColumn;
  BottomRow = LocalScreen.BottomRow - STATUS_BAR_HEIGHT - FOOTER_HEIGHT - SCROLL_ARROW_HEIGHT - 1;

  Selection->TopRow = TopRow;
  Selection->BottomRow = BottomRow;
  Selection->PromptCol = Col;
  Selection->OptionCol = gPromptBlockWidth + 1 + LocalScreen.LeftColumn;
  Selection->Statement = NULL;

  TopOfScreen = Menu.ForwardLink;
  Repaint     = TRUE;
  MenuOption  = NULL;

  //
  // Find current Menu
  //
  CurrentMenu = UiFindMenuList (&Selection->FormSetGuid, Selection->FormId);
  if (CurrentMenu == NULL) {
    //
    // Current menu not found, add it to the menu tree
    //
    CurrentMenu = UiAddMenuList (NULL, &Selection->FormSetGuid, Selection->FormId);
  }
  ASSERT (CurrentMenu != NULL);

  if (Selection->QuestionId == 0) {
    //
    // Highlight not specified, fetch it from cached menu
    //
    Selection->QuestionId = CurrentMenu->QuestionId;
  }

  //
  // Get user's selection
  //
  NewPos = Menu.ForwardLink;

  gST->ConOut->EnableCursor (gST->ConOut, FALSE);
  UpdateStatusBar (REFRESH_STATUS_BAR, (UINT8) 0, TRUE);

  ControlFlag = CfInitialization;
  Selection->Action = UI_ACTION_NONE;
  while (TRUE) {
    switch (ControlFlag) {
    case CfInitialization:
      if (IsListEmpty (&Menu)) {
        ControlFlag = CfReadKey;
      } else {
        ControlFlag = CfCheckSelection;
      }
      break;

    case CfCheckSelection:
      if (Selection->Action != UI_ACTION_NONE) {
        ControlFlag = CfExit;
      } else {
        ControlFlag = CfRepaint;
      }
      break;

    case CfRepaint:
      ControlFlag = CfRefreshHighLight;

      if (Repaint) {
        //
        // Display menu
        //
        gDownArrow      = FALSE;
        gUpArrow        = FALSE;
        Row             = TopRow;

        Temp            = SkipValue;
        Temp2           = SkipValue;

        ClearLines (
          LocalScreen.LeftColumn,
          LocalScreen.RightColumn,
          TopRow - SCROLL_ARROW_HEIGHT,
          BottomRow + SCROLL_ARROW_HEIGHT,
          FIELD_TEXT | FIELD_BACKGROUND
          );

        UiFreeRefreshList ();
        MinRefreshInterval = 0;

        for (Link = TopOfScreen; Link != &Menu; Link = Link->ForwardLink) {
          MenuOption          = MENU_OPTION_FROM_LINK (Link);
          MenuOption->Row     = Row;
          MenuOption->Col     = Col;
          MenuOption->OptCol  = gPromptBlockWidth + 1 + LocalScreen.LeftColumn;

          Statement = MenuOption->ThisTag;
          if (Statement->InSubtitle) {
            MenuOption->Col += SUBTITLE_INDENT;
          }

          if (MenuOption->GrayOut) {
            gST->ConOut->SetAttribute (gST->ConOut, FIELD_TEXT_GRAYED | FIELD_BACKGROUND);
          } else {
            if (Statement->Operand == EFI_IFR_SUBTITLE_OP) {
              gST->ConOut->SetAttribute (gST->ConOut, SUBTITLE_TEXT | FIELD_BACKGROUND);
            }
          }

          Width       = GetWidth (Statement, MenuOption->Handle);
          OriginalRow = Row;

          for (Index = 0; GetLineByWidth (MenuOption->Description, Width, &Index, &OutputString) != 0x0000;) {
            if ((Temp == 0) && (Row <= BottomRow)) {
              PrintStringAt (MenuOption->Col, Row, OutputString);
            }
            //
            // If there is more string to process print on the next row and increment the Skip value
            //
            if (EfiStrLen (&MenuOption->Description[Index])) {
              if (Temp == 0) {
                Row++;
              }
            }

            gBS->FreePool (OutputString);
            if (Temp != 0) {
              Temp--;
            }
          }

          Temp  = 0;
          Row   = OriginalRow;

          gST->ConOut->SetAttribute (gST->ConOut, FIELD_TEXT | FIELD_BACKGROUND);
          Status = ProcessOptions (Selection, MenuOption, FALSE, &OptionString);
          if (EFI_ERROR (Status)) {
            //
            // Repaint to clear possible error prompt pop-up
            //
            Repaint = TRUE;
            NewLine = TRUE;
            ControlFlag = CfRepaint;
            break;
          }

          if (OptionString != NULL) {
            if (Statement->Operand == EFI_IFR_DATE_OP || Statement->Operand == EFI_IFR_TIME_OP) {
              //
              // If leading spaces on OptionString - remove the spaces
              //
              for (Index = 0; OptionString[Index] == L' '; Index++) {
                MenuOption->OptCol++;
              }

              for (Count = 0; OptionString[Index] != CHAR_NULL; Index++) {
                OptionString[Count] = OptionString[Index];
                Count++;
              }

              OptionString[Count] = CHAR_NULL;
            }

            Width       = (UINT16) gOptionBlockWidth;
            OriginalRow = Row;

            for (Index = 0; GetLineByWidth (OptionString, Width, &Index, &OutputString) != 0x0000;) {
              if ((Temp2 == 0) && (Row <= BottomRow)) {
                PrintStringAt (MenuOption->OptCol, Row, OutputString);
              }
              //
              // If there is more string to process print on the next row and increment the Skip value
              //
              if (EfiStrLen (&OptionString[Index])) {
                if (Temp2 == 0) {
                  Row++;
                  //
                  // Since the Number of lines for this menu entry may or may not be reflected accurately
                  // since the prompt might be 1 lines and option might be many, and vice versa, we need to do
                  // some testing to ensure we are keeping this in-sync.
                  //
                  // If the difference in rows is greater than or equal to the skip value, increase the skip value
                  //
                  if ((Row - OriginalRow) >= MenuOption->Skip) {
                    MenuOption->Skip++;
                  }
                }
              }

              gBS->FreePool (OutputString);
              if (Temp2 != 0) {
                Temp2--;
              }
            }

            Temp2 = 0;
            Row   = OriginalRow;

            gBS->FreePool (OptionString);
          }

          //
          // If Question request refresh, register the op-code
          //
          if (Statement->RefreshInterval != 0) {
            //
            // Menu will be refreshed at minimal interval of all Questions
            // which have refresh request
            //
            if (MinRefreshInterval == 0 || Statement->RefreshInterval < MinRefreshInterval) {
              MinRefreshInterval = Statement->RefreshInterval;
            }

            if (gMenuRefreshHead == NULL) {
              MenuRefreshEntry = EfiLibAllocateZeroPool (sizeof (MENU_REFRESH_ENTRY));
              ASSERT (MenuRefreshEntry != NULL);
              MenuRefreshEntry->MenuOption        = MenuOption;
              MenuRefreshEntry->Selection         = Selection;
              MenuRefreshEntry->CurrentColumn     = MenuOption->OptCol;
              MenuRefreshEntry->CurrentRow        = MenuOption->Row;
              MenuRefreshEntry->CurrentAttribute  = FIELD_TEXT | FIELD_BACKGROUND;
              gMenuRefreshHead                    = MenuRefreshEntry;
            } else {
              //
              // Advance to the last entry
              //
              for (MenuRefreshEntry = gMenuRefreshHead;
                   MenuRefreshEntry->Next != NULL;
                   MenuRefreshEntry = MenuRefreshEntry->Next
                  )
                ;
              MenuRefreshEntry->Next = EfiLibAllocateZeroPool (sizeof (MENU_REFRESH_ENTRY));
              ASSERT (MenuRefreshEntry->Next != NULL);
              MenuRefreshEntry                    = MenuRefreshEntry->Next;
              MenuRefreshEntry->MenuOption        = MenuOption;
              MenuRefreshEntry->Selection         = Selection;
              MenuRefreshEntry->CurrentColumn     = MenuOption->OptCol;
              MenuRefreshEntry->CurrentRow        = MenuOption->Row;
              MenuRefreshEntry->CurrentAttribute  = FIELD_TEXT | FIELD_BACKGROUND;
            }
          }

          //
          // If this is a text op with secondary text information
          //
          if ((Statement->Operand == EFI_IFR_TEXT_OP) && (Statement->TextTwo != 0)) {
            StringPtr   = GetToken (Statement->TextTwo, MenuOption->Handle);

            Width       = (UINT16) gOptionBlockWidth;
            OriginalRow = Row;

            for (Index = 0; GetLineByWidth (StringPtr, Width, &Index, &OutputString) != 0x0000;) {
              if ((Temp == 0) && (Row <= BottomRow)) {
                PrintStringAt (MenuOption->OptCol, Row, OutputString);
              }
              //
              // If there is more string to process print on the next row and increment the Skip value
              //
              if (EfiStrLen (&StringPtr[Index])) {
                if (Temp2 == 0) {
                  Row++;
                  //
                  // Since the Number of lines for this menu entry may or may not be reflected accurately
                  // since the prompt might be 1 lines and option might be many, and vice versa, we need to do
                  // some testing to ensure we are keeping this in-sync.
                  //
                  // If the difference in rows is greater than or equal to the skip value, increase the skip value
                  //
                  if ((Row - OriginalRow) >= MenuOption->Skip) {
                    MenuOption->Skip++;
                  }
                }
              }

              gBS->FreePool (OutputString);
              if (Temp2 != 0) {
                Temp2--;
              }
            }

            Row = OriginalRow;
            gBS->FreePool (StringPtr);
          }

          //
          // Need to handle the bottom of the display
          //
          if (MenuOption->Skip > 1) {
            Row += MenuOption->Skip - SkipValue;
            SkipValue = 0;
          } else {
            Row += MenuOption->Skip;
          }

          if (Row > BottomRow) {
            if (!ValueIsScroll (FALSE, Link)) {
              gDownArrow = TRUE;
            }

            Row = BottomRow + 1;
            break;
          }
        }

        if (!ValueIsScroll (TRUE, TopOfScreen)) {
          gUpArrow = TRUE;
        }

        if (gUpArrow) {
          gST->ConOut->SetAttribute (gST->ConOut, ARROW_TEXT | ARROW_BACKGROUND);
          PrintAt (
            LocalScreen.LeftColumn + gPromptBlockWidth + gOptionBlockWidth + 1,
            TopRow - SCROLL_ARROW_HEIGHT,
            L"%c",
            ARROW_UP
            );
          gST->ConOut->SetAttribute (gST->ConOut, FIELD_TEXT | FIELD_BACKGROUND);
        }

        if (gDownArrow) {
          gST->ConOut->SetAttribute (gST->ConOut, ARROW_TEXT | ARROW_BACKGROUND);
          PrintAt (
            LocalScreen.LeftColumn + gPromptBlockWidth + gOptionBlockWidth + 1,
            BottomRow + SCROLL_ARROW_HEIGHT,
            L"%c",
            ARROW_DOWN
            );
          gST->ConOut->SetAttribute (gST->ConOut, FIELD_TEXT | FIELD_BACKGROUND);
        }

        MenuOption = NULL;
      }
      break;

    case CfRefreshHighLight:
      //
      // MenuOption: Last menu option that need to remove hilight
      //             MenuOption is set to NULL in Repaint
      // NewPos:     Current menu option that need to hilight
      //
      ControlFlag = CfUpdateHelpString;

      //
      // Repaint flag is normally reset when finish processing CfUpdateHelpString. Temporarily
      // reset Repaint flag because we may break halfway and skip CfUpdateHelpString processing.
      //
      SavedValue  = Repaint;
      Repaint     = FALSE;

      if (Selection->QuestionId != 0) {
        NewPos = Menu.ForwardLink;
        SavedMenuOption = MENU_OPTION_FROM_LINK (NewPos);

        while (SavedMenuOption->ThisTag->QuestionId != Selection->QuestionId && NewPos->ForwardLink != &Menu) {
          NewPos     = NewPos->ForwardLink;
          SavedMenuOption = MENU_OPTION_FROM_LINK (NewPos);
        }
        if (SavedMenuOption->ThisTag->QuestionId == Selection->QuestionId) {
          //
          // Target Question found, find its MenuOption
          //
          Link = TopOfScreen;

          for (Index = TopRow; Index <= BottomRow && Link != NewPos;) {
            SavedMenuOption = MENU_OPTION_FROM_LINK (Link);
            Index += SavedMenuOption->Skip;
            Link = Link->ForwardLink;
          }

          if (Link != NewPos || Index > BottomRow) {
            //
            // NewPos is not in the current page, simply scroll page so that NewPos is in the end of the page
            //
            Link    = NewPos;
            for (Index = TopRow; Index <= BottomRow; ) {
              Link = Link->BackLink;
              SavedMenuOption = MENU_OPTION_FROM_LINK (Link);
              Index     += SavedMenuOption->Skip;
            }
            TopOfScreen     = Link->ForwardLink;

            Repaint = TRUE;
            NewLine = TRUE;
            ControlFlag = CfRepaint;
            break;
          }
        } else {
          //
          // Target Question not found, highlight the default menu option
          //
          NewPos = TopOfScreen;
        }

        Selection->QuestionId = 0;
      }

      if (NewPos != NULL && (MenuOption == NULL || NewPos != &MenuOption->Link)) {
        if (MenuOption != NULL) {
          //
          // Remove highlight on last Menu Option
          //
          gST->ConOut->SetCursorPosition (gST->ConOut, MenuOption->Col, MenuOption->Row);
          ProcessOptions (Selection, MenuOption, FALSE, &OptionString);
          gST->ConOut->SetAttribute (gST->ConOut, FIELD_TEXT | FIELD_BACKGROUND);
          if ((OptionString != NULL) && (!MenuOption->ReadOnly)) {
            if ((MenuOption->ThisTag->Operand == EFI_IFR_DATE_OP) ||
                (MenuOption->ThisTag->Operand == EFI_IFR_TIME_OP)
               ) {
              //
              // If leading spaces on OptionString - remove the spaces
              //
              for (Index = 0; OptionString[Index] == L' '; Index++)
                ;

              for (Count = 0; OptionString[Index] != CHAR_NULL; Index++) {
                OptionString[Count] = OptionString[Index];
                Count++;
              }

              OptionString[Count] = CHAR_NULL;
            }

            Width               = (UINT16) gOptionBlockWidth;
            OriginalRow         = MenuOption->Row;

            for (Index = 0; GetLineByWidth (OptionString, Width, &Index, &OutputString) != 0x0000;) {
              if (MenuOption->Row >= TopRow && MenuOption->Row <= BottomRow) {
                PrintStringAt (MenuOption->OptCol, MenuOption->Row, OutputString);
              }
              //
              // If there is more string to process print on the next row and increment the Skip value
              //
              if (EfiStrLen (&OptionString[Index])) {
                MenuOption->Row++;
              }

              gBS->FreePool (OutputString);
            }

            MenuOption->Row = OriginalRow;

            gBS->FreePool (OptionString);
          } else {
            if (NewLine) {
              if (MenuOption->GrayOut) {
                gST->ConOut->SetAttribute (gST->ConOut, FIELD_TEXT_GRAYED | FIELD_BACKGROUND);
              } else if (MenuOption->ThisTag->Operand == EFI_IFR_SUBTITLE_OP) {
                gST->ConOut->SetAttribute (gST->ConOut, SUBTITLE_TEXT | FIELD_BACKGROUND);
              }

              OriginalRow = MenuOption->Row;
              Width       = GetWidth (MenuOption->ThisTag, MenuOption->Handle);

              for (Index = 0; GetLineByWidth (MenuOption->Description, Width, &Index, &OutputString) != 0x0000;) {
                if (MenuOption->Row >= TopRow && MenuOption->Row <= BottomRow) {
                  PrintStringAt (MenuOption->Col, MenuOption->Row, OutputString);
                }
                //
                // If there is more string to process print on the next row and increment the Skip value
                //
                if (EfiStrLen (&MenuOption->Description[Index])) {
                  MenuOption->Row++;
                }

                gBS->FreePool (OutputString);
              }

              MenuOption->Row = OriginalRow;
              gST->ConOut->SetAttribute (gST->ConOut, FIELD_TEXT | FIELD_BACKGROUND);
            }
          }
        }

        //
        // This is only possible if we entered this page and the first menu option is
        // a "non-menu" item.  In that case, force it UiDown
        //
        MenuOption = MENU_OPTION_FROM_LINK (NewPos);
        if (!IsSelectable (MenuOption)) {
          ASSERT (ScreenOperation == UiNoOperation);
          ScreenOperation = UiDown;
          ControlFlag     = CfScreenOperation;
          break;
        }

        //
        // This is the current selected statement
        //
        Statement = MenuOption->ThisTag;
        Selection->Statement = Statement;
        //
        // Record highlight for current menu
        //
        CurrentMenu->QuestionId = Statement->QuestionId;

        //
        // Set reverse attribute
        //
        gST->ConOut->SetAttribute (gST->ConOut, FIELD_TEXT_HIGHLIGHT | FIELD_BACKGROUND_HIGHLIGHT);
        gST->ConOut->SetCursorPosition (gST->ConOut, MenuOption->Col, MenuOption->Row);

        //
        // Assuming that we have a refresh linked-list created, lets annotate the
        // appropriate entry that we are highlighting with its new attribute.  Just prior to this
        // lets reset all of the entries' attribute so we do not get multiple highlights in he refresh
        //
        if (gMenuRefreshHead != NULL) {
          for (MenuRefreshEntry = gMenuRefreshHead; MenuRefreshEntry != NULL; MenuRefreshEntry = MenuRefreshEntry->Next) {
            MenuRefreshEntry->CurrentAttribute = FIELD_TEXT | FIELD_BACKGROUND;
            if (MenuRefreshEntry->MenuOption == MenuOption) {
              MenuRefreshEntry->CurrentAttribute = FIELD_TEXT_HIGHLIGHT | FIELD_BACKGROUND_HIGHLIGHT;
            }
          }
        }

        ProcessOptions (Selection, MenuOption, FALSE, &OptionString);
        if ((OptionString != NULL) && (!MenuOption->ReadOnly)) {
          if (Statement->Operand == EFI_IFR_DATE_OP || Statement->Operand == EFI_IFR_TIME_OP) {
            //
            // If leading spaces on OptionString - remove the spaces
            //
            for (Index = 0; OptionString[Index] == L' '; Index++)
              ;

            for (Count = 0; OptionString[Index] != CHAR_NULL; Index++) {
              OptionString[Count] = OptionString[Index];
              Count++;
            }

            OptionString[Count] = CHAR_NULL;
          }
          Width               = (UINT16) gOptionBlockWidth;

          OriginalRow         = MenuOption->Row;

          for (Index = 0; GetLineByWidth (OptionString, Width, &Index, &OutputString) != 0x0000;) {
            if (MenuOption->Row >= TopRow && MenuOption->Row <= BottomRow) {
              PrintStringAt (MenuOption->OptCol, MenuOption->Row, OutputString);
            }
            //
            // If there is more string to process print on the next row and increment the Skip value
            //
            if (EfiStrLen (&OptionString[Index])) {
              MenuOption->Row++;
            }

            gBS->FreePool (OutputString);
          }

          MenuOption->Row = OriginalRow;

          gBS->FreePool (OptionString);
        } else {
          if (NewLine) {
            OriginalRow = MenuOption->Row;

            Width       = GetWidth (Statement, MenuOption->Handle);

            for (Index = 0; GetLineByWidth (MenuOption->Description, Width, &Index, &OutputString) != 0x0000;) {
              if (MenuOption->Row >= TopRow && MenuOption->Row <= BottomRow) {
                PrintStringAt (MenuOption->Col, MenuOption->Row, OutputString);
              }
              //
              // If there is more string to process print on the next row and increment the Skip value
              //
              if (EfiStrLen (&MenuOption->Description[Index])) {
                MenuOption->Row++;
              }

              gBS->FreePool (OutputString);
            }

            MenuOption->Row = OriginalRow;

          }
        }

        UpdateKeyHelp (Selection, MenuOption, FALSE);

        //
        // Clear reverse attribute
        //
        gST->ConOut->SetAttribute (gST->ConOut, FIELD_TEXT | FIELD_BACKGROUND);
      }
      //
      // Repaint flag will be used when process CfUpdateHelpString, so restore its value
      // if we didn't break halfway when process CfRefreshHighLight.
      //
      Repaint = SavedValue;
      break;

    case CfUpdateHelpString:
      ControlFlag = CfPrepareToReadKey;

        if (Repaint || NewLine) {
        //
        // Don't print anything if it is a NULL help token
        //
        if (MenuOption->ThisTag->Help == 0) {
          StringPtr = L"\0";
        } else {
          StringPtr = GetToken (MenuOption->ThisTag->Help, MenuOption->Handle);
        }

        ProcessHelpString (StringPtr, &FormattedString, BottomRow - TopRow);

        gST->ConOut->SetAttribute (gST->ConOut, HELP_TEXT | FIELD_BACKGROUND);

        for (Index = 0; Index < BottomRow - TopRow; Index++) {
          //
          // Pad String with spaces to simulate a clearing of the previous line
          //
          for (; GetStringWidth (&FormattedString[Index * gHelpBlockWidth * 2]) / 2 < gHelpBlockWidth;) {
            EfiStrCat (&FormattedString[Index * gHelpBlockWidth * 2], L" ");
          }

          PrintStringAt (
            LocalScreen.RightColumn - gHelpBlockWidth,
            Index + TopRow,
            &FormattedString[Index * gHelpBlockWidth * 2]
            );
        }
      }
      //
      // Reset this flag every time we finish using it.
      //
      Repaint = FALSE;
      NewLine = FALSE;
      break;

    case CfPrepareToReadKey:
      ControlFlag = CfReadKey;
      ScreenOperation = UiNoOperation;
      break;

    case CfReadKey:
      ControlFlag = CfScreenOperation;

      //
      // Wait for user's selection
      //
      do {
        Status = UiWaitForSingleEvent (gST->ConIn->WaitForKey, 0, MinRefreshInterval);
      } while (Status == EFI_TIMEOUT);

      if (Selection->Action == UI_ACTION_REFRESH_FORMSET) {
        //
        // IFR is updated in Callback of refresh opcode, re-parse it
        //
        Selection->Statement = NULL;
        return EFI_SUCCESS;
      }

      Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);
      //
      // If we encounter error, continue to read another key in.
      //
      if (EFI_ERROR (Status)) {
        ControlFlag = CfReadKey;
        break;
      }

      switch (Key.UnicodeChar) {
      case CHAR_CARRIAGE_RETURN:
        ScreenOperation = UiSelect;
        gDirection      = 0;
        break;

      //
      // We will push the adjustment of these numeric values directly to the input handler
      //  NOTE: we won't handle manual input numeric
      //
      case '+':
      case '-':
        Statement = MenuOption->ThisTag;
        if ((Statement->Operand == EFI_IFR_DATE_OP)
          || (Statement->Operand == EFI_IFR_TIME_OP)
          || ((Statement->Operand == EFI_IFR_NUMERIC_OP) && (Statement->Step != 0))
        ){
          if (Key.UnicodeChar == '+') {
            gDirection = SCAN_RIGHT;
          } else {
            gDirection = SCAN_LEFT;
          }
          Status = ProcessOptions (Selection, MenuOption, TRUE, &OptionString);
          if (EFI_ERROR (Status)) {
            //
            // Repaint to clear possible error prompt pop-up
            //
            Repaint = TRUE;
            NewLine = TRUE;
          }
          EfiLibSafeFreePool (OptionString);
        }
        break;

      case '^':
        ScreenOperation = UiUp;
        break;

      case 'V':
      case 'v':
        ScreenOperation = UiDown;
        break;

      case ' ':
        if ((gClassOfVfr & FORMSET_CLASS_FRONT_PAGE) == 0) {
          if (MenuOption->ThisTag->Operand == EFI_IFR_CHECKBOX_OP && !MenuOption->GrayOut) {
            ScreenOperation = UiSelect;
          }
        }
        break;

      case CHAR_NULL:
        if (((Key.ScanCode == SCAN_F9) && ((gFunctionKeySetting & FUNCTION_NINE) != FUNCTION_NINE)) ||
            ((Key.ScanCode == SCAN_F10) && ((gFunctionKeySetting & FUNCTION_TEN) != FUNCTION_TEN))
            ) {
          //
          // If the function key has been disabled, just ignore the key.
          //
        } else {
          for (Index = 0; Index < sizeof (gScanCodeToOperation) / sizeof (gScanCodeToOperation[0]); Index++) {
            if (Key.ScanCode == gScanCodeToOperation[Index].ScanCode) {
              if (Key.ScanCode == SCAN_F9) {
                //
                // Reset to standard default
                //
                DefaultId = EFI_HII_DEFAULT_CLASS_STANDARD;
              }
              ScreenOperation = gScanCodeToOperation[Index].ScreenOperation;
              break;
            }
          }
        }
        break;
      }
      break;

    case CfScreenOperation:
      if (ScreenOperation != UiReset) {
        //
        // If the screen has no menu items, and the user didn't select UiReset
        // ignore the selection and go back to reading keys.
        //
        if (IsListEmpty (&Menu)) {
          ControlFlag = CfReadKey;
          break;
        }
        //
        // if there is nothing logical to place a cursor on, just move on to wait for a key.
        //
        for (Link = Menu.ForwardLink; Link != &Menu; Link = Link->ForwardLink) {
          NextMenuOption = MENU_OPTION_FROM_LINK (Link);
          if (IsSelectable (NextMenuOption)) {
            break;
          }
        }

        if (Link == &Menu) {
          ControlFlag = CfPrepareToReadKey;
          break;
        }
      }

      for (Index = 0;
           Index < sizeof (gScreenOperationToControlFlag) / sizeof (gScreenOperationToControlFlag[0]);
           Index++
          ) {
        if (ScreenOperation == gScreenOperationToControlFlag[Index].ScreenOperation) {
          ControlFlag = gScreenOperationToControlFlag[Index].ControlFlag;
          break;
        }
      }
      break;

    case CfUiSelect:
      ControlFlag = CfCheckSelection;

      Statement = MenuOption->ThisTag;
      if ((Statement->Operand == EFI_IFR_TEXT_OP) ||
          (Statement->Operand == EFI_IFR_DATE_OP) ||
          (Statement->Operand == EFI_IFR_TIME_OP) ||
          (Statement->Operand == EFI_IFR_NUMERIC_OP && Statement->Step != 0) ||
          MenuOption->ReadOnly) {
        break;
      }

      //
      // Keep highlight on current MenuOption
      //
      Selection->QuestionId = Statement->QuestionId;

      switch (Statement->Operand) {
      case EFI_IFR_REF_OP:
        if (Statement->RefDevicePath != 0) {
          //
          // Goto another Hii Package list
          //
          ControlFlag = CfCheckSelection;
          Selection->Action = UI_ACTION_REFRESH_FORMSET;

          StringPtr = GetToken (Statement->RefDevicePath, Selection->FormSet->HiiHandle);
          if (StringPtr == NULL) {
            //
            // No device path string not found, exit
            //
            Selection->Action = UI_ACTION_EXIT;
            Selection->Statement = NULL;
            break;
          }
          BufferSize = EfiStrLen (StringPtr) / 2;
          DevicePath = EfiLibAllocatePool (BufferSize);

          HexStringToBuffer ((UINT8 *) DevicePath, &BufferSize, StringPtr);
          Selection->Handle = DevicePathToHiiHandle (mHiiDatabase, DevicePath);
          if (Selection->Handle == NULL) {
            //
            // If target Hii Handle not found, exit
            //
            Selection->Action = UI_ACTION_EXIT;
            Selection->Statement = NULL;
            break;
          }

          gBS->FreePool (StringPtr);
          gBS->FreePool (DevicePath);

          EfiCopyMem (&Selection->FormSetGuid, &Statement->RefFormSetId, sizeof (EFI_GUID));
          Selection->FormId = Statement->RefFormId;
          Selection->QuestionId = Statement->RefQuestionId;
        } else if (!EfiCompareGuid (&Statement->RefFormSetId, &gZeroGuid)) {
          //
          // Goto another Formset, check for uncommitted data
          //
          ControlFlag = CfCheckSelection;
          Selection->Action = UI_ACTION_REFRESH_FORMSET;

          EfiCopyMem (&Selection->FormSetGuid, &Statement->RefFormSetId, sizeof (EFI_GUID));
          Selection->FormId = Statement->RefFormId;
          Selection->QuestionId = Statement->RefQuestionId;
        } else if (Statement->RefFormId != 0) {
          //
          // Goto another form inside this formset,
          //
          Selection->Action = UI_ACTION_REFRESH_FORM;

          //
          // Link current form so that we can always go back when someone hits the ESC
          //
          MenuList = UiFindMenuList (&Selection->FormSetGuid, Statement->RefFormId);
          if (MenuList == NULL) {
            MenuList = UiAddMenuList (CurrentMenu, &Selection->FormSetGuid, Statement->RefFormId);
          }

          Selection->FormId = Statement->RefFormId;
          Selection->QuestionId = Statement->RefQuestionId;
        } else if (Statement->RefQuestionId != 0) {
          //
          // Goto another Question
          //
          Selection->QuestionId = Statement->RefQuestionId;

          if ((Statement->QuestionFlags & EFI_IFR_FLAG_CALLBACK)) {
            Selection->Action = UI_ACTION_REFRESH_FORM;
          } else {
            Repaint = TRUE;
            NewLine = TRUE;
            break;
          }
        }
        break;

      case EFI_IFR_ACTION_OP:
        //
        // Process the Config string <ConfigResp>
        //
        Status = ProcessQuestionConfig (Selection, Statement);

        if (EFI_ERROR (Status)) {
          break;
        }

        //
        // The action button may change some Question value, so refresh the form
        //
        Selection->Action = UI_ACTION_REFRESH_FORM;
        break;

      case EFI_IFR_RESET_BUTTON_OP:
        //
        // Reset Question to default value specified by DefaultId
        //
        ControlFlag = CfUiDefault;
        DefaultId = Statement->DefaultId;
        break;

      default:
        //
        // Editable Questions: oneof, ordered list, checkbox, numeric, string, password
        //
        UpdateKeyHelp (Selection, MenuOption, TRUE);
        Status = ProcessOptions (Selection, MenuOption, TRUE, &OptionString);

        if (EFI_ERROR (Status)) {
          Repaint = TRUE;
          NewLine = TRUE;
          UpdateKeyHelp (Selection, MenuOption, FALSE);
        } else {
          Selection->Action = UI_ACTION_REFRESH_FORM;
        }

        EfiLibSafeFreePool (OptionString);
        break;
      }
      break;

    case CfUiReset:
      //
      // We come here when someone press ESC
      //
      ControlFlag = CfCheckSelection;

      if (CurrentMenu->Parent != NULL) {
        //
        // we have a parent, so go to the parent menu
        //
        if (EfiCompareGuid (&CurrentMenu->FormSetGuid, &CurrentMenu->Parent->FormSetGuid)) {
          //
          // The parent menu and current menu are in the same formset
          //
          Selection->Action = UI_ACTION_REFRESH_FORM;
        } else {
          Selection->Action = UI_ACTION_REFRESH_FORMSET;
        }
        Selection->Statement = NULL;

        Selection->FormId = CurrentMenu->Parent->FormId;
        Selection->QuestionId = CurrentMenu->Parent->QuestionId;

        //
        // Clear highlight record for this menu
        //
        CurrentMenu->QuestionId = 0;
        break;
      }

      if ((gClassOfVfr & FORMSET_CLASS_FRONT_PAGE) != 0) {
        //
        // We never exit FrontPage, so skip the ESC
        //
        Selection->Action = UI_ACTION_NONE;
        break;
      }

      //
      // We are going to leave current FormSet, so check uncommited data in this FormSet
      //
      if (gNvUpdateRequired) {
        Status      = gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);

        YesResponse = gYesResponse[0];
        NoResponse  = gNoResponse[0];

        //
        // If NV flag is up, prompt user
        //
        do {
          CreateDialog (4, TRUE, 0, NULL, &Key, gEmptyString, gSaveChanges, gAreYouSure, gEmptyString);
        } while
        (
          (Key.ScanCode != SCAN_ESC) &&
          ((Key.UnicodeChar | UPPER_LOWER_CASE_OFFSET) != (NoResponse | UPPER_LOWER_CASE_OFFSET)) &&
          ((Key.UnicodeChar | UPPER_LOWER_CASE_OFFSET) != (YesResponse | UPPER_LOWER_CASE_OFFSET))
        );

        if (Key.ScanCode == SCAN_ESC) {
          //
          // User hits the ESC key
          //
          Repaint = TRUE;
          NewLine = TRUE;

          Selection->Action = UI_ACTION_NONE;
          break;
        }

        //
        // If the user hits the YesResponse key
        //
        if ((Key.UnicodeChar | UPPER_LOWER_CASE_OFFSET) == (YesResponse | UPPER_LOWER_CASE_OFFSET)) {
          Status = SubmitForm (Selection->FormSet, Selection->Form);
        }
      }

      Selection->Action = UI_ACTION_EXIT;
      Selection->Statement = NULL;
      CurrentMenu->QuestionId = 0;

      return EFI_SUCCESS;

    case CfUiLeft:
      ControlFlag = CfCheckSelection;
      if ((MenuOption->ThisTag->Operand == EFI_IFR_DATE_OP) || (MenuOption->ThisTag->Operand == EFI_IFR_TIME_OP)) {
        if (MenuOption->Sequence != 0) {
          //
          // In the middle or tail of the Date/Time op-code set, go left.
          //
          NewPos = NewPos->BackLink;
        }
      }
      break;

    case CfUiRight:
      ControlFlag = CfCheckSelection;
      if ((MenuOption->ThisTag->Operand == EFI_IFR_DATE_OP) || (MenuOption->ThisTag->Operand == EFI_IFR_TIME_OP)) {
        if (MenuOption->Sequence != 2) {
          //
          // In the middle or tail of the Date/Time op-code set, go left.
          //
          NewPos = NewPos->ForwardLink;
        }
      }
      break;

    case CfUiUp:
      ControlFlag = CfCheckSelection;

      SavedListEntry = TopOfScreen;

      if (NewPos->BackLink != &Menu) {
        NewLine = TRUE;
        //
        // Adjust Date/Time position before we advance forward.
        //
        AdjustDateAndTimePosition (TRUE, &NewPos);

        //
        // Caution that we have already rewind to the top, don't go backward in this situation.
        //
        if (NewPos->BackLink != &Menu) {
          NewPos = NewPos->BackLink;
        }

        PreviousMenuOption = MENU_OPTION_FROM_LINK (NewPos);
        DistanceValue = PreviousMenuOption->Skip;

        //
        // Since the behavior of hitting the up arrow on a Date/Time op-code is intended
        // to be one that back to the previous set of op-codes, we need to advance to the sencond
        // Date/Time op-code and leave the remaining logic in UiDown intact so the appropriate
        // checking can be done.
        //
        DistanceValue += AdjustDateAndTimePosition (TRUE, &NewPos);

        //
        // Check the previous menu entry to see if it was a zero-length advance.  If it was,
        // don't worry about a redraw.
        //
        if ((INTN) MenuOption->Row - (INTN) DistanceValue < (INTN) TopRow) {
          Repaint     = TRUE;
          TopOfScreen = NewPos;
        }

        Difference = MoveToNextStatement (TRUE, &NewPos);
        if ((INTN) MenuOption->Row - (INTN) DistanceValue < (INTN) TopRow) {
          if (Difference > 0) {
            //
            // Previous focus MenuOption is above the TopOfScreen, so we need to scroll
            //
            TopOfScreen = NewPos;
            Repaint     = TRUE;
          }
        }
        if (Difference < 0) {
          //
          // We want to goto previous MenuOption, but finally we go down.
          // it means that we hit the begining MenuOption that can be focused
          // so we simply scroll to the top
          //
          if (SavedListEntry != Menu.ForwardLink) {
            TopOfScreen = Menu.ForwardLink;
            Repaint     = TRUE;
          }
        }

        //
        // If we encounter a Date/Time op-code set, rewind to the first op-code of the set.
        //
        AdjustDateAndTimePosition (TRUE, &TopOfScreen);

        UpdateStatusBar (INPUT_ERROR, MenuOption->ThisTag->QuestionFlags, FALSE);
      } else {
        SavedMenuOption = MenuOption;
        MenuOption      = MENU_OPTION_FROM_LINK (NewPos);
        if (!IsSelectable (MenuOption)) {
          //
          // If we are at the end of the list and sitting on a text op, we need to more forward
          //
          ScreenOperation = UiDown;
          ControlFlag     = CfScreenOperation;
          break;
        }

        MenuOption = SavedMenuOption;
      }
      break;

    case CfUiPageUp:
      ControlFlag     = CfCheckSelection;

      if (NewPos->BackLink == &Menu) {
        NewLine = FALSE;
        Repaint = FALSE;
        break;
      }

      NewLine   = TRUE;
      Repaint   = TRUE;
      Link      = TopOfScreen;
      PreviousMenuOption = MENU_OPTION_FROM_LINK (Link);
      Index = BottomRow;
      while ((Index >= TopRow) && (Link->BackLink != &Menu)) {
        Index = Index - PreviousMenuOption->Skip;
        Link = Link->BackLink;
        PreviousMenuOption = MENU_OPTION_FROM_LINK (Link);
      }

      TopOfScreen = Link;
      Difference = MoveToNextStatement (TRUE, &Link);
      if (Difference > 0) {
        //
        // The focus MenuOption is above the TopOfScreen
        //
        TopOfScreen = Link;
      } else if (Difference < 0) {
        //
        // This happens when there is no MenuOption can be focused from
        // Current MenuOption to the first MenuOption
        //
        TopOfScreen = Menu.ForwardLink;
      }
      Index += Difference;
      if (Index < TopRow) {
        MenuOption = NULL;
      }

      if (NewPos == Link) {
        Repaint = FALSE;
        NewLine = FALSE;
      } else {
        NewPos = Link;
      }

      //
      // If we encounter a Date/Time op-code set, rewind to the first op-code of the set.
      // Don't do this when we are already in the first page.
      //
      AdjustDateAndTimePosition (TRUE, &TopOfScreen);
      AdjustDateAndTimePosition (TRUE, &NewPos);
      break;

    case CfUiPageDown:
      ControlFlag     = CfCheckSelection;

      if (NewPos->ForwardLink == &Menu) {
        NewLine = FALSE;
        Repaint = FALSE;
        break;
      }

      NewLine = TRUE;
      Repaint = TRUE;
      Link    = TopOfScreen;
      NextMenuOption = MENU_OPTION_FROM_LINK (Link);
      Index = TopRow;
      while ((Index <= BottomRow) && (Link->ForwardLink != &Menu)) {
        Index = Index + NextMenuOption->Skip;
        Link           = Link->ForwardLink;
        NextMenuOption = MENU_OPTION_FROM_LINK (Link);
      }

      Index += MoveToNextStatement (FALSE, &Link);
      if (Index > BottomRow) {
        //
        // There are more MenuOption needing scrolling
        //
        TopOfScreen = Link;
        MenuOption = NULL;
      }
      if (NewPos == Link && Index <= BottomRow) {
        //
        // Finally we know that NewPos is the last MenuOption can be focused.
        //
        NewLine = FALSE;
        Repaint = FALSE;
      } else {
        NewPos  = Link;
      }

      //
      // If we encounter a Date/Time op-code set, rewind to the first op-code of the set.
      // Don't do this when we are already in the last page.
      //
      AdjustDateAndTimePosition (TRUE, &TopOfScreen);
      AdjustDateAndTimePosition (TRUE, &NewPos);
      break;

    case CfUiDown:
      ControlFlag = CfCheckSelection;
      //
      // Since the behavior of hitting the down arrow on a Date/Time op-code is intended
      // to be one that progresses to the next set of op-codes, we need to advance to the last
      // Date/Time op-code and leave the remaining logic in UiDown intact so the appropriate
      // checking can be done.  The only other logic we need to introduce is that if a Date/Time
      // op-code is the last entry in the menu, we need to rewind back to the first op-code of
      // the Date/Time op-code.
      //
      SavedListEntry = NewPos;
      DistanceValue  = AdjustDateAndTimePosition (FALSE, &NewPos);

      if (NewPos->ForwardLink != &Menu) {
        MenuOption      = MENU_OPTION_FROM_LINK (NewPos);
        NewLine         = TRUE;
        NewPos          = NewPos->ForwardLink;
        NextMenuOption  = MENU_OPTION_FROM_LINK (NewPos);

        DistanceValue  += NextMenuOption->Skip;
        DistanceValue  += MoveToNextStatement (FALSE, &NewPos);
        //
        // An option might be multi-line, so we need to reflect that data in the overall skip value
        //
        UpdateOptionSkipLines (Selection, NextMenuOption, &OptionString, SkipValue);

        Temp = MenuOption->Row + MenuOption->Skip + DistanceValue - 1;
        if ((MenuOption->Row + MenuOption->Skip == BottomRow + 1) &&
            (NextMenuOption->ThisTag->Operand == EFI_IFR_DATE_OP ||
             NextMenuOption->ThisTag->Operand == EFI_IFR_TIME_OP)
            ) {
          Temp ++;
        }

        //
        // If we are going to scroll, update TopOfScreen
        //
        if (Temp > BottomRow) {
          do {
            //
            // Is the current top of screen a zero-advance op-code?
            // If so, keep moving forward till we hit a >0 advance op-code
            //
            SavedMenuOption = MENU_OPTION_FROM_LINK (TopOfScreen);

            //
            // If bottom op-code is more than one line or top op-code is more than one line
            //
            if ((DistanceValue > 1) || (MenuOption->Skip > 1)) {
              //
              // Is the bottom op-code greater than or equal in size to the top op-code?
              //
              if ((Temp - BottomRow) >= (SavedMenuOption->Skip - OldSkipValue)) {
                //
                // Skip the top op-code
                //
                TopOfScreen     = TopOfScreen->ForwardLink;
                Difference      = (Temp - BottomRow) - (SavedMenuOption->Skip - OldSkipValue);

                OldSkipValue    = Difference;

                SavedMenuOption = MENU_OPTION_FROM_LINK (TopOfScreen);

                //
                // If we have a remainder, skip that many more op-codes until we drain the remainder
                //
                for (;
                     Difference >= (INTN) SavedMenuOption->Skip;
                     Difference = Difference - (INTN) SavedMenuOption->Skip
                    ) {
                  //
                  // Since the Difference is greater than or equal to this op-code's skip value, skip it
                  //
                  TopOfScreen     = TopOfScreen->ForwardLink;
                  SavedMenuOption = MENU_OPTION_FROM_LINK (TopOfScreen);
                  if (Difference < (INTN) SavedMenuOption->Skip) {
                    Difference = SavedMenuOption->Skip - Difference - 1;
                    break;
                  } else {
                    if (Difference == (INTN) SavedMenuOption->Skip) {
                      TopOfScreen     = TopOfScreen->ForwardLink;
                      SavedMenuOption = MENU_OPTION_FROM_LINK (TopOfScreen);
                      Difference      = SavedMenuOption->Skip - Difference;
                      break;
                    }
                  }
                }
                //
                // Since we will act on this op-code in the next routine, and increment the
                // SkipValue, set the skips to one less than what is required.
                //
                SkipValue = Difference - 1;

              } else {
                //
                // Since we will act on this op-code in the next routine, and increment the
                // SkipValue, set the skips to one less than what is required.
                //
                SkipValue = OldSkipValue + (Temp - BottomRow) - 1;
              }
            } else {
              if ((OldSkipValue + 1) == (INTN) SavedMenuOption->Skip) {
                TopOfScreen = TopOfScreen->ForwardLink;
                break;
              } else {
                SkipValue = OldSkipValue;
              }
            }
            //
            // If the op-code at the top of the screen is more than one line, let's not skip it yet
            // Let's set a skip flag to smoothly scroll the top of the screen.
            //
            if (SavedMenuOption->Skip > 1) {
              if (SavedMenuOption == NextMenuOption) {
                SkipValue = 0;
              } else {
                SkipValue++;
              }
            } else {
              SkipValue   = 0;
              TopOfScreen = TopOfScreen->ForwardLink;
            }
          } while (SavedMenuOption->Skip == 0);

          Repaint       = TRUE;
          OldSkipValue  = SkipValue;
        }

        MenuOption = MENU_OPTION_FROM_LINK (SavedListEntry);

        UpdateStatusBar (INPUT_ERROR, MenuOption->ThisTag->QuestionFlags, FALSE);

      } else {
        SavedMenuOption = MenuOption;
        MenuOption      = MENU_OPTION_FROM_LINK (NewPos);
        if (!IsSelectable (MenuOption)) {
          //
          // If we are at the end of the list and sitting on a text op, we need to more forward
          //
          ScreenOperation = UiUp;
          ControlFlag     = CfScreenOperation;
          break;
        }

        MenuOption = SavedMenuOption;
        //
        // If we are at the end of the list and sitting on a Date/Time op, rewind to the head.
        //
        AdjustDateAndTimePosition (TRUE, &NewPos);
      }
      break;

    case CfUiSave:
      ControlFlag = CfCheckSelection;

      //
      // Submit the form
      //
      Status = SubmitForm (Selection->FormSet, Selection->Form);

      if (!EFI_ERROR (Status)) {
        UpdateStatusBar (INPUT_ERROR, MenuOption->ThisTag->QuestionFlags, FALSE);
        UpdateStatusBar (NV_UPDATE_REQUIRED, MenuOption->ThisTag->QuestionFlags, FALSE);
      } else {
        do {
          CreateDialog (4, TRUE, 0, NULL, &Key, gEmptyString, gSaveFailed, gPressEnter, gEmptyString);
        } while (Key.UnicodeChar != CHAR_CARRIAGE_RETURN);

        Repaint = TRUE;
        NewLine = TRUE;
      }
      break;

    case CfUiDefault:
      ControlFlag = CfCheckSelection;
      if (!Selection->FormEditable) {
        //
        // This Form is not editable, ignore the F9 (reset to default)
        //
        break;
      }

      Status = ExtractFormDefault (Selection->FormSet, Selection->Form, DefaultId);

      if (!EFI_ERROR (Status)) {
        Selection->Action = UI_ACTION_REFRESH_FORM;
        Selection->Statement = NULL;

        //
        // Show NV update flag on status bar
        //
        gNvUpdateRequired = TRUE;
      }
      break;

    case CfUiNoOperation:
      ControlFlag = CfCheckSelection;
      break;

    case CfExit:
      UiFreeRefreshList ();

      gST->ConOut->SetAttribute (gST->ConOut, EFI_TEXT_ATTR (EFI_LIGHTGRAY, EFI_BLACK));

      return EFI_SUCCESS;

    default:
      break;
    }
  }
}
