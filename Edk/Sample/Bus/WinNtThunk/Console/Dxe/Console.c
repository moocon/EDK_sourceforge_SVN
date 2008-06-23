/*++

Copyright (c) 2004 - 2008, Intel Corporation                                                         
All rights reserved. This program and the accompanying materials                          
are licensed and made available under the terms and conditions of the BSD License         
which accompanies this distribution.  The full text of the license may be found at        
http://opensource.org/licenses/bsd-license.php                                            
                                                                                          
THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

Module Name:

  Console.c

Abstract:

  Console based on Win32 APIs. 

--*/

#include "Console.h"

EFI_STATUS
EFIAPI
WinNtConsoleDriverBindingSupported (
  IN  EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN  EFI_HANDLE                   Handle,
  IN  EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  );

EFI_STATUS
EFIAPI
WinNtConsoleDriverBindingStart (
  IN  EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN  EFI_HANDLE                   Handle,
  IN  EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  );

EFI_STATUS
EFIAPI
WinNtConsoleDriverBindingStop (
  IN  EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN  EFI_HANDLE                   Handle,
  IN  UINTN                        NumberOfChildren,
  IN  EFI_HANDLE                   *ChildHandleBuffer
  );

EFI_DRIVER_BINDING_PROTOCOL gWinNtConsoleDriverBinding = {
  WinNtConsoleDriverBindingSupported,
  WinNtConsoleDriverBindingStart,
  WinNtConsoleDriverBindingStop,
  0xa,
  NULL,
  NULL
};

EFI_DRIVER_ENTRY_POINT (InitializeWinNtConsole)

EFI_STATUS
EFIAPI
InitializeWinNtConsole (
  IN EFI_HANDLE            ImageHandle,
  IN EFI_SYSTEM_TABLE      *SystemTable
  )
/*++

Routine Description:

  Intialize Win32 windows to act as EFI SimpleTextOut and SimpleTextIn windows
  following the EFI 1.1 driver model. 

Arguments:

  (Standard EFI Image entry - EFI_IMAGE_ENTRY_POINT)

Returns: 

  EFI_STATUS

--*/
// TODO:    ImageHandle - add argument and description to function comment
// TODO:    SystemTable - add argument and description to function comment
{
  return INSTALL_ALL_DRIVER_PROTOCOLS_OR_PROTOCOLS2 (
          ImageHandle,
          SystemTable,
          &gWinNtConsoleDriverBinding,
          ImageHandle,
          &gWinNtConsoleComponentName,
          NULL,
          NULL
          );
}

EFI_STATUS
EFIAPI
WinNtConsoleDriverBindingSupported (
  IN  EFI_DRIVER_BINDING_PROTOCOL   *This,
  IN  EFI_HANDLE                    Handle,
  IN  EFI_DEVICE_PATH_PROTOCOL      *RemainingDevicePath
  )
/*++

Routine Description:

Arguments:

Returns:

  None

--*/
// TODO:    This - add argument and description to function comment
// TODO:    Handle - add argument and description to function comment
// TODO:    RemainingDevicePath - add argument and description to function comment
{
  EFI_STATUS              Status;
  EFI_WIN_NT_IO_PROTOCOL  *WinNtIo;

  //
  // Open the IO Abstraction(s) needed to perform the supported test
  //
  Status = gBS->OpenProtocol (
                  Handle,
                  &gEfiWinNtIoProtocolGuid,
                  &WinNtIo,
                  This->DriverBindingHandle,
                  Handle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Make sure that the WinNt Thunk Protocol is valid
  //
  Status = EFI_UNSUPPORTED;
  if (WinNtIo->WinNtThunk->Signature == EFI_WIN_NT_THUNK_PROTOCOL_SIGNATURE) {

    //
    // Check the GUID to see if this is a handle type the driver supports
    //
    if (EfiCompareGuid (WinNtIo->TypeGuid, &gEfiWinNtConsoleGuid)) {
      Status = EFI_SUCCESS;
    }
  }

  //
  // Close the I/O Abstraction(s) used to perform the supported test
  //
  gBS->CloseProtocol (
        Handle,
        &gEfiWinNtIoProtocolGuid,
        This->DriverBindingHandle,
        Handle
        );

  return Status;
}

EFI_STATUS
EFIAPI
WinNtConsoleDriverBindingStart (
  IN  EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN  EFI_HANDLE                   Handle,
  IN  EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
/*++

Routine Description:

Arguments:

Returns:

  None

--*/
// TODO:    This - add argument and description to function comment
// TODO:    Handle - add argument and description to function comment
// TODO:    RemainingDevicePath - add argument and description to function comment
{
  EFI_STATUS                      Status;
  EFI_WIN_NT_IO_PROTOCOL          *WinNtIo;
  WIN_NT_SIMPLE_TEXT_PRIVATE_DATA *Private;

  //
  // Grab the IO abstraction we need to get any work done
  //
  Status = gBS->OpenProtocol (
                  Handle,
                  &gEfiWinNtIoProtocolGuid,
                  &WinNtIo,
                  This->DriverBindingHandle,
                  Handle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->AllocatePool (
                  EfiBootServicesData,
                  sizeof (WIN_NT_SIMPLE_TEXT_PRIVATE_DATA),
                  &Private
                  );
  if (EFI_ERROR (Status)) {
    goto Done;
  }

  EfiZeroMem (Private, sizeof (WIN_NT_SIMPLE_TEXT_PRIVATE_DATA));

  Private->Signature  = WIN_NT_SIMPLE_TEXT_PRIVATE_DATA_SIGNATURE;
  Private->Handle     = Handle;
  Private->WinNtIo    = WinNtIo;
  Private->WinNtThunk = WinNtIo->WinNtThunk;

  WinNtSimpleTextOutOpenWindow (Private);
  WinNtSimpleTextInAttachToWindow (Private);

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Handle,
                  &gEfiSimpleTextOutProtocolGuid,
                  &Private->SimpleTextOut,
                  &gEfiSimpleTextInProtocolGuid,
                  &Private->SimpleTextIn,
                  NULL
                  );
  if (!EFI_ERROR (Status)) {
    return Status;
  }

Done:
  gBS->CloseProtocol (
        Handle,
        &gEfiWinNtIoProtocolGuid,
        This->DriverBindingHandle,
        Handle
        );
  if (Private != NULL) {

    EfiLibFreeUnicodeStringTable (Private->ControllerNameTable);

    if (Private->NtOutHandle != NULL) {
      Private->WinNtThunk->CloseHandle (Private->NtOutHandle);
    }

    if (Private->SimpleTextIn.WaitForKey != NULL) {
      gBS->CloseEvent (Private->SimpleTextIn.WaitForKey);
    }

    gBS->FreePool (Private);
  }

  return Status;
}

EFI_STATUS
EFIAPI
WinNtConsoleDriverBindingStop (
  IN  EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN  EFI_HANDLE                   Handle,
  IN  UINTN                        NumberOfChildren,
  IN  EFI_HANDLE                   *ChildHandleBuffer
  )
/*++

Routine Description:

  TODO: Add function description

Arguments:

  This              - TODO: add argument description
  Handle            - TODO: add argument description
  NumberOfChildren  - TODO: add argument description
  ChildHandleBuffer - TODO: add argument description

Returns:

  EFI_UNSUPPORTED - TODO: Add description for return value

--*/
{
  EFI_SIMPLE_TEXT_OUT_PROTOCOL    *SimpleTextOut;
  EFI_STATUS                      Status;
  WIN_NT_SIMPLE_TEXT_PRIVATE_DATA *Private;

  //
  // Kick people off our interface???
  //
  Status = gBS->OpenProtocol (
                  Handle,
                  &gEfiSimpleTextOutProtocolGuid,
                  &SimpleTextOut,
                  This->DriverBindingHandle,
                  Handle,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    return EFI_UNSUPPORTED;
  }

  Private = WIN_NT_SIMPLE_TEXT_OUT_PRIVATE_DATA_FROM_THIS (SimpleTextOut);

  ASSERT (Private->Handle == Handle);

  Status = gBS->UninstallMultipleProtocolInterfaces (
                  Handle,
                  &gEfiSimpleTextOutProtocolGuid,
                  &Private->SimpleTextOut,
                  &gEfiSimpleTextInProtocolGuid,
                  &Private->SimpleTextIn,
                  NULL
                  );
  if (!EFI_ERROR (Status)) {

    //
    // Shut down our device
    //
    Status = gBS->CloseProtocol (
                    Handle,
                    &gEfiWinNtIoProtocolGuid,
                    This->DriverBindingHandle,
                    Handle
                    );

    Status = gBS->CloseEvent (Private->SimpleTextIn.WaitForKey);
    ASSERT_EFI_ERROR (Status);

    Private->WinNtThunk->CloseHandle (Private->NtOutHandle);
    //
    // DO NOT close Private->NtInHandle. It points to StdIn and not
    //  the Private->NtOutHandle is StdIn and should not be closed!
    //
    EfiLibFreeUnicodeStringTable (Private->ControllerNameTable);

    gBS->FreePool (Private);
  }

  return Status;
}
