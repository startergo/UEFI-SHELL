/** @file
  Legacy EFI 1.1 Shell Protocol implementation for backward compatibility

  This file implements the EFI 1.1 shell protocols including ShellInterface
  and ShellEnvironment protocols to provide compatibility with older
  EFI shell applications.

  Copyright (c) 2025, GitHub Copilot. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "Shell.h"
#include "ShellLegacyProtocols.h"
#include <Library/BaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>

//
// Global GUIDs for legacy protocols
//
EFI_GUID gEfiShellInterfaceGuid = EFI_SHELL_INTERFACE_GUID;
EFI_GUID gEfiShellEnvironmentGuid = EFI_SHELL_ENVIRONMENT_GUID;
EFI_GUID gEfiShellEnvironment2Guid = EFI_SHELL_ENVIRONMENT2_GUID;

//
// Global instances of legacy protocols
//
STATIC EFI_SHELL_INTERFACE     mShellInterface;
STATIC EFI_SHELL_ENVIRONMENT   mShellEnvironment;
STATIC EFI_SHELL_ENVIRONMENT2  mShellEnvironment2;

//
// Legacy protocol support state
//
STATIC BOOLEAN mLegacyProtocolsInitialized = FALSE;

/**
  Legacy wrapper for Execute function

  @param[in] ParentImageHandle   Parent image handle
  @param[in] CommandLine         Command line to execute
  @param[in] Output             Whether to show output

  @retval EFI_SUCCESS           The operation was successful
  @retval EFI_NOT_FOUND         Command not found
  @retval EFI_INVALID_PARAMETER Invalid parameter
**/
EFI_STATUS
EFIAPI
LegacyShellExecute (
  IN EFI_HANDLE   *ParentImageHandle,
  IN CHAR16       *CommandLine,
  IN BOOLEAN      Output
  )
{
  EFI_STATUS Status;
  
  if (CommandLine == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Use the modern shell's execute function
  //
  Status = EfiShellExecute (
             ParentImageHandle,
             CommandLine,
             NULL,    // Environment
             NULL     // StatusCode
             );

  return Status;
}

/**
  Legacy wrapper for GetEnv function

  @param[in]  Name      Environment variable name
  @param[out] Value     Pointer to environment variable value

  @retval VOID
**/
VOID
EFIAPI
LegacyShellGetEnv (
  IN  CHAR16   *Name,
  OUT CHAR16   **Value
  )
{
  CONST CHAR16 *EnvValue;
  
  if ((Name == NULL) || (Value == NULL)) {
    if (Value != NULL) {
      *Value = NULL;
    }
    return;
  }

  //
  // Use the modern shell's GetEnv function
  //
  EnvValue = EfiShellGetEnv (Name);
  
  if (EnvValue != NULL) {
    *Value = AllocateCopyPool (StrSize (EnvValue), (VOID*)EnvValue);
  } else {
    *Value = NULL;
  }
}

/**
  Legacy wrapper for GetEnv function that returns value directly

  @param[in] Name      Environment variable name

  @retval CHAR16*      Environment variable value or NULL
**/
CHAR16*
EFIAPI
LegacyShellGetEnvValue (
  IN CHAR16   *Name
  )
{
  CONST CHAR16 *EnvValue;
  CHAR16       *RetValue;
  
  if (Name == NULL) {
    return NULL;
  }

  //
  // Use the modern shell's GetEnv function
  //
  EnvValue = EfiShellGetEnv (Name);
  
  if (EnvValue != NULL) {
    RetValue = AllocateCopyPool (StrSize (EnvValue), (VOID*)EnvValue);
    return RetValue;
  }
  
  return NULL;
}

/**
  Legacy wrapper for SetEnv function

  @param[in] Name      Environment variable name
  @param[in] Value     Environment variable value

  @retval EFI_SUCCESS           The operation was successful
  @retval EFI_INVALID_PARAMETER Invalid parameter
**/
EFI_STATUS
EFIAPI
LegacyShellSetEnv (
  IN CHAR16   *Name,
  IN CHAR16   *Value
  )
{
  if (Name == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Use the modern shell's SetEnv function
  // Default to volatile for legacy compatibility
  //
  return EfiShellSetEnv (Name, Value, TRUE);
}

/**
  Legacy wrapper for GetAlias function

  @param[in]  Name      Alias name
  @param[out] Volatile  Whether alias is volatile

  @retval CHAR16*      Alias value or NULL
**/
CHAR16*
EFIAPI
LegacyShellGetAlias (
  IN  CHAR16    *Name,
  OUT BOOLEAN   *Volatile OPTIONAL
  )
{
  CONST CHAR16 *AliasValue;
  CHAR16       *RetValue;
  
  if (Name == NULL) {
    return NULL;
  }

  //
  // Use the modern shell's GetAlias function
  //
  AliasValue = EfiShellGetAlias (Name, Volatile);
  
  if (AliasValue != NULL) {
    RetValue = AllocateCopyPool (StrSize (AliasValue), (VOID*)AliasValue);
    return RetValue;
  }
  
  return NULL;
}

/**
  Legacy wrapper for SetAlias function

  @param[in] Name      Alias name
  @param[in] Value     Alias value
  @param[in] Replace   Whether to replace existing alias
  @param[in] Volatile  Whether alias is volatile

  @retval EFI_SUCCESS           The operation was successful
  @retval EFI_INVALID_PARAMETER Invalid parameter
  @retval EFI_ACCESS_DENIED     Access denied
**/
EFI_STATUS
EFIAPI
LegacyShellSetAlias (
  IN CHAR16    *Name,
  IN CHAR16    *Value,
  IN BOOLEAN   Replace,
  IN BOOLEAN   Volatile
  )
{
  if (Name == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Use the modern shell's SetAlias function
  //
  return EfiShellSetAlias (Name, Value, Replace, Volatile);
}

/**
  Legacy wrapper for GetCurDir function

  @param[out] Dir      Current directory

  @retval VOID
**/
VOID
EFIAPI
LegacyShellGetCurDir (
  OUT CHAR16   **Dir
  )
{
  CONST CHAR16 *CurDir;
  
  if (Dir == NULL) {
    return;
  }

  //
  // Use the modern shell's GetCurDir function
  //
  CurDir = EfiShellGetCurDir (NULL);
  
  if (CurDir != NULL) {
    *Dir = AllocateCopyPool (StrSize (CurDir), (VOID*)CurDir);
  } else {
    *Dir = NULL;
  }
}

/**
  Initialize legacy shell protocol support

  @retval EFI_SUCCESS           The operation was successful
  @retval EFI_ALREADY_STARTED   Already initialized
  @retval EFI_OUT_OF_RESOURCES  Out of resources
**/
EFI_STATUS
EFIAPI
InitializeLegacyShellSupport (
  VOID
  )
{
  if (mLegacyProtocolsInitialized) {
    return EFI_ALREADY_STARTED;
  }

  //
  // Initialize Shell Environment Protocol
  //
  mShellEnvironment.Execute = LegacyShellExecute;
  mShellEnvironment.GetEnv = LegacyShellGetEnvValue;
  mShellEnvironment.SetEnv = LegacyShellSetEnv;
  mShellEnvironment.GetAlias = LegacyShellGetAlias;
  mShellEnvironment.SetAlias = LegacyShellSetAlias;
  mShellEnvironment.GetCurDir = LegacyShellGetCurDir;

  //
  // Initialize Shell Environment2 Protocol (same as Environment for now)
  //
  CopyMem (&mShellEnvironment2, &mShellEnvironment, sizeof(EFI_SHELL_ENVIRONMENT));

  mLegacyProtocolsInitialized = TRUE;
  
  return EFI_SUCCESS;
}

/**
  Install Shell Interface Protocol on a specific image handle

  @param[in] ImageHandle   Image handle to install protocol on
  @param[in] Argv         Command line arguments
  @param[in] Argc         Number of arguments

  @retval EFI_SUCCESS           The operation was successful
  @retval EFI_INVALID_PARAMETER Invalid parameter
  @retval EFI_OUT_OF_RESOURCES  Out of resources
**/
EFI_STATUS
EFIAPI
InstallShellInterfaceProtocol (
  IN EFI_HANDLE   ImageHandle,
  IN CHAR16       **Argv,
  IN UINTN        Argc
  )
{
  EFI_STATUS                Status;
  EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
  EFI_SHELL_INTERFACE       *ShellInterface;

  if (ImageHandle == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Get the loaded image protocol
  //
  Status = gBS->OpenProtocol (
                  ImageHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID**)&LoadedImage,
                  gImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Allocate shell interface structure
  //
  ShellInterface = AllocateZeroPool (sizeof(EFI_SHELL_INTERFACE));
  if (ShellInterface == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Initialize shell interface
  //
  ShellInterface->ImageHandle = ImageHandle;
  ShellInterface->Info = LoadedImage;
  ShellInterface->Argc = Argc;
  ShellInterface->Argv = Argv;
  ShellInterface->RedirArgc = 0;
  ShellInterface->RedirArgv = NULL;
  
  //
  // Use the current shell's standard I/O
  //
  ShellInterface->StdIn = ConvertShellHandleToEfiFileProtocol (
                           ShellInfoObject.NewShellParametersProtocol->StdIn
                           );
  ShellInterface->StdOut = ConvertShellHandleToEfiFileProtocol (
                            ShellInfoObject.NewShellParametersProtocol->StdOut
                            );
  ShellInterface->StdErr = ConvertShellHandleToEfiFileProtocol (
                            ShellInfoObject.NewShellParametersProtocol->StdErr
                            );

  //
  // Install the protocol
  //
  Status = gBS->InstallProtocolInterface (
                  &ImageHandle,
                  &gEfiShellInterfaceGuid,
                  EFI_NATIVE_INTERFACE,
                  ShellInterface
                  );

  if (EFI_ERROR (Status)) {
    FreePool (ShellInterface);
  }

  return Status;
}

/**
  Install Shell Environment Protocols

  @retval EFI_SUCCESS           The operation was successful
  @retval EFI_ALREADY_STARTED   Already installed
  @retval EFI_OUT_OF_RESOURCES  Out of resources
**/
EFI_STATUS
EFIAPI
InstallShellEnvironmentProtocol (
  VOID
  )
{
  EFI_STATUS Status;

  //
  // Make sure legacy support is initialized
  //
  Status = InitializeLegacyShellSupport ();
  if (EFI_ERROR (Status) && (Status != EFI_ALREADY_STARTED)) {
    return Status;
  }

  //
  // Install Shell Environment Protocol
  //
  Status = gBS->InstallProtocolInterface (
                  &gImageHandle,
                  &gEfiShellEnvironmentGuid,
                  EFI_NATIVE_INTERFACE,
                  &mShellEnvironment
                  );

  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Install Shell Environment2 Protocol
  //
  Status = gBS->InstallProtocolInterface (
                  &gImageHandle,
                  &gEfiShellEnvironment2Guid,
                  EFI_NATIVE_INTERFACE,
                  &mShellEnvironment2
                  );

  return Status;
}

/**
  Cleanup legacy shell protocol support

  @retval EFI_SUCCESS           The operation was successful
**/
EFI_STATUS
EFIAPI
CleanupLegacyShellSupport (
  VOID
  )
{
  if (!mLegacyProtocolsInitialized) {
    return EFI_SUCCESS;
  }

  //
  // Uninstall protocols
  //
  gBS->UninstallProtocolInterface (
         gImageHandle,
         &gEfiShellEnvironmentGuid,
         &mShellEnvironment
         );

  gBS->UninstallProtocolInterface (
         gImageHandle,
         &gEfiShellEnvironment2Guid,
         &mShellEnvironment2
         );

  mLegacyProtocolsInitialized = FALSE;

  return EFI_SUCCESS;
}

/**
  Detect if running on EFI 1.1 environment

  @retval TRUE     Running on EFI 1.1
  @retval FALSE    Running on UEFI 2.0+
**/
BOOLEAN
EFIAPI
IsEfi11Environment (
  VOID
  )
{
  EFI_STATUS Status;
  VOID       *Interface;

  //
  // Try to locate UEFI 2.0+ specific protocols
  // If they don't exist, we're likely on EFI 1.1
  //
  Status = gBS->LocateProtocol (
                  &gEfiShellProtocolGuid,
                  NULL,
                  &Interface
                  );

  if (EFI_ERROR (Status)) {
    //
    // No modern shell protocol, check for EFI 1.1 indicators
    //
    
    //
    // Check system table revision
    //
    if (gST->Hdr.Revision < EFI_2_00_SYSTEM_TABLE_REVISION) {
      return TRUE;
    }
    
    //
    // Check for existence of legacy shell protocols
    //
    Status = gBS->LocateProtocol (
                    &gEfiShellEnvironmentGuid,
                    NULL,
                    &Interface
                    );
    if (!EFI_ERROR (Status)) {
      return TRUE;
    }
  }

  return FALSE;
}
