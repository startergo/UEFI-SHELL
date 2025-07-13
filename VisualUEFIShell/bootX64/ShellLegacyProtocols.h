/** @file
  Legacy EFI 1.1 Shell Protocol definitions for backward compatibility

  This file contains the protocol definitions needed to support EFI 1.1 
  shell environments, including the ShellInterface and ShellEnvironment protocols.

  Copyright (c) 2025, GitHub Copilot. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef _SHELL_LEGACY_PROTOCOLS_H_
#define _SHELL_LEGACY_PROTOCOLS_H_

#include <Uefi.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/LoadedImage.h>

//
// EFI System Table Revision definitions for compatibility checking
//
#ifndef EFI_2_00_SYSTEM_TABLE_REVISION
#define EFI_2_00_SYSTEM_TABLE_REVISION  ((2<<16) | (0))
#endif

//
// EFI Shell Interface Protocol GUID (EFI 1.1)
//
#define EFI_SHELL_INTERFACE_GUID \
  { \
    0x47c7b223, 0xc42a, 0x11d2, {0x8e, 0x57, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b } \
  }

//
// EFI Shell Environment Protocol GUID (EFI 1.1)
//
#define EFI_SHELL_ENVIRONMENT_GUID \
  { \
    0x47c7b221, 0xc42a, 0x11d2, {0x8e, 0x57, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b } \
  }

//
// EFI Shell Environment2 Protocol GUID (EFI 1.1)
//
#define EFI_SHELL_ENVIRONMENT2_GUID \
  { \
    0x47c7b221, 0xc42a, 0x11d2, {0x8e, 0x57, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b } \
  }

typedef struct _EFI_SHELL_INTERFACE EFI_SHELL_INTERFACE;
typedef struct _EFI_SHELL_ENVIRONMENT EFI_SHELL_ENVIRONMENT;
typedef struct _EFI_SHELL_ENVIRONMENT2 EFI_SHELL_ENVIRONMENT2;

//
// Shell Interface Protocol (EFI 1.1)
//
typedef struct _EFI_SHELL_INTERFACE {
  EFI_HANDLE                ImageHandle;
  EFI_LOADED_IMAGE_PROTOCOL *Info;
  CHAR16                    **Argv;
  UINTN                     Argc;
  CHAR16                    **RedirArgv;
  UINTN                     RedirArgc;
  EFI_FILE_PROTOCOL         *StdIn;
  EFI_FILE_PROTOCOL         *StdOut;
  EFI_FILE_PROTOCOL         *StdErr;
} EFI_SHELL_INTERFACE;

//
// Legacy file information structure
//
typedef struct {
  UINT32    Signature;
  EFI_LIST_ENTRY    Link;        // List of shell files
  EFI_STATUS        Status;
  CHAR16            *FullName;
  CHAR16            *FileName;
  EFI_FILE_HANDLE   Handle;
  EFI_FILE_INFO     *Info;
} SHELL_FILE_ARG;

//
// Legacy shell arguments structure
//
typedef struct _SHELL_FILE_ARG_SIGNATURE {
  EFI_LIST_ENTRY  Link;
  UINTN           Argc;
  CHAR16          **Argv;
  SHELL_FILE_ARG  *ArgList;
} SHELL_FILE_ARG_SIGNATURE;

//
// Function prototypes for Shell Environment Protocol
//
typedef
EFI_STATUS
(EFIAPI *SHELL_EXECUTE)(
  IN EFI_HANDLE                 *ParentImageHandle,
  IN CHAR16                     *CommandLine,
  IN BOOLEAN                    Output
  );

typedef
VOID
(EFIAPI *SHELL_GET_ENV)(
  IN CHAR16                     *Name,
  OUT CHAR16                    **Value
  );

typedef
CHAR16*
(EFIAPI *SHELL_GET_ENV_VALUE)(
  IN CHAR16                     *Name
  );

typedef
EFI_STATUS
(EFIAPI *SHELL_SET_ENV)(
  IN CHAR16                     *Name,
  IN CHAR16                     *Value
  );

typedef
CHAR16*
(EFIAPI *SHELL_GET_ALIAS)(
  IN CHAR16                     *Name,
  OUT BOOLEAN                   *Volatile OPTIONAL
  );

typedef
EFI_STATUS
(EFIAPI *SHELL_SET_ALIAS)(
  IN CHAR16                     *Name,
  IN CHAR16                     *Value,
  IN BOOLEAN                    Replace,
  IN BOOLEAN                    Volatile
  );

typedef
VOID
(EFIAPI *SHELL_GET_CUR_DIR)(
  OUT CHAR16                    **Dir
  );

//
// EFI Shell Environment Protocol (EFI 1.1)
//
typedef struct _EFI_SHELL_ENVIRONMENT {
  SHELL_EXECUTE                 Execute;
  SHELL_GET_ENV_VALUE           GetEnv;
  SHELL_SET_ENV                 SetEnv;
  SHELL_GET_ALIAS               GetAlias;
  SHELL_SET_ALIAS               SetAlias;
  SHELL_GET_CUR_DIR             GetCurDir;
} EFI_SHELL_ENVIRONMENT;

//
// Extended Shell Environment Protocol (EFI 1.1+)
//
typedef struct _EFI_SHELL_ENVIRONMENT2 {
  SHELL_EXECUTE                 Execute;
  SHELL_GET_ENV_VALUE           GetEnv;
  SHELL_SET_ENV                 SetEnv;
  SHELL_GET_ALIAS               GetAlias;
  SHELL_SET_ALIAS               SetAlias;
  SHELL_GET_CUR_DIR             GetCurDir;
  // Extended functions would go here
} EFI_SHELL_ENVIRONMENT2;

//
// Global GUIDs
//
extern EFI_GUID gEfiShellInterfaceGuid;
extern EFI_GUID gEfiShellEnvironmentGuid;
extern EFI_GUID gEfiShellEnvironment2Guid;

//
// Function prototypes for legacy protocol implementation
//
EFI_STATUS
EFIAPI
InitializeLegacyShellSupport (
  VOID
  );

EFI_STATUS
EFIAPI
InstallShellInterfaceProtocol (
  IN EFI_HANDLE                 ImageHandle,
  IN CHAR16                     **Argv,
  IN UINTN                      Argc
  );

EFI_STATUS
EFIAPI
InstallShellEnvironmentProtocol (
  VOID
  );

EFI_STATUS
EFIAPI
CleanupLegacyShellSupport (
  VOID
  );

//
// Legacy protocol detection
//
BOOLEAN
EFIAPI
IsEfi11Environment (
  VOID
  );

#endif // _SHELL_LEGACY_PROTOCOLS_H_
