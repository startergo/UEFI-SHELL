/** @file
  Legacy EFI 1.1 Shell Protocol definitions for backward compatibility

  This file contains the minimal definitions needed to support EFI 1.1 
  shell environments, using existing EDK2 protocol definitions.

  Copyright (c) 2025, GitHub Copilot. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef _SHELL_LEGACY_PROTOCOLS_H_
#define _SHELL_LEGACY_PROTOCOLS_H_

#include <Uefi.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/EfiShellInterface.h>
#include <Protocol/EfiShellEnvironment2.h>
#include <Protocol/ShellParameters.h>

//
// EFI System Table Revision definitions for compatibility checking
//
#ifndef EFI_2_00_SYSTEM_TABLE_REVISION
#define EFI_2_00_SYSTEM_TABLE_REVISION  ((2<<16) | (0))
#endif

//
// Utility macros
//
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(Array) (sizeof(Array) / sizeof((Array)[0]))
#endif

//
// EFI Shell Environment Protocol GUID (EFI 1.1) 
// This is separate from the Environment2 protocol
//
#define EFI_SHELL_ENVIRONMENT_GUID \
  { \
    0x47c7b220, 0xc42a, 0x11d2, {0x8e, 0x57, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b } \
  }

//
// Forward declaration for EFI_SHELL_ENVIRONMENT (different from EFI_SHELL_ENVIRONMENT2)
//
typedef struct _EFI_SHELL_ENVIRONMENT EFI_SHELL_ENVIRONMENT;

//
// EFI Shell Environment Protocol (EFI 1.1) - enhanced version
//
struct _EFI_SHELL_ENVIRONMENT {
  //
  // Execute a command line
  //
  EFI_STATUS
  (EFIAPI *Execute)(
    IN EFI_HANDLE   *ParentImageHandle,
    IN CHAR16       *CommandLine,
    IN BOOLEAN      Output
    );

  //
  // Get environment variable
  //
  CHAR16 *
  (EFIAPI *GetEnv)(
    IN CHAR16   *Name
    );

  //
  // Get/Set current directory
  //
  CHAR16 *
  (EFIAPI *GetCurDir)(
    IN CHAR16 *FileSystemMapping OPTIONAL
    );

  EFI_STATUS
  (EFIAPI *SetCurDir)(
    IN CHAR16 *FileSystemMapping OPTIONAL,
    IN CHAR16 *Dir
    );

  //
  // Alias management
  //
  CHAR16 *
  (EFIAPI *GetAlias)(
    IN CHAR16   *Name,
    OUT BOOLEAN *Volatile OPTIONAL
    );

  EFI_STATUS
  (EFIAPI *SetAlias)(
    IN CHAR16   *Name,
    IN CHAR16   *Value,
    IN BOOLEAN  Replace,
    IN BOOLEAN  Volatile
    );

  //
  // Reserved for future expansion
  //
  VOID *Reserved[4];
};

//
// Global instances for Shell Interface Protocol management
//
typedef struct {
  EFI_SHELL_INTERFACE Interface;
  CHAR16              **AllocatedArgv;
  EFI_SHELL_ARG_INFO  *AllocatedArgInfo;
  BOOLEAN             IsActive;
} SHELL_INTERFACE_INSTANCE;

//
// Function declarations
//
EFI_STATUS
EFIAPI
InstallShellEnvironmentProtocol (
  VOID
  );

EFI_STATUS
EFIAPI
InitializeLegacyShellSupport (
  VOID
  );

EFI_STATUS
EFIAPI
CleanupLegacyShellSupport (
  VOID
  );

BOOLEAN
EFIAPI
IsEfi11Environment (
  VOID
  );

EFI_STATUS
EFIAPI
InstallShellInterfaceProtocol (
  IN EFI_HANDLE ImageHandle,
  IN CHAR16     **Argv,
  IN UINTN      Argc
  );

EFI_STATUS
EFIAPI
UninstallShellInterfaceProtocol (
  IN EFI_HANDLE ImageHandle
  );

EFI_STATUS
EFIAPI
CreateShellInterfaceForApplication (
  IN  EFI_HANDLE  ImageHandle,
  OUT EFI_SHELL_INTERFACE **ShellInterface
  );

//
// Enhanced Shell Environment functions
//
CHAR16 *
EFIAPI
LegacyShellGetAlias (
  IN CHAR16   *Name,
  OUT BOOLEAN *Volatile OPTIONAL
  );

EFI_STATUS
EFIAPI
LegacyShellSetAlias (
  IN CHAR16   *Name,
  IN CHAR16   *Value,
  IN BOOLEAN  Replace,
  IN BOOLEAN  Volatile
  );

CHAR16 *
EFIAPI
LegacyShellGetCurDir (
  IN CHAR16 *FileSystemMapping OPTIONAL
  );

EFI_STATUS
EFIAPI
LegacyShellSetCurDir (
  IN CHAR16 *FileSystemMapping OPTIONAL,
  IN CHAR16 *Dir
  );

#endif // _SHELL_LEGACY_PROTOCOLS_H_
