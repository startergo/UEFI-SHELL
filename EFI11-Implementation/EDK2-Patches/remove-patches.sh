#!/bin/bash
# Remove EFI 1.1 Legacy Protocol Support Patches from EDK2
# Usage: ./remove-efi11-patches.sh <EDK2_SOURCE_PATH> [BACKUP_DIR]

set -e

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
    echo "Usage: $0 <EDK2_SOURCE_PATH> [BACKUP_DIR]"
    echo "Example: $0 /path/to/edk2"
    echo "Example: $0 /path/to/edk2 /path/to/backup"
    exit 1
fi

EDK2_PATH="$1"
BACKUP_DIR="$2"

# Verify EDK2 path exists and is valid
if [ ! -d "$EDK2_PATH" ]; then
    echo "Error: EDK2 path does not exist: $EDK2_PATH"
    exit 1
fi

if [ ! -f "$EDK2_PATH/ShellPkg/ShellPkg.dec" ]; then
    echo "Error: Invalid EDK2 source tree (ShellPkg not found): $EDK2_PATH"
    exit 1
fi

echo "Removing EFI 1.1 Legacy Protocol Support Patches from EDK2..."
echo "EDK2 Source: $EDK2_PATH"

# If backup directory specified, restore from it
if [ -n "$BACKUP_DIR" ] && [ -d "$BACKUP_DIR" ]; then
    echo "Restoring from backup: $BACKUP_DIR"
    
    # Restore original files
    if [ -f "$BACKUP_DIR/ShellPkg/Application/Shell/Shell.c" ]; then
        cp "$BACKUP_DIR/ShellPkg/Application/Shell/Shell.c" "$EDK2_PATH/ShellPkg/Application/Shell/"
        echo "Restored Shell.c"
    fi
    
    if [ -f "$BACKUP_DIR/ShellPkg/Application/Shell/Shell.h" ]; then
        cp "$BACKUP_DIR/ShellPkg/Application/Shell/Shell.h" "$EDK2_PATH/ShellPkg/Application/Shell/"
        echo "Restored Shell.h"
    fi
    
    if [ -f "$BACKUP_DIR/ShellPkg/Application/Shell/Shell.inf" ]; then
        cp "$BACKUP_DIR/ShellPkg/Application/Shell/Shell.inf" "$EDK2_PATH/ShellPkg/Application/Shell/"
        echo "Restored Shell.inf"
    fi
    
    if [ -f "$BACKUP_DIR/ShellPkg/ShellPkg.dec" ]; then
        cp "$BACKUP_DIR/ShellPkg/ShellPkg.dec" "$EDK2_PATH/ShellPkg/"
        echo "Restored ShellPkg.dec"
    fi
fi

# Remove EFI 1.1 implementation files
echo "Removing EFI 1.1 implementation files..."
rm -f "$EDK2_PATH/ShellPkg/Application/Shell/ShellLegacyProtocols.c"
rm -f "$EDK2_PATH/ShellPkg/Application/Shell/ShellLegacyProtocols.h"

# Remove protocol headers
rm -f "$EDK2_PATH/ShellPkg/Include/Protocol/Efi11ShellEnvironment.h"
rm -f "$EDK2_PATH/ShellPkg/Include/Protocol/Efi11ShellInterface.h"

# Remove empty protocol directory if it exists and is empty
if [ -d "$EDK2_PATH/ShellPkg/Include/Protocol" ]; then
    rmdir "$EDK2_PATH/ShellPkg/Include/Protocol" 2>/dev/null || true
    if [ -d "$EDK2_PATH/ShellPkg/Include" ]; then
        rmdir "$EDK2_PATH/ShellPkg/Include" 2>/dev/null || true
    fi
fi

echo "EFI 1.1 patches removed successfully!"
echo ""
if [ -z "$BACKUP_DIR" ]; then
    echo "WARNING: No backup directory specified. Original files were not restored."
    echo "You may need to restore the original EDK2 files manually or from git."
else
    echo "Original files restored from backup."
fi
echo ""
echo "Remember to remove the PCD setting from your platform DSC file:"
echo "  gEfiShellPkgTokenSpaceGuid.PcdShellSupportOldProtocols|TRUE"
