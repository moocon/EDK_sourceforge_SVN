/*++

Copyright (c) 2006, Intel Corporation                                                         
All rights reserved. This program and the accompanying materials                          
are licensed and made available under the terms and conditions of the BSD License         
which accompanies this distribution.  The full text of the license may be found at        
http://opensource.org/licenses/bsd-license.php                                            
                                                                                          
THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

Module Name: 

  BdsPlatform.h

Abstract:

  Head file for BDS Platform specific code

--*/

#ifndef _UOL_BDS_PLATFORM_H_
#define _UOL_BDS_PLATFORM_H_

#include "Tiano.h"
#include "Bds.h"
#include "BdsLib.h"
#include "GraphicsLib.h"
#include "Pci22.h"

#include EFI_PROTOCOL_DEFINITION (UgaSplash)
#include EFI_PROTOCOL_DEFINITION (WinNtThunk)
#include EFI_PROTOCOL_DEFINITION (WinNtIo)
#include EFI_PROTOCOL_DEFINITION (PciIo)

extern BDS_CONSOLE_CONNECT_ENTRY  gPlatformConsole[];
extern EFI_DEVICE_PATH_PROTOCOL   *gPlatformConnectSequence[];
extern EFI_DEVICE_PATH_PROTOCOL   *gPlatformDriverOption[];
extern EFI_DEVICE_PATH_PROTOCOL   *gPlatformRootBridges[];
extern ACPI_HID_DEVICE_PATH       gPnpPs2KeyboardDeviceNode;
extern ACPI_HID_DEVICE_PATH       gPnp16550ComPortDeviceNode;
extern UART_DEVICE_PATH           gUartDeviceNode;
extern VENDOR_DEVICE_PATH         gTerminalTypeDeviceNode;

#define PCI_DEVICE_PATH_NODE(Func, Dev) \
  { \
    HARDWARE_DEVICE_PATH, \
    HW_PCI_DP, \
    (UINT8) (sizeof (PCI_DEVICE_PATH)), \
    (UINT8) ((sizeof (PCI_DEVICE_PATH)) >> 8), \
    (Func), \
    (Dev) \
  }

#define PNPID_DEVICE_PATH_NODE(PnpId) \
  { \
    ACPI_DEVICE_PATH, \
    ACPI_DP, \
    (UINT8) (sizeof (ACPI_HID_DEVICE_PATH)), \
    (UINT8) ((sizeof (ACPI_HID_DEVICE_PATH)) >> 8), \
    EISA_PNP_ID((PnpId)), \
    0 \
  }

#define gPciRootBridge \
  PNPID_DEVICE_PATH_NODE(0x0A03)

#define gPciIsaBridge \
  PCI_DEVICE_PATH_NODE(0, 0x1f)

#define gP2PBridge \
  PCI_DEVICE_PATH_NODE(0, 0x1e)

#define gPnpPs2Keyboard \
  PNPID_DEVICE_PATH_NODE(0x0303)

#define gPnp16550ComPort \
  PNPID_DEVICE_PATH_NODE(0x0501)

#define gUart \
  { \
    MESSAGING_DEVICE_PATH, \
    MSG_UART_DP, \
    (UINT8) (sizeof (UART_DEVICE_PATH)), \
    (UINT8) ((sizeof (UART_DEVICE_PATH)) >> 8), \
    0, \
    115200, \
    8, \
    1, \
    1 \
  }

#define gPcAnsiTerminal \
  { \
    MESSAGING_DEVICE_PATH, \
    MSG_VENDOR_DP, \
    (UINT8) (sizeof (VENDOR_DEVICE_PATH)), \
    (UINT8) ((sizeof (VENDOR_DEVICE_PATH)) >> 8), \
    DEVICE_PATH_MESSAGING_PC_ANSI \
  }

#define gEndEntire \
  { \
    END_DEVICE_PATH_TYPE, \
    END_ENTIRE_DEVICE_PATH_SUBTYPE, \
    END_DEVICE_PATH_LENGTH, \
    0 \
  }

#define PCI_CLASS_SCC          0x07
#define PCI_SUBCLASS_SERIAL    0x00
#define PCI_IF_16550           0x02
#define IS_PCI_16550SERIAL(_p)           IS_CLASS3 (_p, PCI_CLASS_SCC, PCI_SUBCLASS_SERIAL, PCI_IF_16550)

//
// Platform Root Bridge
//
typedef struct {
  ACPI_HID_DEVICE_PATH      PciRootBridge;
  EFI_DEVICE_PATH_PROTOCOL  End;
} PLATFORM_ROOT_BRIDGE_DEVICE_PATH;

typedef struct {
  ACPI_HID_DEVICE_PATH      PciRootBridge;
  PCI_DEVICE_PATH           IsaBridge;
  ACPI_HID_DEVICE_PATH      Keyboard;
  EFI_DEVICE_PATH_PROTOCOL  End;
} PLATFORM_DUMMY_ISA_KEYBOARD_DEVICE_PATH;

typedef struct {
  ACPI_HID_DEVICE_PATH      PciRootBridge;
  PCI_DEVICE_PATH           IsaBridge;
  ACPI_HID_DEVICE_PATH      IsaSerial;
  UART_DEVICE_PATH          Uart;
  VENDOR_DEVICE_PATH        TerminalType;
  EFI_DEVICE_PATH_PROTOCOL  End;
} PLATFORM_DUMMY_ISA_SERIAL_DEVICE_PATH;

typedef struct {
  ACPI_HID_DEVICE_PATH      PciRootBridge;
  PCI_DEVICE_PATH           VgaDevice;
  EFI_DEVICE_PATH_PROTOCOL  End;
} PLATFORM_DUMMY_PCI_VGA_DEVICE_PATH;

typedef struct {
  ACPI_HID_DEVICE_PATH      PciRootBridge;
  PCI_DEVICE_PATH           PciBridge;
  PCI_DEVICE_PATH           SerialDevice;
  UART_DEVICE_PATH          Uart;
  VENDOR_DEVICE_PATH        TerminalType;
  EFI_DEVICE_PATH_PROTOCOL  End;
} PLATFORM_DUMMY_PCI_SERIAL_DEVICE_PATH;

extern PLATFORM_ROOT_BRIDGE_DEVICE_PATH  gPlatformRootBridge0;

//
// Platform BDS Functions
//
VOID
PlatformBdsInit (
  IN EFI_BDS_ARCH_PROTOCOL_INSTANCE  *PrivateData
  )
;

VOID
PlatformBdsPolicyBehavior (
  IN EFI_BDS_ARCH_PROTOCOL_INSTANCE  *PrivateData,
  IN EFI_LIST_ENTRY                  *DriverOptionList,
  IN EFI_LIST_ENTRY                  *BootOptionList
  )
;

VOID
PlatformBdsGetDriverOption (
  IN EFI_LIST_ENTRY               *BdsDriverLists
  )
;

EFI_STATUS
BdsMemoryTest (
  EXTENDMEM_COVERAGE_LEVEL Level
  )
;

EFI_STATUS
PlatformBdsShowProgress (
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL TitleForeground,
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL TitleBackground,
  CHAR16                        *Title,
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL ProgressColor,
  UINTN                         Progress,
  UINTN                         PreviousValue
  )
;

VOID
PlatformBdsConnectSequence (
  VOID
  )
;

VOID
PlatformBdsBootFail (
  IN  BDS_COMMON_OPTION  *Option,
  IN  EFI_STATUS         Status,
  IN  CHAR16             *ExitData,
  IN  UINTN              ExitDataSize
  )
;

VOID
PlatformBdsBootSuccess (
  IN  BDS_COMMON_OPTION *Option
  )
;

EFI_STATUS
ProcessCapsules (
  EFI_BOOT_MODE BootMode
  )
;

EFI_STATUS
PlatformBdsConnectConsole (
  IN BDS_CONSOLE_CONNECT_ENTRY   *PlatformConsole
  )
;

EFI_STATUS
PlatformBdsNoConsoleAction (
  VOID
  )
;

#endif // _UOL_BDS_PLATFORM_H_
