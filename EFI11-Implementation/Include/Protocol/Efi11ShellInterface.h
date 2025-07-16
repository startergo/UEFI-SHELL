/** @file
  EFI 1.1 Shell Interface Protocol definitions.
  
  This protocol provides the interface between the shell and applications
  in EFI 1.1 specification.
**/

#ifndef __EFI11_SHELL_INTERFACE_H__
#define __EFI11_SHELL_INTERFACE_H__

#define EFI_SHELL_INTERFACE_GUID \
  { 0x47c7b223, 0xc42a, 0x11d2, { 0x8e, 0x57, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b }}

typedef struct _EFI_SHELL_INTERFACE EFI_SHELL_INTERFACE;

/**
  EFI 1.1 Shell Interface Protocol structure.
  
  This protocol provides argc/argv style parameter passing that
  EFI 1.1 applications expect.
**/
struct _EFI_SHELL_INTERFACE {
  ///
  /// Handle back to original image handle & image info
  ///
  EFI_HANDLE                ImageHandle;
  EFI_LOADED_IMAGE_PROTOCOL *Info;

  ///
  /// Parsed command line arguments
  ///
  CHAR16                    **Argv;
  UINTN                     Argc;

  ///
  /// Storage for file redirection args after parse
  ///
  CHAR16                    **RedirArgv;
  UINTN                     RedirArgc;

  ///
  /// A file handle for StdIn, StdOut, & StdErr
  ///
  EFI_FILE_PROTOCOL         *StdIn;
  EFI_FILE_PROTOCOL         *StdOut;
  EFI_FILE_PROTOCOL         *StdErr;

  ///
  /// List of attributes for each argument
  ///
  UINT32                    *ArgAttributes;

  ///
  /// Whether we are echoing
  ///
  BOOLEAN                   EchoOn;
};

extern EFI_GUID gEfiShellInterfaceGuid;

#endif
