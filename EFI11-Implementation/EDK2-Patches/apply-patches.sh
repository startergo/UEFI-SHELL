#!/bin/bash
# Apply EFI 1.1 Legacy Protocol Support Patches to EDK2
# Usage: ./apply-efi11-patches.sh <EDK2_SOURCE_PATH>

set -e

if [ $# -ne 1 ]; then
    echo "Usage: $0 <EDK2_SOURCE_PATH>"
    echo "Example: $0 /path/to/edk2"
    exit 1
fi

EDK2_PATH="$1"
PATCH_DIR="$(dirname "$0")"

# Verify EDK2 path exists and is valid
if [ ! -d "$EDK2_PATH" ]; then
    echo "Error: EDK2 path does not exist: $EDK2_PATH"
    exit 1
fi

if [ ! -f "$EDK2_PATH/ShellPkg/ShellPkg.dec" ]; then
    echo "Error: Invalid EDK2 source tree (ShellPkg not found): $EDK2_PATH"
    exit 1
fi

echo "Applying EFI 1.1 Legacy Protocol Support Patches to EDK2..."
echo "EDK2 Source: $EDK2_PATH"
echo "Patch Source: $PATCH_DIR"

# Create backup directory
BACKUP_DIR="$EDK2_PATH/BACKUP_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$BACKUP_DIR"

echo "Creating backup in: $BACKUP_DIR"

# Backup original files
echo "Backing up original files..."
mkdir -p "$BACKUP_DIR/ShellPkg/Application/Shell"
mkdir -p "$BACKUP_DIR/ShellPkg/Include/Protocol"

cp "$EDK2_PATH/ShellPkg/Application/Shell/Shell.c" "$BACKUP_DIR/ShellPkg/Application/Shell/" 2>/dev/null || true
cp "$EDK2_PATH/ShellPkg/Application/Shell/Shell.h" "$BACKUP_DIR/ShellPkg/Application/Shell/" 2>/dev/null || true
cp "$EDK2_PATH/ShellPkg/Application/Shell/Shell.inf" "$BACKUP_DIR/ShellPkg/Application/Shell/" 2>/dev/null || true
cp "$EDK2_PATH/ShellPkg/ShellPkg.dec" "$BACKUP_DIR/ShellPkg/" 2>/dev/null || true

# Create necessary directories
echo "Creating directories..."
mkdir -p "$EDK2_PATH/ShellPkg/Include/Protocol"

# Copy patch files
echo "Applying patches..."

# Copy modified core files
cp "$PATCH_DIR/ShellPkg/Application/Shell/Shell.c" "$EDK2_PATH/ShellPkg/Application/Shell/"
cp "$PATCH_DIR/ShellPkg/Application/Shell/Shell.h" "$EDK2_PATH/ShellPkg/Application/Shell/"
cp "$PATCH_DIR/ShellPkg/Application/Shell/Shell.inf" "$EDK2_PATH/ShellPkg/Application/Shell/"
cp "$PATCH_DIR/ShellPkg/ShellPkg.dec" "$EDK2_PATH/ShellPkg/"

# Copy EFI 1.1 implementation files
cp "$PATCH_DIR/ShellPkg/Application/Shell/ShellLegacyProtocols.c" "$EDK2_PATH/ShellPkg/Application/Shell/"
cp "$PATCH_DIR/ShellPkg/Application/Shell/ShellLegacyProtocols.h" "$EDK2_PATH/ShellPkg/Application/Shell/"

# Copy protocol headers
cp "$PATCH_DIR/ShellPkg/Include/Protocol/"* "$EDK2_PATH/ShellPkg/Include/Protocol/"

echo "Patches applied successfully!"
echo ""
echo "To enable EFI 1.1 legacy protocol support, add this to your platform DSC file:"
echo ""
echo "[PcdsFixedAtBuild]"
echo "  gEfiShellPkgTokenSpaceGuid.PcdShellSupportOldProtocols|TRUE"
echo ""
echo "Backup of original files saved in: $BACKUP_DIR"
echo ""
echo "You can now rebuild the Shell application with EFI 1.1 support."
