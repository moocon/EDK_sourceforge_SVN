/*++

Copyright 2004, Intel Corporation                                                         
All rights reserved. This program and the accompanying materials                          
are licensed and made available under the terms and conditions of the BSD License         
which accompanies this distribution.  The full text of the license may be found at        
http://opensource.org/licenses/bsd-license.php                                            
                                                                                          
THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

Module Name:

  BootMode.c
   
Abstract:

  EFI 2.0 PEIM to provide the platform support functionality within Windows

--*/

#include "Tiano.h"
#include "Pei.h"
#include EFI_PPI_DEFINITION (BootMode)
#include EFI_PPI_DEFINITION (BootInRecoveryMode)

//
// Module globals
//
EFI_PEI_PPI_DESCRIPTOR mPpiListBootMode = {
  (EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
  &gPeiMasterBootModePpiGuid,
  NULL
};

EFI_PEI_PPI_DESCRIPTOR mPpiListRecoveryBootMode = {
  (EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
  &gPeiBootInRecoveryModePpiGuid,
  NULL
};

EFI_STATUS
EFIAPI
InitializeBootMode (
  IN EFI_FFS_FILE_HEADER       *FfsHeader,
  IN EFI_PEI_SERVICES          **PeiServices
  )
/*++

Routine Description:

  Peform the boot mode determination logic

Arguments:

  PeiServices - General purpose services available to every PEIM.
    
Returns:

  Status -  EFI_SUCCESS if the boot mode could be set

--*/
{
  EFI_STATUS        Status;
  UINTN             BootMode;
  //
  // Let's assume things are OK if not told otherwise
  // Should we read an environment variable in order to easily change this?
  //
  BootMode = BOOT_WITH_FULL_CONFIGURATION;

  Status = (**PeiServices).SetBootMode (PeiServices, (UINT8) BootMode);

  ASSERT_PEI_ERROR (PeiServices, Status);    

  Status = (**PeiServices).InstallPpi (PeiServices, &mPpiListBootMode);

  ASSERT_PEI_ERROR (PeiServices, Status);    

  if (BootMode == BOOT_IN_RECOVERY_MODE) {
    Status = (**PeiServices).InstallPpi (PeiServices, &mPpiListRecoveryBootMode);
    ASSERT_PEI_ERROR (PeiServices, Status);    
  }
  return Status;
}
