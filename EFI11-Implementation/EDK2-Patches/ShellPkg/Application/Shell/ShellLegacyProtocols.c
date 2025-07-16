/** @file
  Legacy EFI 1.1 Shell Protocol implementation for backward compatibility

  This file implements minimal EFI 1.1 shell protocols for compatibility 
  with older EFI shell applications.

  Copyright (c) 2025, GitHub Copilot. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "Shell.h"
#include "ShellLegacyProtocols.h"
#include <Library/BaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/ShellParameters.h>
#include <Protocol/LoadedImage.h>

//
// Global GUIDs for legacy protocols (declared in ShellPkg.dec)
//
extern EFI_GUID gEfiShellEnvironmentGuid;

//
// Global instances of legacy protocols
//
STATIC EFI_SHELL_ENVIRONMENT   mShellEnvironment;

//
// Shell Interface Protocol instances for applications
//
STATIC SHELL_INTERFACE_INSTANCE mShellInterfaces[16];  // Support up to 16 concurrent apps
STATIC UINTN                   mActiveInterfaces = 0;

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

  @param[in] Name      Environment variable name

  @retval NULL         Variable not found
  @retval Other        Pointer to environment variable value
**/
CHAR16 *
EFIAPI
LegacyShellGetEnv (
  IN CHAR16   *Name
  )
{
  CONST CHAR16 *EnvValue;
  CHAR16       *Result;
  
  if (Name == NULL) {
    return NULL;
  }

  //
  // Use the modern shell's get environment function
  //
  EnvValue = EfiShellGetEnv (Name);
  if (EnvValue != NULL) {
    //
    // Allocate a copy for the caller
    //
    Result = AllocateCopyPool (StrSize (EnvValue), (VOID*)EnvValue);
    return Result;
  }

  return NULL;
}

/**
  Legacy wrapper for GetAlias function

  @param[in] Name         Alias name
  @param[out] Volatile    Whether alias is volatile (optional)

  @retval NULL            Alias not found
  @retval Other           Pointer to alias value
**/
CHAR16 *
EFIAPI
LegacyShellGetAlias (
  IN CHAR16   *Name,
  OUT BOOLEAN *Volatile OPTIONAL
  )
{
  CONST CHAR16 *AliasValue;
  CHAR16       *Result;
  
  if (Name == NULL) {
    return NULL;
  }

  //
  // Use the modern shell's get alias function
  //
  AliasValue = EfiShellGetAlias (Name, Volatile);
  if (AliasValue != NULL) {
    //
    // Allocate a copy for the caller
    //
    Result = AllocateCopyPool (StrSize (AliasValue), (VOID*)AliasValue);
    return Result;
  }

  return NULL;
}

/**
  Legacy wrapper for SetAlias function

  @param[in] Name         Alias name
  @param[in] Value        Alias value
  @param[in] Replace      Whether to replace existing alias
  @param[in] Volatile     Whether alias is volatile

  @retval EFI_SUCCESS     The operation was successful
  @retval EFI_ACCESS_DENIED   Alias exists and Replace is FALSE
**/
EFI_STATUS
EFIAPI
LegacyShellSetAlias (
  IN CHAR16   *Name,
  IN CHAR16   *Value,
  IN BOOLEAN  Replace,
  IN BOOLEAN  Volatile
  )
{
  if (Name == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Use the modern shell's set alias function
  //
  return EfiShellSetAlias (Name, Value, Replace, Volatile);
}

/**
  Legacy wrapper for GetCurDir function

  @param[in] FileSystemMapping   File system mapping (optional)

  @retval NULL                   Current directory not available
  @retval Other                  Pointer to current directory string
**/
CHAR16 *
EFIAPI
LegacyShellGetCurDir (
  IN CHAR16 *FileSystemMapping OPTIONAL
  )
{
  CONST CHAR16 *CurrentDir;
  CHAR16       *Result;

  //
  // Use the modern shell's get current directory function
  //
  CurrentDir = EfiShellGetCurDir (FileSystemMapping);
  if (CurrentDir != NULL) {
    //
    // Allocate a copy for the caller
    //
    Result = AllocateCopyPool (StrSize (CurrentDir), (VOID*)CurrentDir);
    return Result;
  }

  return NULL;
}

/**
  Legacy wrapper for SetCurDir function

  @param[in] FileSystemMapping   File system mapping (optional)
  @param[in] Dir                 Directory to set as current

  @retval EFI_SUCCESS           The operation was successful
  @retval EFI_NOT_FOUND         Directory not found
  @retval EFI_INVALID_PARAMETER Invalid parameter
**/
EFI_STATUS
EFIAPI
LegacyShellSetCurDir (
  IN CHAR16 *FileSystemMapping OPTIONAL,
  IN CHAR16 *Dir
  )
{
  if (Dir == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Use the modern shell's set current directory function
  //
  return EfiShellSetCurDir (FileSystemMapping, Dir);
}

/**
  Initialize legacy shell protocol support

  @retval EFI_SUCCESS           Already initialized or initialization successful
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
  // Initialize the legacy shell environment protocol
  //
  ZeroMem (&mShellEnvironment, sizeof (EFI_SHELL_ENVIRONMENT));
  mShellEnvironment.Execute = LegacyShellExecute;
  mShellEnvironment.GetEnv = LegacyShellGetEnv;
  mShellEnvironment.GetCurDir = LegacyShellGetCurDir;
  mShellEnvironment.SetCurDir = LegacyShellSetCurDir;
  mShellEnvironment.GetAlias = LegacyShellGetAlias;
  mShellEnvironment.SetAlias = LegacyShellSetAlias;

  //
  // Initialize Shell Interface instances
  //
  ZeroMem (mShellInterfaces, sizeof (mShellInterfaces));
  mActiveInterfaces = 0;

  mLegacyProtocolsInitialized = TRUE;
  return EFI_SUCCESS;
}

/**
  Install legacy shell environment protocol

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

  return Status;
}

/**
  Create and populate Shell Interface for an application

  @param[in] ImageHandle     Application image handle
  @param[out] ShellInterface Pointer to created shell interface

  @retval EFI_SUCCESS        Interface created successfully
  @retval EFI_OUT_OF_RESOURCES   Out of memory
**/
EFI_STATUS
EFIAPI
CreateShellInterfaceForApplication (
  IN  EFI_HANDLE  ImageHandle,
  OUT EFI_SHELL_INTERFACE **ShellInterface
  )
{
  EFI_STATUS                Status;
  EFI_SHELL_PARAMETERS_PROTOCOL *ShellParameters;
  EFI_LOADED_IMAGE_PROTOCOL     *LoadedImage;
  SHELL_INTERFACE_INSTANCE      *Instance;
  UINTN                         Index;
  UINTN                         ArgIndex;

  if (ShellInterface == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Find an available instance slot
  //
  Instance = NULL;
  for (Index = 0; Index < ARRAY_SIZE(mShellInterfaces); Index++) {
    if (!mShellInterfaces[Index].IsActive) {
      Instance = &mShellInterfaces[Index];
      break;
    }
  }

  if (Instance == NULL) {
    DEBUG ((DEBUG_WARN, "CreateShellInterfaceForApplication: No available interface slots\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Get Shell Parameters Protocol
  //
  Status = gBS->HandleProtocol (
                  ImageHandle,
                  &gEfiShellParametersProtocolGuid,
                  (VOID**)&ShellParameters
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "CreateShellInterfaceForApplication: No ShellParameters protocol - %r\n", Status));
    ShellParameters = NULL;
  }

  //
  // Get Loaded Image Protocol
  //
  Status = gBS->HandleProtocol (
                  ImageHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID**)&LoadedImage
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "CreateShellInterfaceForApplication: Failed to get LoadedImage - %r\n", Status));
    return Status;
  }

  //
  // Initialize the Shell Interface
  //
  ZeroMem (&Instance->Interface, sizeof (EFI_SHELL_INTERFACE));
  Instance->Interface.ImageHandle = ImageHandle;
  Instance->Interface.Info = LoadedImage;

  //
  // Set up Argc/Argv from Shell Parameters
  //
  if (ShellParameters != NULL && ShellParameters->Argc > 0) {
    Instance->Interface.Argc = ShellParameters->Argc;
    
    //
    // Allocate and copy Argv
    //
    Instance->AllocatedArgv = AllocateZeroPool (ShellParameters->Argc * sizeof (CHAR16*));
    if (Instance->AllocatedArgv == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    for (ArgIndex = 0; ArgIndex < ShellParameters->Argc; ArgIndex++) {
      Instance->AllocatedArgv[ArgIndex] = AllocateCopyPool (
                                            StrSize (ShellParameters->Argv[ArgIndex]),
                                            ShellParameters->Argv[ArgIndex]
                                            );
      if (Instance->AllocatedArgv[ArgIndex] == NULL) {
        //
        // Cleanup on failure
        //
        for (UINTN CleanupIndex = 0; CleanupIndex < ArgIndex; CleanupIndex++) {
          FreePool (Instance->AllocatedArgv[CleanupIndex]);
        }
        FreePool (Instance->AllocatedArgv);
        return EFI_OUT_OF_RESOURCES;
      }
    }
    Instance->Interface.Argv = Instance->AllocatedArgv;

    //
    // Allocate and initialize ArgInfo
    //
    Instance->AllocatedArgInfo = AllocateZeroPool (ShellParameters->Argc * sizeof (EFI_SHELL_ARG_INFO));
    if (Instance->AllocatedArgInfo != NULL) {
      Instance->Interface.ArgInfo = Instance->AllocatedArgInfo;
      //
      // Initialize with default attributes (no special parsing for now)
      //
      for (ArgIndex = 0; ArgIndex < ShellParameters->Argc; ArgIndex++) {
        Instance->AllocatedArgInfo[ArgIndex].Attributes = ARG_NO_ATTRIB;
      }
    }
  } else {
    //
    // No parameters available, set up minimal interface
    //
    Instance->Interface.Argc = 1;
    Instance->AllocatedArgv = AllocateZeroPool (sizeof (CHAR16*));
    if (Instance->AllocatedArgv != NULL) {
      Instance->AllocatedArgv[0] = AllocateCopyPool (StrSize (L"unknown"), L"unknown");
      Instance->Interface.Argv = Instance->AllocatedArgv;
    }
  }

  //
  // Set up file redirection (not implemented, set to NULL)
  //
  Instance->Interface.RedirArgv = NULL;
  Instance->Interface.RedirArgc = 0;

  //
  // Set up standard I/O handles (use console for now)
  //
  Instance->Interface.StdIn = NULL;   // Would need to implement file-style console wrapper
  Instance->Interface.StdOut = NULL;  // Would need to implement file-style console wrapper
  Instance->Interface.StdErr = NULL;  // Would need to implement file-style console wrapper

  //
  // Set echo mode
  //
  Instance->Interface.EchoOn = TRUE;

  //
  // Mark instance as active
  //
  Instance->IsActive = TRUE;
  mActiveInterfaces++;

  *ShellInterface = &Instance->Interface;
  
  DEBUG ((DEBUG_INFO, "CreateShellInterfaceForApplication: Created interface for handle %p, Argc=%d\n", 
          ImageHandle, Instance->Interface.Argc));
  
  return EFI_SUCCESS;
}

/**
  Install Shell Interface Protocol for a specific image

  @param[in] ImageHandle   Image handle to install protocol on
  @param[in] Argv         Argument array (legacy parameter, unused)
  @param[in] Argc         Argument count (legacy parameter, unused)

  @retval EFI_SUCCESS     Protocol installed successfully
  @retval Other           Error occurred
**/
EFI_STATUS
EFIAPI
InstallShellInterfaceProtocol (
  IN EFI_HANDLE ImageHandle,
  IN CHAR16     **Argv,
  IN UINTN      Argc
  )
{
  EFI_STATUS          Status;
  EFI_SHELL_INTERFACE *ShellInterface;

  if (ImageHandle == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Create the Shell Interface for this application
  //
  Status = CreateShellInterfaceForApplication (ImageHandle, &ShellInterface);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "InstallShellInterfaceProtocol: Failed to create interface - %r\n", Status));
    return Status;
  }

  //
  // Install the Shell Interface Protocol on the image handle
  //
  Status = gBS->InstallProtocolInterface (
                  &ImageHandle,
                  &gEfiShellInterfaceGuid,
                  EFI_NATIVE_INTERFACE,
                  ShellInterface
                  );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "InstallShellInterfaceProtocol: Failed to install protocol - %r\n", Status));
    //
    // Cleanup the interface on failure
    //
    UninstallShellInterfaceProtocol (ImageHandle);
    return Status;
  }

  DEBUG ((DEBUG_INFO, "InstallShellInterfaceProtocol: Installed on handle %p\n", ImageHandle));
  return EFI_SUCCESS;
}

/**
  Uninstall Shell Interface Protocol for a specific image

  @param[in] ImageHandle   Image handle to uninstall protocol from

  @retval EFI_SUCCESS     Protocol uninstalled successfully
  @retval Other           Error occurred
**/
EFI_STATUS
EFIAPI
UninstallShellInterfaceProtocol (
  IN EFI_HANDLE ImageHandle
  )
{
  EFI_STATUS               Status;
  EFI_SHELL_INTERFACE      *ShellInterface;
  SHELL_INTERFACE_INSTANCE *Instance;
  UINTN                    Index;
  UINTN                    ArgIndex;

  if (ImageHandle == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Get the Shell Interface Protocol
  //
  Status = gBS->HandleProtocol (
                  ImageHandle,
                  &gEfiShellInterfaceGuid,
                  (VOID**)&ShellInterface
                  );
  if (EFI_ERROR (Status)) {
    return Status;  // Not installed, nothing to do
  }

  //
  // Find the corresponding instance
  //
  Instance = NULL;
  for (Index = 0; Index < ARRAY_SIZE(mShellInterfaces); Index++) {
    if (mShellInterfaces[Index].IsActive && 
        &mShellInterfaces[Index].Interface == ShellInterface) {
      Instance = &mShellInterfaces[Index];
      break;
    }
  }

  if (Instance != NULL) {
    //
    // Cleanup allocated memory
    //
    if (Instance->AllocatedArgv != NULL) {
      for (ArgIndex = 0; ArgIndex < Instance->Interface.Argc; ArgIndex++) {
        if (Instance->AllocatedArgv[ArgIndex] != NULL) {
          FreePool (Instance->AllocatedArgv[ArgIndex]);
        }
      }
      FreePool (Instance->AllocatedArgv);
      Instance->AllocatedArgv = NULL;
    }

    if (Instance->AllocatedArgInfo != NULL) {
      FreePool (Instance->AllocatedArgInfo);
      Instance->AllocatedArgInfo = NULL;
    }

    //
    // Mark instance as inactive
    //
    Instance->IsActive = FALSE;
    if (mActiveInterfaces > 0) {
      mActiveInterfaces--;
    }
  }

  //
  // Uninstall the protocol
  //
  Status = gBS->UninstallProtocolInterface (
                  ImageHandle,
                  &gEfiShellInterfaceGuid,
                  ShellInterface
                  );

  DEBUG ((DEBUG_INFO, "UninstallShellInterfaceProtocol: Uninstalled from handle %p - %r\n", ImageHandle, Status));
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
  EFI_STATUS Status;
  UINTN      Index;

  if (!mLegacyProtocolsInitialized) {
    return EFI_SUCCESS;
  }

  //
  // Cleanup any remaining Shell Interface instances
  //
  for (Index = 0; Index < ARRAY_SIZE(mShellInterfaces); Index++) {
    if (mShellInterfaces[Index].IsActive) {
      //
      // Force cleanup of the instance
      //
      if (mShellInterfaces[Index].AllocatedArgv != NULL) {
        for (UINTN ArgIndex = 0; ArgIndex < mShellInterfaces[Index].Interface.Argc; ArgIndex++) {
          if (mShellInterfaces[Index].AllocatedArgv[ArgIndex] != NULL) {
            FreePool (mShellInterfaces[Index].AllocatedArgv[ArgIndex]);
          }
        }
        FreePool (mShellInterfaces[Index].AllocatedArgv);
      }
      
      if (mShellInterfaces[Index].AllocatedArgInfo != NULL) {
        FreePool (mShellInterfaces[Index].AllocatedArgInfo);
      }
      
      mShellInterfaces[Index].IsActive = FALSE;
    }
  }
  mActiveInterfaces = 0;

  //
  // Uninstall Shell Environment Protocol
  //
  Status = gBS->UninstallProtocolInterface (
                  gImageHandle,
                  &gEfiShellEnvironmentGuid,
                  &mShellEnvironment
                  );

  if (!EFI_ERROR (Status)) {
    mLegacyProtocolsInitialized = FALSE;
  }

  return Status;
}

/**
  Determine if we are running on EFI 1.1

  @retval TRUE     Running on EFI 1.1 
  @retval FALSE    Running on UEFI 2.0+
**/
BOOLEAN
EFIAPI
IsEfi11Environment (
  VOID
  )
{
  //
  // Primary check: System table revision
  // EFI 1.1 systems have revision < 2.00
  //
  if (gST->Hdr.Revision < EFI_2_00_SYSTEM_TABLE_REVISION) {
    DEBUG ((DEBUG_INFO, "IsEfi11Environment: System table revision %d.%02d indicates EFI 1.1\n", 
            (gST->Hdr.Revision >> 16) & 0xFFFF, gST->Hdr.Revision & 0xFFFF));
    return TRUE;
  }

  //
  // For testing purposes, always assume we might need EFI 1.1 support
  // if the old protocols PCD is enabled
  //
  if (PcdGetBool (PcdShellSupportOldProtocols)) {
    DEBUG ((DEBUG_INFO, "IsEfi11Environment: Legacy protocol support enabled\n"));
    return TRUE;
  }

  DEBUG ((DEBUG_INFO, "IsEfi11Environment: System appears to be UEFI 2.0+\n"));
  return FALSE;
}
