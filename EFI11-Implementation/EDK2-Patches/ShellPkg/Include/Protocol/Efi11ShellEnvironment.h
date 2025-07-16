/** @file
  EFI 1.1 Shell Environment Protocol definitions.
  
  This protocol provides environment variable management and other
  shell services for EFI 1.1 applications.
**/

#ifndef __EFI11_SHELL_ENVIRONMENT_H__
#define __EFI11_SHELL_ENVIRONMENT_H__

#define EFI_SHELL_ENVIRONMENT_GUID \
  { 0x47c7b224, 0xc42a, 0x11d2, { 0x8e, 0x57, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b }}

typedef struct _EFI_SHELL_ENVIRONMENT EFI_SHELL_ENVIRONMENT;

/**
  Get environment variable.
  
  @param Name   Variable name
  
  @retval Value of the variable or NULL if not found
**/
typedef
CHAR16 *
(EFIAPI *EFI_SHELL_GET_ENV)(
  IN CHAR16 *Name
  );

/**
  Get current directory.
  
  @param DeviceName   Device name (optional)
  
  @retval Current directory path
**/
typedef
CHAR16 *
(EFIAPI *EFI_SHELL_GET_CUR_DIR)(
  IN CHAR16 *DeviceName OPTIONAL
  );

/**
  Execute a command.
  
  @param ImageHandle    Handle of calling image
  @param CommandLine    Command to execute
  @param Environment   Environment variables
  
  @retval EFI_SUCCESS   Command executed successfully
**/
typedef
EFI_STATUS
(EFIAPI *EFI_SHELL_EXECUTE)(
  IN EFI_HANDLE           ImageHandle,
  IN CHAR16               *CommandLine,
  IN CHAR16               **Environment
  );

/**
  EFI 1.1 Shell Environment Protocol structure.
**/
struct _EFI_SHELL_ENVIRONMENT {
  EFI_SHELL_EXECUTE    Execute;
  EFI_SHELL_GET_ENV    GetEnv;
  EFI_SHELL_GET_CUR_DIR GetCurDir;
};

extern EFI_GUID gEfiShellEnvironmentGuid;

#endif
