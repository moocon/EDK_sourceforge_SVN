/*++

Copyright 2004, Intel Corporation                                                         
All rights reserved. This program and the accompanying materials                          
are licensed and made available under the terms and conditions of the BSD License         
which accompanies this distribution.  The full text of the license may be found at        
http://opensource.org/licenses/bsd-license.php                                            
                                                                                          
THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

Module Name:

  WinNtThunk.h

Abstract:

--*/

// TODO: add protective #ifndef
#include "Efi2WinNT.h"
#include "EfiWinNtLib.h"
#include "EfiDriverLib.h"
#include EFI_PROTOCOL_DEFINITION (DevicePath)

//
// WinNtThunk Device Path Protocol Instance Type
//
typedef struct {
  VENDOR_DEVICE_PATH        Vendor;
  EFI_DEVICE_PATH_PROTOCOL  EndDevicePath;
} WIN_NT_THUNK_DEVICE_PATH;
