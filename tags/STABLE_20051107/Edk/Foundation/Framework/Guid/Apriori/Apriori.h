/*++

Copyright (c) 2004, Intel Corporation                                                         
All rights reserved. This program and the accompanying materials                          
are licensed and made available under the terms and conditions of the BSD License         
which accompanies this distribution.  The full text of the license may be found at        
http://opensource.org/licenses/bsd-license.php                                            
                                                                                          
THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

Module Name:

  Apriori.h
    
Abstract:

  GUID used as an FV filename for A Priori file. The A Priori file contains a
  list of FV filenames that the DXE dispatcher will schedule reguardless of
  the dependency grammer.

--*/

#ifndef _APRIORI_GUID_H_
#define _APRIORI_GUID_H_

#define EFI_APRIORI_GUID \
  { \
    0xfc510ee7, 0xffdc, 0x11d4, 0xbd, 0x41, 0x0, 0x80, 0xc7, 0x3c, 0x88, 0x81 \
  }

extern EFI_GUID gAprioriGuid;

#endif
