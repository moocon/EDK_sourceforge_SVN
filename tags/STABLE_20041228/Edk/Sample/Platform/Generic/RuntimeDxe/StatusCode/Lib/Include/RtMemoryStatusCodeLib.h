/*++

Copyright 2004, Intel Corporation                                                         
All rights reserved. This program and the accompanying materials                          
are licensed and made available under the terms and conditions of the BSD License         
which accompanies this distribution.  The full text of the license may be found at        
http://opensource.org/licenses/bsd-license.php                                            
                                                                                          
THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

Module Name:
  
  RtMemoryStatusCodeLib.h
   
Abstract:

  Lib to provide memory status code reporting.

--*/

#ifndef _EFI_RT_MEMORY_STATUS_CODE_LIB_H_
#define _EFI_RT_MEMORY_STATUS_CODE_LIB_H_

//
// Statements that include other files
//
#include "Tiano.h"

//
// Initialization function
//
EFI_BOOTSERVICE
VOID
EFIAPI
RtMemoryInitializeStatusCode (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
;

//
// Status code reporting function
//
EFI_RUNTIMESERVICE
EFI_STATUS
EFIAPI
RtMemoryReportStatusCode (
  IN EFI_STATUS_CODE_TYPE     CodeType,
  IN EFI_STATUS_CODE_VALUE    Value,
  IN UINT32                   Instance,
  IN EFI_GUID                 * CallerId,
  IN EFI_STATUS_CODE_DATA     * Data OPTIONAL
  )
;

//
// Playback all prior status codes to a listener
//
EFI_RUNTIMESERVICE
VOID
EFIAPI
PlaybackStatusCodes (
  IN EFI_REPORT_STATUS_CODE   ReportStatusCode
  )
;

#endif
