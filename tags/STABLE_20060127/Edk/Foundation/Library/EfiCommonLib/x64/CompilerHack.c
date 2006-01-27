/*++

Copyright (c) 2005, Intel Corporation                                                         
All rights reserved. This program and the accompanying materials                          
are licensed and made available under the terms and conditions of the BSD License         
which accompanies this distribution.  The full text of the license may be found at        
http://opensource.org/licenses/bsd-license.php                                            
                                                                                          
THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

Module Name:
  CompilerHack.c

Abstract:

  The x64 compiler inlines memcpy and memset and we can not stop it.
  These routines allow the code to link!!!!!!! Need to fix the compiler!!!!

  There is no *.h definition of these modules as they are well known by the 
  compiler. See MSFT DDK documentation for more details!

  volatile is used to prevent the compiler from trying to implement these
  C functions as inline functions. 

--*/

#include "Tiano.h"

VOID *
memset (
  OUT VOID    *Dest,
  IN  UINTN   Char,
  IN  UINTN   Count
  )
{
  volatile UINT8  *Ptr;
  
  for (Ptr = Dest; Count > 0; Count--, Ptr++) {
    *Ptr = (UINT8) Char;
  }

  return Dest;
}

VOID *
memcpy (
  OUT VOID        *Dest,
  IN  const VOID  *Src,
  IN  UINTN       Count
  )
{
  volatile UINT8  *Ptr;
  const    UINT8  *Source;
  
  for (Ptr = Dest, Source = Src; Count > 0; Count--, Source++, Ptr++) {
    *Ptr = *Source;
  }

  return Dest;
}

