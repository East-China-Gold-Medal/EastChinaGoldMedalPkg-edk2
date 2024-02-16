/** @file

  The implementation for resolution fix.

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
#include <Protocol/AcpiSystemDescriptionTable.h>
#include <IndustryStandard/Acpi50.h>

extern EFI_GUID gEfiGraphicsOutputProtocolGuid;
extern EFI_GUID gEfiAcpi20TableGuid;

STATIC
UINT8
SumBytes(
  IN const UINT8* Arr,
  IN UINTN Size
  )
{
  UINT8 Sum = 0;
  for (UINTN i = 0; i < Size; ++i) {
    Sum += Arr[i];
  }
  return Sum;
}

STATIC
VOID
SetAcpiSdtChecksum (
  IN VOID* Data
  )
{
  UINT8* Arr = Data;
  UINTN Size = *(const UINT32*)&Arr[4];
  Arr[9] = 0;
  Arr[9] = -SumBytes(Arr, Size);
}

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
  EFI_GRAPHICS_OUTPUT_PROTOCOL* GraphicsProtocol;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION*  GraphicsInformation;
  EFI_ACPI_5_0_BOOT_GRAPHICS_RESOURCE_TABLE *Bgrt = NULL;
  EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER *Rsdt;
  UINT32 MaxModeIndex=0, MaxModeWidth=0;
  UINT32 OriginalWidth, OriginalHeight;
  UINTN GraphicsInformationSize;

  Status = gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid,NULL,(VOID**) &GraphicsProtocol);
  if(Status) {
    Print(L"Cannot locate GOP:%r\n",Status);
    return Status;
  }

  for(UINT32 i=0;i<GraphicsProtocol->Mode->MaxMode;i++) {
    Status = GraphicsProtocol->QueryMode(GraphicsProtocol,i,&GraphicsInformationSize,&GraphicsInformation);
    if(Status) {
      Print(L"Cannot get information from mode #%d:%r\n",i,Status);
      return Status;
    }
    if (MaxModeWidth<GraphicsInformation->HorizontalResolution) {
      MaxModeWidth=GraphicsInformation->HorizontalResolution;
      MaxModeIndex=i;
    }

    Print (L"Graphics mode #%d, %d*%d\n",i,GraphicsInformation->HorizontalResolution,GraphicsInformation->VerticalResolution);
  }
  Status = GraphicsProtocol->QueryMode(GraphicsProtocol,MaxModeIndex,&GraphicsInformationSize,&GraphicsInformation);
  OriginalWidth = GraphicsProtocol->Mode->Info->HorizontalResolution;
  OriginalHeight = GraphicsProtocol->Mode->Info->VerticalResolution;
  if (GraphicsProtocol->Mode->Info->HorizontalResolution!=MaxModeWidth) {
    Print (L"Changing graphics mode %d*%d -> %d*%d\n",OriginalWidth,OriginalHeight,GraphicsInformation->HorizontalResolution,GraphicsInformation->VerticalResolution);
    Status = GraphicsProtocol->SetMode(GraphicsProtocol,MaxModeIndex);
    if(Status) {
      Print(L"Cannot set graphics mode to #%d:%r\n",MaxModeIndex,Status);
      return Status;
    }
  }
  // Update BGRT.
  for (int i = 0; i < SystemTable->NumberOfTableEntries; i++) {
    if(!(CompareGuid(&SystemTable->ConfigurationTable[i].VendorGuid, &gEfiAcpi20TableGuid))) {
      continue;
    }
    Rsdt = (EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER*) SystemTable->ConfigurationTable[i].VendorTable;
    if (CompareMem(&Rsdt->Signature, "RSD PTR ", 8) != 0 || Rsdt->Revision < 2) {
      continue;
    }
    Print(L"RSDT found @ %p\n",Rsdt);
    EFI_ACPI_SDT_HEADER* Xsdt = (EFI_ACPI_SDT_HEADER *) (UINTN) Rsdt->XsdtAddress;
    EFI_ACPI_SDT_HEADER* TableEntries = &Xsdt[1];
    for(UINTN j=0;j<(Xsdt->Length-sizeof(EFI_ACPI_SDT_HEADER))/sizeof(UINT64);j++) {
      EFI_ACPI_SDT_HEADER *Entry = &TableEntries[j];
      if (CompareMem(&Entry->Signature, "BGRT", 4) != 0) {
        continue;
      }
      Bgrt = (EFI_ACPI_5_0_BOOT_GRAPHICS_RESOURCE_TABLE*) Entry;
      break;
    }
    break;
  }
  if(Bgrt) {
    Print(L"BGRT found @ %p\n",Bgrt);
    Print(L"Image info: margin (%d,%d),%s\n",Bgrt->ImageOffsetX, Bgrt->ImageOffsetY,Bgrt->Status?L"Displayed":L"Not Displayed");

  }
  else {
    Print(L"BGRT Not found!\n");
  }
  // Chainload Bootmgfw.
  static CHAR16 MsBootPath[] = L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi";
  EFI_LOADED_IMAGE* Image=NULL;
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
