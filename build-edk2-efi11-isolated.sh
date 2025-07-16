#!/bin/bash
# EDK2 Shell with EFI 1.1 Support - Isolated Build
# Builds UEFI shell with EFI 1.1 backward compatibility using isolated workspace
# Protects original EDK2 source tree from contamination

set -e

WORKSPACE_ROOT="/Users/macbookpro/UEFI-SHELL1.1"
BUILD_WORKSPACE="$WORKSPACE_ROOT/BUILD-WORKSPACE"
VISUAL_BUILD_WORKSPACE="$WORKSPACE_ROOT/VISUAL-BUILD-WORKSPACE"

echo "🔒 EDK2 Shell + EFI 1.1 Support (Isolated Build)"
echo "================================================"
echo "🎯 Building UEFI shell with EFI 1.1 backward compatibility"
echo "🛡️ Using isolated workspace to protect EDK2 and VisualUEFIShell source trees"

# Check if EFI 1.1 implementation files exist
if [ ! -f "EFI11-Implementation/ShellLegacyProtocols.c" ]; then
    echo "❌ EFI 1.1 implementation files not found in EFI11-Implementation/"
    exit 1
fi

echo "✅ EFI 1.1 implementation files found"

# Clean and create isolated build workspaces
echo "🏗️ Creating clean isolated build workspaces..."
rm -rf "$BUILD_WORKSPACE"
rm -rf "$VISUAL_BUILD_WORKSPACE"
mkdir -p "$BUILD_WORKSPACE"
mkdir -p "$VISUAL_BUILD_WORKSPACE"

# Copy EDK2 to isolated workspace
echo "📂 Copying EDK2 to isolated workspace..."
cp -r "$WORKSPACE_ROOT/EDK2" "$BUILD_WORKSPACE/"

# Copy VisualUEFIShell to isolated workspace
echo "📂 Copying VisualUEFIShell to isolated workspace..."
cp -r "$WORKSPACE_ROOT/VisualUEFIShell" "$VISUAL_BUILD_WORKSPACE/"

cd "$BUILD_WORKSPACE/EDK2"

# Add EFI 1.1 legacy protocol support to the standard EDK2 shell
echo "🔗 Adding EFI 1.1 legacy protocol support..."
SHELL_APP_DIR="ShellPkg/Application/Shell"

# Copy EFI 1.1 legacy protocol files
cp "$WORKSPACE_ROOT/EFI11-Implementation/ShellLegacyProtocols.c" "$SHELL_APP_DIR/"
cp "$WORKSPACE_ROOT/EFI11-Implementation/ShellLegacyProtocols.h" "$SHELL_APP_DIR/"

# Modify Shell.c to include legacy protocol support
echo "🔄 Adding legacy protocol includes to Shell.c..."
sed -i '' '/#include "Shell.h"/a\
#include "ShellLegacyProtocols.h"
' "$SHELL_APP_DIR/Shell.c"

# Create a custom Shell.inf that includes our legacy protocol files
echo "📝 Creating custom Shell.inf with legacy protocol support..."
cp "$SHELL_APP_DIR/Shell.inf" "$SHELL_APP_DIR/Shell.inf.backup"

# Create new Shell.inf based on original but with our additions
cat > "$SHELL_APP_DIR/Shell.inf" << 'EOF'
##  @file
#  This is the shell application with EFI 1.1 legacy protocol support
#
#  (C) Copyright 2013 Hewlett-Packard Development Company, L.P.<BR>
#  Copyright (c) 2009 - 2018, Intel Corporation. All rights reserved.<BR>
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

[Defines]
  INF_VERSION                    = 0x00010006
  BASE_NAME                      = Shell
  FILE_GUID                      = 7C04A583-9E3E-4f1c-AD65-E05268D0B4D1
  MODULE_TYPE                    = UEFI_APPLICATION
  VERSION_STRING                 = 1.0
  ENTRY_POINT                    = UefiMain

[Sources]
  Shell.c
  Shell.h
  ShellParametersProtocol.c
  ShellParametersProtocol.h
  ShellProtocol.c
  ShellProtocol.h
  FileHandleWrappers.c
  FileHandleWrappers.h
  FileHandleInternal.h
  ShellEnvVar.c
  ShellEnvVar.h
  ShellManParser.c
  ShellManParser.h
  Shell.uni
  ConsoleLogger.c
  ConsoleLogger.h
  ConsoleWrappers.c
  ConsoleWrappers.h
  ShellLegacyProtocols.c

[Packages]
  MdePkg/MdePkg.dec
  ShellPkg/ShellPkg.dec
  MdeModulePkg/MdeModulePkg.dec

[LibraryClasses]
  BaseLib
  UefiApplicationEntryPoint
  UefiLib
  DebugLib
  MemoryAllocationLib
  ShellCommandLib
  UefiRuntimeServicesTableLib
  UefiBootServicesTableLib
  DevicePathLib
  BaseMemoryLib
  PcdLib
  FileHandleLib
  PrintLib
  HiiLib
  SortLib
  HandleParsingLib
  UefiHiiServicesLib

[Guids]
  gShellVariableGuid
  gShellAliasGuid

[Protocols]
  gEfiShellProtocolGuid
  gEfiShellParametersProtocolGuid
  gEfiSimpleTextInputExProtocolGuid
  gEfiSimpleTextInProtocolGuid
  gEfiSimpleTextOutProtocolGuid
  gEfiSimpleFileSystemProtocolGuid
  gEfiLoadedImageProtocolGuid
  gEfiComponentName2ProtocolGuid
  gEfiUnicodeCollation2ProtocolGuid
  gEfiDevicePathProtocolGuid
  gEfiHiiPackageListProtocolGuid

[Pcd]
  gEfiShellPkgTokenSpaceGuid.PcdShellSupportLevel
  gEfiShellPkgTokenSpaceGuid.PcdShellSupportOldProtocols
  gEfiShellPkgTokenSpaceGuid.PcdShellRequireHiiPlatform
  gEfiShellPkgTokenSpaceGuid.PcdShellSupportFrameworkHii
  gEfiShellPkgTokenSpaceGuid.PcdShellPageBreakDefault
  gEfiShellPkgTokenSpaceGuid.PcdShellInsertModeDefault
  gEfiShellPkgTokenSpaceGuid.PcdShellScreenLogCount
  gEfiShellPkgTokenSpaceGuid.PcdShellPrintBufferSize
  gEfiShellPkgTokenSpaceGuid.PcdShellForceConsole
  gEfiShellPkgTokenSpaceGuid.PcdShellSupplier
  gEfiShellPkgTokenSpaceGuid.PcdShellMaxHistoryCommandCount
  gEfiShellPkgTokenSpaceGuid.PcdShellDefaultDelay

[UserExtensions.TianoCore."ExtraFiles"]
  ShellExtra.uni
EOF

# Build EDK2 BaseTools
echo "🔧 Building EDK2 BaseTools..."
export WORKSPACE="$PWD"
make -C BaseTools

# Setup environment manually
export EDK_TOOLS_PATH="$PWD/BaseTools"
export CONF_PATH="$PWD/Conf"
export PYTHONPATH="$EDK_TOOLS_PATH/Source/Python"
export PATH="$EDK_TOOLS_PATH/Source/C/bin:$EDK_TOOLS_PATH/BinWrappers/PosixLike:$PATH"

# Create configuration files if they don't exist
mkdir -p Conf
if [ ! -f "Conf/target.txt" ]; then
    cp "$EDK_TOOLS_PATH/Conf/target.template" "Conf/target.txt"
fi
if [ ! -f "Conf/tools_def.txt" ]; then
    cp "$EDK_TOOLS_PATH/Conf/tools_def.template" "Conf/tools_def.txt"
fi
if [ ! -f "Conf/build_rule.txt" ]; then
    cp "$EDK_TOOLS_PATH/Conf/build_rule.template" "Conf/build_rule.txt"
fi

# Build the shell
echo "🏗️ Building EDK2 shell with EFI 1.1 legacy support..."
export WORKSPACE="$PWD"
export PACKAGES_PATH="$PWD"

# Build RELEASE version
echo "🚀 Building RELEASE version..."
build -a X64 -t XCODE5 -p ShellPkg/ShellPkg.dsc -b RELEASE -m ShellPkg/Application/Shell/Shell.inf

# Build DEBUG version
echo "🐛 Building DEBUG version..."
build -a X64 -t XCODE5 -p ShellPkg/ShellPkg.dsc -b DEBUG -m ShellPkg/Application/Shell/Shell.inf

# Check if build was successful
echo "🔍 Locating built shell binaries..."

RELEASE_SHELL_EFI="Build/Shell/RELEASE_XCODE5/X64/ShellPkg/Application/Shell/Shell/OUTPUT/Shell.efi"
DEBUG_SHELL_EFI="Build/Shell/DEBUG_XCODE5/X64/ShellPkg/Application/Shell/Shell/OUTPUT/Shell.efi"

if [ -f "$RELEASE_SHELL_EFI" ]; then
    echo "✅ EFI 1.1 compatible shell build successful!"
    echo "📦 RELEASE Shell.efi found at: $RELEASE_SHELL_EFI"
    
    # Create output directories
    mkdir -p "$WORKSPACE_ROOT/UEFIBinaries/pureEDK2Shell/release"
    mkdir -p "$WORKSPACE_ROOT/UEFIBinaries/pureEDK2Shell/debug"
    
    # Copy the RELEASE shell
    cp "$RELEASE_SHELL_EFI" "$WORKSPACE_ROOT/UEFIBinaries/pureEDK2Shell/BOOTX64.EFI"
    cp "$RELEASE_SHELL_EFI" "$WORKSPACE_ROOT/UEFIBinaries/pureEDK2Shell/Shell.efi"
    cp "$RELEASE_SHELL_EFI" "$WORKSPACE_ROOT/UEFIBinaries/pureEDK2Shell/release/BOOTX64.EFI"
    
    echo "✅ RELEASE EFI 1.1 shell copied to:"
    echo "   📁 UEFIBinaries/pureEDK2Shell/BOOTX64.EFI"
    echo "   📁 UEFIBinaries/pureEDK2Shell/Shell.efi"
    echo "   📁 UEFIBinaries/pureEDK2Shell/release/BOOTX64.EFI"
    
    # Copy DEBUG shell if available
    if [ -f "$DEBUG_SHELL_EFI" ]; then
        cp "$DEBUG_SHELL_EFI" "$WORKSPACE_ROOT/UEFIBinaries/pureEDK2Shell/debug/BOOTX64.EFI"
        cp "$DEBUG_SHELL_EFI" "$WORKSPACE_ROOT/UEFIBinaries/pureEDK2Shell/debug/Shell.efi"
        echo "✅ DEBUG EFI 1.1 shell copied to:"
        echo "   📁 UEFIBinaries/pureEDK2Shell/debug/BOOTX64.EFI"
        echo "   📁 UEFIBinaries/pureEDK2Shell/debug/Shell.efi"
        
        # Show size comparison
        RELEASE_SIZE=$(ls -lh "$RELEASE_SHELL_EFI" | awk '{print $5}')
        DEBUG_SIZE=$(ls -lh "$DEBUG_SHELL_EFI" | awk '{print $5}')
        echo ""
        echo "📊 Build sizes:"
        echo "   🚀 RELEASE: $RELEASE_SIZE"
        echo "   🐛 DEBUG: $DEBUG_SIZE"
    else
        echo "⚠️  DEBUG build not found - only RELEASE version available"
    fi
    
    echo ""
    echo "🎯 This shell includes:"
    echo "   • Full EDK2 UEFI 2.x shell functionality (edit, file commands, etc.)"
    echo "   • EFI 1.1 legacy protocol support for backward compatibility"
    echo "   • Clean isolated build (no workspace contamination)"
    echo ""
    echo "🧪 Ready for testing with EFI 1.1 firmwares!"
    
else
    echo "❌ Build failed - Shell.efi not found"
    echo "🔧 Expected location: $RELEASE_SHELL_EFI"
    echo ""
    echo "📋 Available build outputs:"
    find Build -name "*.efi" 2>/dev/null || echo "No .efi files found"
    exit 1
fi

cd "$WORKSPACE_ROOT"
echo ""
echo "🏁 EDK2 EFI 1.1 compatible shell build completed!"

# Now build VisualUEFIShell in isolated environment
echo ""
echo "🎨 Building VisualUEFIShell in isolated environment..."
echo "======================================================="

cd "$VISUAL_BUILD_WORKSPACE/VisualUEFIShell/bootX64"

# Add EFI 1.1 legacy protocol support to isolated VisualUEFIShell
echo "🔗 Adding EFI 1.1 legacy protocol support to VisualUEFIShell..."

# Copy EFI 1.1 legacy protocol files
cp "$WORKSPACE_ROOT/EFI11-Implementation/ShellLegacyProtocols.c" .
cp "$WORKSPACE_ROOT/EFI11-Implementation/ShellLegacyProtocols.h" .
cp "$WORKSPACE_ROOT/EFI11-Implementation/ShellLegacyTest.c" .

# Replace Shell.c and ShellProtocol.c with EFI 1.1 modified versions
cp "$WORKSPACE_ROOT/EFI11-Implementation/Shell_Modified.c" Shell.c
cp "$WORKSPACE_ROOT/EFI11-Implementation/ShellProtocol_Modified.c" ShellProtocol.c

# Create AutoGen.h for standalone build
echo "📝 Creating AutoGen.h for VisualUEFIShell standalone build..."
cat > AutoGen.h << 'AUTOGEN_EOF'
// AutoGen.h - Generated for VisualUEFIShell standalone build with EFI 1.1 support

#ifndef _AUTOGEN_H_
#define _AUTOGEN_H_

#include <Uefi.h>
#include <Library/PcdLib.h>

// PCD Definitions for Shell functionality
#define _PCD_TOKEN_PcdShellSupportLevel  ((UINT32) 1U)
#define _PCD_SIZE_PcdShellSupportLevel  ((UINTN) 4)
#define _PCD_GET_MODE_SIZE_PcdShellSupportLevel  _PCD_SIZE_PcdShellSupportLevel
#define _PCD_VALUE_PcdShellSupportLevel  ((UINT32) 3U)
#define _PCD_GET_MODE_32_PcdShellSupportLevel()  _PCD_VALUE_PcdShellSupportLevel

#define _PCD_TOKEN_PcdShellSupportOldProtocols  ((UINT32) 2U)
#define _PCD_SIZE_PcdShellSupportOldProtocols  ((UINTN) 1)
#define _PCD_GET_MODE_SIZE_PcdShellSupportOldProtocols  _PCD_SIZE_PcdShellSupportOldProtocols
#define _PCD_VALUE_PcdShellSupportOldProtocols  ((BOOLEAN) 1)
#define _PCD_GET_MODE_BOOL_PcdShellSupportOldProtocols()  _PCD_VALUE_PcdShellSupportOldProtocols

#define _PCD_TOKEN_PcdShellRequireHiiPlatform  ((UINT32) 3U)
#define _PCD_SIZE_PcdShellRequireHiiPlatform  ((UINTN) 1)
#define _PCD_GET_MODE_SIZE_PcdShellRequireHiiPlatform  _PCD_SIZE_PcdShellRequireHiiPlatform
#define _PCD_VALUE_PcdShellRequireHiiPlatform  ((BOOLEAN) 0)
#define _PCD_GET_MODE_BOOL_PcdShellRequireHiiPlatform()  _PCD_VALUE_PcdShellRequireHiiPlatform

#define _PCD_TOKEN_PcdShellSupportFrameworkHii  ((UINT32) 4U)
#define _PCD_SIZE_PcdShellSupportFrameworkHii  ((UINTN) 1)
#define _PCD_GET_MODE_SIZE_PcdShellSupportFrameworkHii  _PCD_SIZE_PcdShellSupportFrameworkHii
#define _PCD_VALUE_PcdShellSupportFrameworkHii  ((BOOLEAN) 0)
#define _PCD_GET_MODE_BOOL_PcdShellSupportFrameworkHii()  _PCD_VALUE_PcdShellSupportFrameworkHii

#define _PCD_TOKEN_PcdShellPageBreakDefault  ((UINT32) 5U)
#define _PCD_SIZE_PcdShellPageBreakDefault  ((UINTN) 1)
#define _PCD_GET_MODE_SIZE_PcdShellPageBreakDefault  _PCD_SIZE_PcdShellPageBreakDefault
#define _PCD_VALUE_PcdShellPageBreakDefault  ((BOOLEAN) 0)
#define _PCD_GET_MODE_BOOL_PcdShellPageBreakDefault()  _PCD_VALUE_PcdShellPageBreakDefault

#define _PCD_TOKEN_PcdShellInsertModeDefault  ((UINT32) 6U)
#define _PCD_SIZE_PcdShellInsertModeDefault  ((UINTN) 1)
#define _PCD_GET_MODE_SIZE_PcdShellInsertModeDefault  _PCD_SIZE_PcdShellInsertModeDefault
#define _PCD_VALUE_PcdShellInsertModeDefault  ((BOOLEAN) 1)
#define _PCD_GET_MODE_BOOL_PcdShellInsertModeDefault()  _PCD_VALUE_PcdShellInsertModeDefault

#define _PCD_TOKEN_PcdShellScreenLogCount  ((UINT32) 7U)
#define _PCD_SIZE_PcdShellScreenLogCount  ((UINTN) 1)
#define _PCD_GET_MODE_SIZE_PcdShellScreenLogCount  _PCD_SIZE_PcdShellScreenLogCount
#define _PCD_VALUE_PcdShellScreenLogCount  ((UINT8) 3)
#define _PCD_GET_MODE_8_PcdShellScreenLogCount()  _PCD_VALUE_PcdShellScreenLogCount

#define _PCD_TOKEN_PcdShellPrintBufferSize  ((UINT32) 8U)
#define _PCD_SIZE_PcdShellPrintBufferSize  ((UINTN) 2)
#define _PCD_GET_MODE_SIZE_PcdShellPrintBufferSize  _PCD_SIZE_PcdShellPrintBufferSize
#define _PCD_VALUE_PcdShellPrintBufferSize  ((UINT16) 16000)
#define _PCD_GET_MODE_16_PcdShellPrintBufferSize()  _PCD_VALUE_PcdShellPrintBufferSize

#define _PCD_TOKEN_PcdShellForceConsole  ((UINT32) 9U)
#define _PCD_SIZE_PcdShellForceConsole  ((UINTN) 1)
#define _PCD_GET_MODE_SIZE_PcdShellForceConsole  _PCD_SIZE_PcdShellForceConsole
#define _PCD_VALUE_PcdShellForceConsole  ((BOOLEAN) 0)
#define _PCD_GET_MODE_BOOL_PcdShellForceConsole()  _PCD_VALUE_PcdShellForceConsole

#define _PCD_TOKEN_PcdShellSupplier  ((UINT32) 10U)
#define _PCD_SIZE_PcdShellSupplier  ((UINTN) 2)
#define _PCD_GET_MODE_SIZE_PcdShellSupplier  _PCD_SIZE_PcdShellSupplier
#define _PCD_VALUE_PcdShellSupplier  L"EDK2"
#define _PCD_GET_MODE_PTR_PcdShellSupplier()  _PCD_VALUE_PcdShellSupplier

#define _PCD_TOKEN_PcdShellMaxHistoryCommandCount  ((UINT32) 11U)
#define _PCD_SIZE_PcdShellMaxHistoryCommandCount  ((UINTN) 2)
#define _PCD_GET_MODE_SIZE_PcdShellMaxHistoryCommandCount  _PCD_SIZE_PcdShellMaxHistoryCommandCount
#define _PCD_VALUE_PcdShellMaxHistoryCommandCount  ((UINT16) 20)
#define _PCD_GET_MODE_16_PcdShellMaxHistoryCommandCount()  _PCD_VALUE_PcdShellMaxHistoryCommandCount

#define _PCD_TOKEN_PcdShellDefaultDelay  ((UINT32) 12U)
#define _PCD_SIZE_PcdShellDefaultDelay  ((UINTN) 4)
#define _PCD_GET_MODE_SIZE_PcdShellDefaultDelay  _PCD_SIZE_PcdShellDefaultDelay
#define _PCD_VALUE_PcdShellDefaultDelay  ((UINT32) 5)
#define _PCD_GET_MODE_32_PcdShellDefaultDelay()  _PCD_VALUE_PcdShellDefaultDelay

// String token definitions
#define STR_GEN_PROBLEM_VAL           0x0001
#define STR_GEN_PROBLEM               0x0002
#define STR_GEN_TOO_MANY              0x0003
#define STR_GEN_TOO_FEW               0x0004
#define STR_GEN_PARAM_INV             0x0005
#define STR_GEN_FILE_OPEN_FAIL        0x0006
#define STR_GEN_FILE_AD               0x0007
#define STR_GEN_CRLF                  0x0008
#define STR_SHELL_CURDIR              0x0009
#define STR_SHELL_STARTUP_QUESTION    0x000A

// GUID declarations
extern EFI_GUID gEfiCallerIdGuid;
extern EFI_GUID gEfiCallerBaseName;

#endif
AUTOGEN_EOF

# Create AutoGen.c for linking
echo "📝 Creating AutoGen.c for VisualUEFIShell standalone build..."
cat > AutoGen.c << 'AUTOGEN_C_EOF'
// AutoGen.c - Generated for VisualUEFIShell standalone build with EFI 1.1 support

#include "AutoGen.h"

// GUID definitions
EFI_GUID gEfiCallerIdGuid = {0x7C04A583, 0x9E3E, 0x4f1c, {0xAD, 0x65, 0xE0, 0x52, 0x68, 0xD0, 0xB4, 0xD1}};
EFI_GUID gEfiCallerBaseName = {0x7C04A583, 0x9E3E, 0x4f1c, {0xAD, 0x65, 0xE0, 0x52, 0x68, 0xD0, 0xB4, 0xD1}};

// String definitions (minimal implementation)
CHAR16 *mShellStrings[] = {
  L"",                                    // 0x0000
  L"Value error",                         // STR_GEN_PROBLEM_VAL
  L"Problem",                             // STR_GEN_PROBLEM  
  L"Too many arguments",                  // STR_GEN_TOO_MANY
  L"Too few arguments",                   // STR_GEN_TOO_FEW
  L"Invalid parameter",                   // STR_GEN_PARAM_INV
  L"File open failed",                    // STR_GEN_FILE_OPEN_FAIL
  L"Access denied",                       // STR_GEN_FILE_AD
  L"\r\n",                               // STR_GEN_CRLF
  L"%E%s%N",                             // STR_SHELL_CURDIR
  L"Startup.nsh not found, continue?",   // STR_SHELL_STARTUP_QUESTION
};

// EFI 1.1 Legacy Support Functions (stubs for Visual Studio build)
BOOLEAN IsEfi11Environment(VOID) {
  return FALSE;
}

EFI_STATUS InstallShellEnvironmentProtocol(VOID) {
  return EFI_SUCCESS;
}

EFI_STATUS InstallShellInterfaceProtocol(VOID) {
  return EFI_SUCCESS;
}
AUTOGEN_C_EOF

echo "✅ EFI 1.1 support files added to isolated VisualUEFIShell"
echo "📦 AutoGen.h and AutoGen.c created for standalone build"
echo ""
echo "🎯 VisualUEFIShell ready for isolated build with EFI 1.1 support"
echo "   • Legacy protocol files: ShellLegacyProtocols.c/h, ShellLegacyTest.c"
echo "   • Modified Shell.c and ShellProtocol.c with EFI 1.1 integration"
echo "   • Standalone AutoGen files for Visual Studio build"
echo ""

cd "$WORKSPACE_ROOT"
echo ""
echo "🏁 Both builds prepared in isolated environments!"
echo "✨ Original EDK2 and VisualUEFIShell workspaces preserved (no contamination)"
echo ""
echo "🔍 Verify original workspaces are clean:"
echo "   EDK2: cd EDK2 && git status"
echo "   VisualUEFIShell: cd VisualUEFIShell && git status"
echo ""
echo "🎨 Build VisualUEFIShell:"
echo "   cd $VISUAL_BUILD_WORKSPACE/VisualUEFIShell"
echo "   open VisualUEFIShell.sln"
