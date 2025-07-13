/** @file
  Simple test program to verify EFI 1.1 shell protocol support

  This test program checks for the presence of legacy EFI 1.1 shell protocols
  and demonstrates basic functionality.

  Copyright (c) 2025, GitHub Copilot. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Protocol/Shell.h>
#include <Protocol/ShellParameters.h>
#include "ShellLegacyProtocols.h"

/**
  Test entry point

  @param[in] ImageHandle  The firmware allocated handle for the EFI image.
  @param[in] SystemTable  A pointer to the EFI System Table.

  @retval EFI_SUCCESS     Test completed successfully
  @retval EFI_ERROR       Test failed
**/
EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                Status;
  EFI_SHELL_PROTOCOL        *ShellProtocol;
  EFI_SHELL_INTERFACE       *ShellInterface;
  EFI_SHELL_ENVIRONMENT     *ShellEnvironment;
  EFI_SHELL_ENVIRONMENT2    *ShellEnvironment2;
  BOOLEAN                   IsEfi11;
  
  Print (L"EFI 1.1 Shell Protocol Test\n");
  Print (L"===========================\n\n");

  //
  // Test EFI version detection
  //
  IsEfi11 = IsEfi11Environment ();
  Print (L"EFI 1.1 Environment Detected: %s\n", IsEfi11 ? L"YES" : L"NO");
  Print (L"System Table Revision: 0x%08x\n", SystemTable->Hdr.Revision);
  
  //
  // Test for modern UEFI Shell Protocol
  //
  Status = gBS->LocateProtocol (
                  &gEfiShellProtocolGuid,
                  NULL,
                  (VOID**)&ShellProtocol
                  );
  Print (L"UEFI Shell Protocol: %s\n", EFI_ERROR(Status) ? L"NOT FOUND" : L"FOUND");
  
  //
  // Test for legacy ShellInterface Protocol
  //
  Status = gBS->HandleProtocol (
                  ImageHandle,
                  &gEfiShellInterfaceGuid,
                  (VOID**)&ShellInterface
                  );
  Print (L"EFI 1.1 ShellInterface Protocol: %s\n", EFI_ERROR(Status) ? L"NOT FOUND" : L"FOUND");
  
  if (!EFI_ERROR(Status)) {
    Print (L"  Argc: %d\n", ShellInterface->Argc);
    if (ShellInterface->Argc > 0 && ShellInterface->Argv != NULL) {
      Print (L"  Argv[0]: %s\n", ShellInterface->Argv[0]);
    }
  }
  
  //
  // Test for legacy ShellEnvironment Protocol
  //
  Status = gBS->LocateProtocol (
                  &gEfiShellEnvironmentGuid,
                  NULL,
                  (VOID**)&ShellEnvironment
                  );
  Print (L"EFI 1.1 ShellEnvironment Protocol: %s\n", EFI_ERROR(Status) ? L"NOT FOUND" : L"FOUND");
  
  if (!EFI_ERROR(Status)) {
    CHAR16 *TestEnv;
    
    //
    // Test environment variable access
    //
    TestEnv = ShellEnvironment->GetEnv (L"path");
    Print (L"  PATH environment variable: %s\n", 
           (TestEnv != NULL) ? L"ACCESSIBLE" : L"NOT ACCESSIBLE");
    if (TestEnv != NULL) {
      FreePool (TestEnv);
    }
  }
  
  //
  // Test for legacy ShellEnvironment2 Protocol
  //
  Status = gBS->LocateProtocol (
                  &gEfiShellEnvironment2Guid,
                  NULL,
                  (VOID**)&ShellEnvironment2
                  );
  Print (L"EFI 1.1 ShellEnvironment2 Protocol: %s\n", EFI_ERROR(Status) ? L"NOT FOUND" : L"FOUND");
  
  Print (L"\nTest completed.\n");
  
  return EFI_SUCCESS;
}
