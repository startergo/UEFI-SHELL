@echo off
echo Building UEFI Shell with EFI 1.1 Legacy Protocol Support...
echo ================================================================

set BUILDSTART=%time% %date%

REM Set up the build environment
cd EDK2
call edksetup.bat

REM Build the shell with our EFI 1.1 implementation
python BaseTools\Source\Python\build\build.py -p ShellPkg\ShellPkg.dsc -a X64 -t VS2015 -b RELEASE

set BUILDEND=%time% %date%
cd ..

echo.
echo Build started: %BUILDSTART%
echo Build ended:   %BUILDEND%
echo.

if exist EDK2\Build\Shell\RELEASE_VS2015\X64\ShellPkg\Application\Shell\EA4BB293-2D7F-4456-A681-1F22F42CD0BC\OUTPUT\Shell.efi (
    echo.
    echo ===============================================================
    echo SUCCESS: Shell.efi built with EFI 1.1 legacy protocol support
    echo ===============================================================
    echo.
    if exist UEFIBinaries\pureEDK2Shell\BOOTX64.EFI attrib -r   UEFIBinaries\pureEDK2Shell\BOOTX64.EFI > NUL
    if exist UEFIBinaries\pureEDK2Shell\BOOTX64.EFI del         UEFIBinaries\pureEDK2Shell\BOOTX64.EFI > NUL
    copy EDK2\Build\Shell\RELEASE_VS2015\X64\ShellPkg\Application\Shell\EA4BB293-2D7F-4456-A681-1F22F42CD0BC\OUTPUT\Shell.efi UEFIBinaries\pureEDK2Shell\BOOTX64.EFI > NUL
    echo CREATED SUCCESSFULLY: EDK2\Build\Shell\RELEASE_VS2015\X64\ShellPkg\Application\Shell\EA4BB293-2D7F-4456-A681-1F22F42CD0BC\OUTPUT\Shell.efi
    echo Copied to: UEFIBinaries\pureEDK2Shell\BOOTX64.EFI
    echo.
    echo This UEFI Shell now includes:
    echo - EFI 1.1 ShellInterface Protocol support
    echo - EFI 1.1 ShellEnvironment Protocol support
    echo - Framework HII compatibility
    echo - Legacy protocol detection and auto-enablement
    echo.
    echo The shell should now work on EFI 1.1 systems like 2010 Mac Pro
) else (
    echo.
    echo =============================================================
    echo FATAL FAIL: Shell.efi build failed
    echo =============================================================
    echo EDK2\Build\Shell\RELEASE_VS2015\X64\ShellPkg\Application\Shell\EA4BB293-2D7F-4456-A681-1F22F42CD0BC\OUTPUT\Shell.efi doesn't exist
)
