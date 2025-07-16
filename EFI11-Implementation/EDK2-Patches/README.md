## EDK2 EFI 1.1 Legacy Protocol Support Patches

This directory contains all the modified EDK2 files needed to add EFI 1.1 legacy protocol support to the UEFI Shell for backward compatibility with older shell applications designed for EFI 1.1.

### Overview

The patches enable legacy EFI 1.1 shell protocols while maintaining full UEFI 2.x compatibility. This allows older shell.efi binaries designed for EFI 1.1 systems (like Mac Pro 5,1) to run on modern UEFI implementations.

### Files Modified

#### ShellPkg/Application/Shell/
- **Shell.c** - Added EFI 1.1 legacy protocol initialization
- **Shell.h** - Added include for ShellLegacyProtocols.h
- **Shell.inf** - Added EFI 1.1 source files to build
- **ShellLegacyProtocols.c** - EFI 1.1 protocol implementation
- **ShellLegacyProtocols.h** - EFI 1.1 protocol definitions

#### ShellPkg/Include/Protocol/
- **Efi11ShellEnvironment.h** - EFI 1.1 Shell Environment protocol definition
- **Efi11ShellInterface.h** - EFI 1.1 Shell Interface protocol definition

#### ShellPkg/
- **ShellPkg.dec** - Added EFI 1.1 protocol GUID declarations

### Key Changes

#### 1. Shell.c Modifications

```c
// Added after ShellInitialize() call:
if (PcdGetBool (PcdShellSupportOldProtocols)) {
  Status = InitializeShellLegacyProtocols ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "Warning: Failed to initialize EFI 1.1 legacy protocols: %r\n", Status));
  } else {
    DEBUG ((DEBUG_INFO, "EFI 1.1 legacy protocols initialized successfully\n"));
  }
}
```

#### 2. Shell.h Modifications

```c
// Added include:
#include "ShellLegacyProtocols.h"
```

#### 3. Shell.inf Modifications

```ini
[Sources]
  ...existing files...
  ShellLegacyProtocols.c
  ShellLegacyProtocols.h
```

#### 4. ShellPkg.dec Modifications

```ini
[Protocols]
  gEfiShellEnvironment2Guid           = {0x47c7b221, 0xc42a, 0x11d2, {0x8e, 0x57, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}
  gEfiShellInterfaceGuid              = {0x47c7b223, 0xc42a, 0x11d2, {0x8e, 0x57, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}
  gEfiShellEnvironmentGuid            = {0x47c7b220, 0xc42a, 0x11d2, {0x8e, 0x57, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}
```

### Protocol Implementation

The EFI 1.1 legacy protocols provide:

1. **EFI_SHELL_ENVIRONMENT** - Core shell environment services
2. **EFI_SHELL_INTERFACE** - Per-application shell interface

### Legacy Functions Supported

- `Execute()` - Execute shell commands
- `GetEnv()` / `SetEnv()` - Environment variable access
- `GetCurDir()` / `SetCurDir()` - Current directory management
- `IsRootShell()` - Root shell detection
- `CloseConsoleProxy()` - Console management

### Configuration

Enable EFI 1.1 support by setting the PCD:

```ini
[PcdsFixedAtBuild]
  gEfiShellPkgTokenSpaceGuid.PcdShellSupportOldProtocols|TRUE
```

### Target Compatibility

This implementation specifically targets:
- Mac Pro 5,1 (2010-2012) firmware compatibility
- EFI 1.1 shell applications
- Legacy shell scripts and utilities
- Older UEFI implementations requiring EFI 1.1 protocols

### Installation

1. Copy all files to their corresponding locations in your EDK2 source tree
2. Set `PcdShellSupportOldProtocols` to `TRUE` in your platform DSC
3. Rebuild the Shell application

### Testing

The implementation includes a test framework in `ShellLegacyTest.c` for validating protocol functionality.

### Future Support

These patches provide a foundation for:
- Additional EFI 1.1 protocol implementations
- Extended backward compatibility features
- Legacy application support enhancements

For more details, see the main implementation documentation in the parent directory.
