/*++

Copyright 2004, Intel Corporation                                                         
All rights reserved. This program and the accompanying materials                          
are licensed and made available under the terms and conditions of the BSD License         
which accompanies this distribution.  The full text of the license may be found at        
http://opensource.org/licenses/bsd-license.php                                            
                                                                                          
THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

Module Name:

  UsbHostController.c

Abstract:

  USB Host Controller protocol.

 
--*/

#include "Tiano.h"
#include EFI_PROTOCOL_DEFINITION (UsbHostController)

EFI_GUID  gEfiUsbHcProtocolGuid = EFI_USB_HC_PROTOCOL_GUID;

EFI_GUID_STRING(&gEfiUsbHcProtocolGuid, "Usb Host Controller Protocol", "USB 1.1 Host Controller");
