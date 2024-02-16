/** @file

  The implementation for Slic table loading.

  SPDX-License-Identifier: WTFPL

**/

#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/AcpiTable.h>

// the SLIC table, in binary bytes.
extern UINT8 SlicTable[];
extern UINTN SlicTableSize;

STATIC
EFI_HANDLE
LoadApp (
  IN EFI_HANDLE ImageHandle,
  IN EFI_LOADED_IMAGE* Image,
  IN const CHAR16* Path)
{
  EFI_DEVICE_PATH* BootDp = FileDevicePath(Image->DeviceHandle, (CHAR16*) Path);
  EFI_HANDLE Result = 0;
  Print(L"Loading application %s.\n", Path);
  if (EFI_ERROR(gBS->LoadImage(0, ImageHandle, BootDp, 0, 0, &Result))) {
    Print(L"Failed to load application %s.\n", Path);
  }
  return Result;
}

EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS     Status;
  static CHAR16 MsBootPath[] = L"\\EFI\\BOOT\\BOOTX64_CHAIN.efi";
  EFI_LOADED_IMAGE* Image=NULL;
  UINTN SlicTableKey;
  EFI_ACPI_TABLE_PROTOCOL* AcpiTableProtocol;

  Status = gBS->LocateProtocol(&gEfiAcpiTableProtocolGuid,NULL,(VOID**) &AcpiTableProtocol);
  if(Status) {
    Print(L"Cannot locate ACPI Table protocol:%r\n",Status);
    return Status;
  }

  Status = AcpiTableProtocol->InstallAcpiTable(AcpiTableProtocol,SlicTable,SlicTableSize,&SlicTableKey);
  if (Status) {
    Print (L"Cannot install SLIC Table:%r.\n",Status);
    return Status;
  }

  Status = gBS->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (void**) &Image);
  if (Status) {
    Print (L"Cannot load bootmgfw:%r.\n",Status);
    return Status;
  }

  EFI_HANDLE NextImageHandle = LoadApp(ImageHandle, Image, MsBootPath);
  if(NextImageHandle!=NULL) {
    Print (L"Starting bootmgfw...\n");
    gBS->StartImage(NextImageHandle, 0,0);
  }
  Print (L"End......\n");
  return EFI_SUCCESS;
}
