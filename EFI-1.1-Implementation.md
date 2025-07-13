# EFI 1.1 Shell Protocol Support Implementation

This document describes the implementation of EFI 1.1 shell protocol support in the UEFI Shell to provide backward compatibility with older EFI shell applications, including support for systems like the 2010 Mac Pro (cMP5,1).

## Overview

The EFI 1.1 support implementation adds the following legacy protocols:

1. **EFI_SHELL_INTERFACE_PROTOCOL** - Provides command-line arguments and standard I/O handles
2. **EFI_SHELL_ENVIRONMENT_PROTOCOL** - Provides shell environment services
3. **EFI_SHELL_ENVIRONMENT2_PROTOCOL** - Extended shell environment services

## Key Components

### Files Added/Modified

- `ShellLegacyProtocols.h` - Protocol definitions and structures for EFI 1.1 compatibility
- `ShellLegacyProtocols.c` - Implementation of legacy protocol wrappers
- `Shell.c` - Modified to enable legacy support and remove blocking checks
- `ShellProtocol.c` - Modified to install legacy protocols on shell applications
- `ShellLegacyTest.c` - Test program to verify EFI 1.1 protocol functionality

### Key Changes Made

#### 1. Removed EFI_UNSUPPORTED Block
**File**: `Shell.c`
**Change**: Removed the check that returned `EFI_UNSUPPORTED` when legacy protocols were enabled
**Impact**: Allows the shell to run with legacy protocol support enabled

#### 2. Added Legacy Protocol Initialization
**File**: `Shell.c`
**Change**: Added calls to `InstallShellEnvironmentProtocol()` during shell startup
**Impact**: Makes legacy protocols available to EFI 1.1 applications

#### 3. Implemented Framework HII Support
**File**: `Shell.c`
**Change**: Added fallback for HII package installation in EFI 1.1 mode
**Impact**: Allows shell to operate without modern HII requirements

#### 4. Added ShellInterface Protocol Installation
**File**: `ShellProtocol.c`
**Change**: Implemented the TODO for installing ShellInterface protocol on each shell application
**Impact**: EFI 1.1 applications receive proper command-line arguments and I/O handles

#### 5. Added ShellEnvironment Protocol Installation
**File**: `ShellProtocol.c`
**Change**: Implemented the TODO for ShellEnvironment and ShellEnvironment2 protocols
**Impact**: Legacy applications can access shell environment services

## Protocol Implementations

### EFI_SHELL_INTERFACE_PROTOCOL

Provides per-application interfaces with:
- Command-line arguments (Argc/Argv)
- Standard I/O handles (StdIn/StdOut/StdErr)
- Loaded image information
- Redirection support (basic)

### EFI_SHELL_ENVIRONMENT_PROTOCOL

Provides shell services:
- `Execute()` - Execute shell commands
- `GetEnv()` - Get environment variables
- `SetEnv()` - Set environment variables
- `GetAlias()` - Get command aliases
- `SetAlias()` - Set command aliases
- `GetCurDir()` - Get current directory

### Legacy Protocol Wrappers

All legacy protocols are implemented as wrappers around the modern UEFI 2.0+ shell protocols:

```c
// Example: Legacy GetEnv wraps modern EfiShellGetEnv
CHAR16* LegacyShellGetEnvValue(IN CHAR16 *Name) {
    CONST CHAR16 *EnvValue = EfiShellGetEnv(Name);
    if (EnvValue != NULL) {
        return AllocateCopyPool(StrSize(EnvValue), (VOID*)EnvValue);
    }
    return NULL;
}
```

## EFI 1.1 Environment Detection

The implementation includes automatic detection of EFI 1.1 environments:

```c
BOOLEAN IsEfi11Environment(VOID) {
    // Check system table revision
    if (gST->Hdr.Revision < EFI_2_00_SYSTEM_TABLE_REVISION) {
        return TRUE;
    }
    
    // Check for missing UEFI 2.0+ protocols
    EFI_STATUS Status = gBS->LocateProtocol(&gEfiShellProtocolGuid, NULL, &Interface);
    if (EFI_ERROR(Status)) {
        return TRUE;  // Likely EFI 1.1
    }
    
    return FALSE;
}
```

## Configuration

### PCDs (Platform Configuration Database)

- `PcdShellSupportOldProtocols` - Enable legacy protocol support (set to TRUE)
- `PcdShellRequireHiiPlatform` - Require HII platform (set to FALSE for EFI 1.1)
- `PcdShellSupportFrameworkHii` - Enable Framework HII support (set to TRUE for EFI 1.1)

### Build Configuration

To enable EFI 1.1 support, ensure these PCDs are set in your platform's DSC file:

```
[PcdsFixedAtBuild]
gEfiShellPkgTokenSpaceGuid.PcdShellSupportOldProtocols|TRUE

[PcdsFeatureFlag]
gEfiShellPkgTokenSpaceGuid.PcdShellRequireHiiPlatform|FALSE
gEfiShellPkgTokenSpaceGuid.PcdShellSupportFrameworkHii|TRUE
```

**Note**: `PcdShellSupportOldProtocols` must be in `[PcdsFixedAtBuild]` section because it's defined as a Dynamic PCD in the ShellPkg.dec file, not as a FeatureFlag PCD.

## Testing

### Using the Test Application

The `ShellLegacyTest.c` program can be compiled and run to verify EFI 1.1 protocol support:

```
Shell> ShellLegacyTest.efi
EFI 1.1 Shell Protocol Test
===========================

EFI 1.1 Environment Detected: YES
System Table Revision: 0x00020000
UEFI Shell Protocol: FOUND
EFI 1.1 ShellInterface Protocol: FOUND
  Argc: 1
  Argv[0]: ShellLegacyTest.efi
EFI 1.1 ShellEnvironment Protocol: FOUND
  PATH environment variable: ACCESSIBLE
EFI 1.1 ShellEnvironment2 Protocol: FOUND

Test completed.
```

### Manual Testing

1. Load an EFI 1.1 shell application
2. Verify it receives proper command-line arguments
3. Test environment variable access
4. Verify I/O redirection works
5. Test shell command execution from within the application

## Compatibility Notes

### 2010 Mac Pro (cMP5,1) Compatibility

This implementation should work on 2010 Mac Pro systems because:

1. **EFI 1.10 Support**: The implementation detects and supports pre-UEFI 2.0 environments
2. **Reduced Dependencies**: Framework HII support reduces dependency on modern HII infrastructure
3. **Legacy Protocol Bridges**: All EFI 1.1 protocols are properly bridged to modern implementations
4. **Graceful Degradation**: The shell can operate with reduced functionality when modern features aren't available

### Known Limitations

1. **Framework HII**: Currently uses a minimal implementation - some HII-dependent features may not work
2. **Advanced I/O**: Some advanced redirection features may not be fully compatible
3. **Performance**: Legacy protocol wrappers add small overhead
4. **Memory Management**: Legacy applications are responsible for freeing returned strings

## Future Enhancements

1. **Complete Framework HII**: Implement full Framework HII protocol support
2. **Enhanced I/O Redirection**: Improve redirection compatibility
3. **Legacy Command Support**: Add support for EFI 1.1-specific shell commands
4. **Improved Detection**: Better detection of EFI 1.1 vs UEFI 2.0+ environments

## Debugging

Enable debug output to see legacy protocol installation:

```
DEBUG ((DEBUG_INFO, "Shell: Installing EFI 1.1 legacy protocol support\n"));
```

This will help verify that legacy protocols are being installed correctly during shell startup.

## Conclusion

This implementation provides comprehensive EFI 1.1 backward compatibility while maintaining full UEFI 2.0+ functionality. The wrapper approach ensures that legacy applications can run without modification while benefiting from the modern shell's enhanced capabilities.
