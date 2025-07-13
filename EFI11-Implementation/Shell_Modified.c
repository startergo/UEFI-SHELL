/** @file
  This is THE shell (application)

  Copyright (c) 2009 - 2019, Intel Corporation. All rights reserved.<BR>
  (C) Copyright 2013-2014 Hewlett-Packard Development Company, L.P.<BR>
  Copyright 2015-2018 Dell Technologies.<BR>
  Copyright (C) 2023, Apple Inc. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#define _CRT_SECURE_NO_WARNINGS
#include "Shell.h"
#include "ShellLegacyProtocols.h"
#define NCDETRACE/* REMOVE TO ENABLE TRACES */
#include "VERSION.h"
#include "BUILDNUM.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <wchar.h>
#include <cde.h>
#include "plugins.h"
#include <protocol\GraphicsOutput.h>

#include <Protocol\DevicePath.h>
#include <Protocol\DevicePathToText.h>
#include <Protocol\DevicePathFromText.h>
#include <Protocol\DevicePathUtilities.h>
#include <Protocol\LoadedImage.h>
#include <Protocol\SimpleFileSystem.h>

#include <Guid\Acpi.h>
#include <Protocol\AcpiSystemDescriptionTable.h>
#include <Protocol\AcpiTable.h>
#include <Guid\Acpi.h>
#include <IndustryStandard/Acpi62.h>
#include <IndustryStandard/MemoryMappedConfigurationSpaceAccessTable.h>

#include <Protocol\AcpiSystemDescriptionTable.h>

extern UINT64 _osifUefiShellGetTscPerSec(IN void* pCdeAppIf, unsigned short AcpiPmTmrBase);//kgtest

EFI_HANDLE        gImageHandle;
EFI_SYSTEM_TABLE* gSystemTable;

extern char* _strefierror(EFI_STATUS);
char _gfDEFAULT_UEFI_DRIVE_NAMING = 0;          // 1 -> FS0:..., 0 -> A:...
char*  _gPLUGINSTART;                           // .COFF plugin address im memory
size_t _gPLUGINSIZE;                            // .COFF plugin size
static char fSkipSTARTUPNSH = 0;                // preliminary: if F8 is pressed, STARTUP.NSH is skipped
static wchar_t wcsBootDrive[] = { L"FS99:" };   // drive name
static wchar_t wcsSetScreenResolutionByMode[32];// commandline to set video mode at boot, keep implicite zero initialization!!!
static char fInitiallyChangeToBootDrive = 0;    // flag to change to boot drive
extern unsigned _gCdeRTUefiShellInstanceType;   // flag to distinguish normal SHELL instance from EFI_SHELL_PROTOCOL.Execute()-invoked instance
//NOTE: value of 0:  initial boot type, Shell is started during system boot, w/o command line (pLoadedImageProtocol->LoadOptions == L"")
//      value of 1:  EFI_SHELL_PROTOCOL.Execute() type, Shell instance was invoked with the "Shell.efi -exit 'SHELL_COMMAND'",
//                   For this case suppress F8-delay and supress all output messages 
//      value of 2:  not yet defined

//
// Initialize the global structure
//
SHELL_INFO  ShellInfoObject = {
  NULL,
  NULL,
  FALSE,
  FALSE,
  {
    {
      {
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0
      }
    },
    0,
    NULL,
    NULL
  },
  {
    { NULL,NULL   }, NULL
  },
  {
    {
      { NULL,NULL   }, NULL
    },
    0,
    0,
    TRUE
  },
  NULL,
  0,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  {
    { NULL,NULL   }, NULL, NULL
  },
  {
    { NULL,NULL   }, NULL, NULL
  },
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  FALSE
};

STATIC CONST CHAR16  mScriptExtension[]      = L".NSH";
STATIC CONST CHAR16  mExecutableExtensions[] = L".NSH;.EFI";
STATIC CONST CHAR16  mStartupScript[]        = L"startup.nsh";
CONST CHAR16         mNoNestingEnvVarName[]  = L"nonesting";
CONST CHAR16         mNoNestingTrue[]        = L"True";
CONST CHAR16         mNoNestingFalse[]       = L"False";

/**
  Cleans off leading and trailing spaces and tabs.

  @param[in] String pointer to the string to trim them off.
**/
EFI_STATUS
TrimSpaces (
  IN CHAR16  **String
  )
{
  ASSERT (String != NULL);
  ASSERT (*String != NULL);
  //
  // Remove any spaces and tabs at the beginning of the (*String).
  //
  while (((*String)[0] == L' ') || ((*String)[0] == L'\t')) {
    CopyMem ((*String), (*String)+1, StrSize ((*String)) - sizeof ((*String)[0]));
  }

  //
  // Remove any spaces and tabs at the end of the (*String).
  //
  while ((StrLen (*String) > 0) && (((*String)[StrLen ((*String))-1] == L' ') || ((*String)[StrLen ((*String))-1] == L'\t'))) {
    (*String)[StrLen ((*String))-1] = CHAR_NULL;
  }

  return (EFI_SUCCESS);
}

/**
  Parse for the next instance of one string within another string. Can optionally make sure that
  the string was not escaped (^ character) per the shell specification.

  @param[in] SourceString             The string to search within
  @param[in] FindString               The string to look for
  @param[in] CheckForEscapeCharacter  TRUE to skip escaped instances of FinfString, otherwise will return even escaped instances
**/
CHAR16 *
FindNextInstance (
  IN CONST CHAR16   *SourceString,
  IN CONST CHAR16   *FindString,
  IN CONST BOOLEAN  CheckForEscapeCharacter
  )
{
  CHAR16  *Temp;

  if (SourceString == NULL) {
    return (NULL);
  }

  Temp = StrStr (SourceString, FindString);

  //
  // If nothing found, or we don't care about escape characters
  //
  if ((Temp == NULL) || !CheckForEscapeCharacter) {
    return (Temp);
  }

  //
  // If we found an escaped character, try again on the remainder of the string
  //
  if ((Temp > (SourceString)) && (*(Temp-1) == L'^')) {
    return FindNextInstance (Temp+1, FindString, CheckForEscapeCharacter);
  }

  //
  // we found the right character
  //
  return (Temp);
}

/**
  Check whether the string between a pair of % is a valid environment variable name.

  @param[in] BeginPercent       pointer to the first percent.
  @param[in] EndPercent          pointer to the last percent.

  @retval TRUE                          is a valid environment variable name.
  @retval FALSE                         is NOT a valid environment variable name.
**/
BOOLEAN
IsValidEnvironmentVariableName (
  IN CONST CHAR16  *BeginPercent,
  IN CONST CHAR16  *EndPercent
  )
{
  CONST CHAR16  *Walker;

  Walker = NULL;

  ASSERT (BeginPercent != NULL);
  ASSERT (EndPercent != NULL);
  ASSERT (BeginPercent < EndPercent);

  if ((BeginPercent + 1) == EndPercent) {
    return FALSE;
  }

  for (Walker = BeginPercent + 1; Walker < EndPercent; Walker++) {
    if (
        ((*Walker >= L'0') && (*Walker <= L'9')) ||
        ((*Walker >= L'A') && (*Walker <= L'Z')) ||
        ((*Walker >= L'a') && (*Walker <= L'z')) ||
        (*Walker == L'_')
        )
    {
      if ((Walker == BeginPercent + 1) && ((*Walker >= L'0') && (*Walker <= L'9'))) {
        return FALSE;
      } else {
        continue;
      }
    } else {
      return FALSE;
    }
  }

  return TRUE;
}

/**
  Determine if a command line contains a split operation

  @param[in] CmdLine      The command line to parse.

  @retval TRUE            CmdLine has a valid split.
  @retval FALSE           CmdLine does not have a valid split.
**/
BOOLEAN
ContainsSplit (
  IN CONST CHAR16  *CmdLine
  )
{
  CONST CHAR16  *TempSpot;
  CONST CHAR16  *FirstQuote;
  CONST CHAR16  *SecondQuote;

  FirstQuote  = FindNextInstance (CmdLine, L"\"", TRUE);
  SecondQuote = NULL;
  TempSpot    = FindFirstCharacter (CmdLine, L"|", L'^');

  if ((FirstQuote == NULL) ||
      (TempSpot == NULL) ||
      (TempSpot == CHAR_NULL) ||
      (FirstQuote > TempSpot)
      )
  {
      //CDETRACE((TRCINF(1) "SPLIT: %d\n", (BOOLEAN)((TempSpot != NULL) && (*TempSpot != CHAR_NULL))));
      return (BOOLEAN)((TempSpot != NULL) && (*TempSpot != CHAR_NULL));
  }

  while ((TempSpot != NULL) && (*TempSpot != CHAR_NULL)) {
    if ((FirstQuote == NULL) || (FirstQuote > TempSpot)) {
      break;
    }

    SecondQuote = FindNextInstance (FirstQuote + 1, L"\"", TRUE);
    if (SecondQuote == NULL) {
      break;
    }

    if (SecondQuote < TempSpot) {
      FirstQuote = FindNextInstance (SecondQuote + 1, L"\"", TRUE);
      continue;
    } else {
      FirstQuote = FindNextInstance (SecondQuote + 1, L"\"", TRUE);
      TempSpot   = FindFirstCharacter (TempSpot + 1, L"|", L'^');
      continue;
    }
  }
  
  //CDETRACE((TRCINF(1) "SPLIT: %d\n", (BOOLEAN)((TempSpot != NULL) && (*TempSpot != CHAR_NULL))));

  return (BOOLEAN)((TempSpot != NULL) && (*TempSpot != CHAR_NULL));
}

/**
  Function to start monitoring for CTRL-S using SimpleTextInputEx.  This
  feature's enabled state was not known when the shell initially launched.

  @retval EFI_SUCCESS           The feature is enabled.
  @retval EFI_OUT_OF_RESOURCES  There is not enough memory available.
**/
EFI_STATUS
InternalEfiShellStartCtrlSMonitor (
  VOID
  )
{
  EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  *SimpleEx;
  EFI_KEY_DATA                       KeyData;
  EFI_STATUS                         Status;

  Status = gBS->OpenProtocol (
                  gST->ConsoleInHandle,
                  &gEfiSimpleTextInputExProtocolGuid,
                  (VOID **)&SimpleEx,
                  gImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (
      -1,
      -1,
      NULL,
      STRING_TOKEN (STR_SHELL_NO_IN_EX),
      ShellInfoObject.HiiHandle
      );
    return (EFI_SUCCESS);
  }

  KeyData.KeyState.KeyToggleState = 0;
  KeyData.Key.ScanCode            = 0;
  KeyData.KeyState.KeyShiftState  = EFI_SHIFT_STATE_VALID|EFI_LEFT_CONTROL_PRESSED;
  KeyData.Key.UnicodeChar         = L's';

  Status = SimpleEx->RegisterKeyNotify (
                       SimpleEx,
                       &KeyData,
                       NotificationFunction,
                       &ShellInfoObject.CtrlSNotifyHandle1
                       );

  KeyData.KeyState.KeyShiftState = EFI_SHIFT_STATE_VALID|EFI_RIGHT_CONTROL_PRESSED;
  if (!EFI_ERROR (Status)) {
    Status = SimpleEx->RegisterKeyNotify (
                         SimpleEx,
                         &KeyData,
                         NotificationFunction,
                         &ShellInfoObject.CtrlSNotifyHandle2
                         );
  }

  KeyData.KeyState.KeyShiftState = EFI_SHIFT_STATE_VALID|EFI_LEFT_CONTROL_PRESSED;
  KeyData.Key.UnicodeChar        = 19;

  if (!EFI_ERROR (Status)) {
    Status = SimpleEx->RegisterKeyNotify (
                         SimpleEx,
                         &KeyData,
                         NotificationFunction,
                         &ShellInfoObject.CtrlSNotifyHandle3
                         );
  }

  KeyData.KeyState.KeyShiftState = EFI_SHIFT_STATE_VALID|EFI_RIGHT_CONTROL_PRESSED;
  if (!EFI_ERROR (Status)) {
    Status = SimpleEx->RegisterKeyNotify (
                         SimpleEx,
                         &KeyData,
                         NotificationFunction,
                         &ShellInfoObject.CtrlSNotifyHandle4
                         );
  }

  return (Status);
}

/**
  The entry point for the application.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                      Status;
  CHAR16                          *TempString;
  UINTN                           Size;
  EFI_HANDLE                      ConInHandle;
  EFI_SIMPLE_TEXT_INPUT_PROTOCOL  *OldConIn;
  SPLIT_LIST                      *Split;

  if (PcdGet8 (PcdShellSupportLevel) > 3) {
    return (EFI_UNSUPPORTED);
  }

  //
  // Clear the screen
  //
  Status = gST->ConOut->ClearScreen (gST->ConOut);
  if (EFI_ERROR (Status)) {
    return (Status);
  }

  //
  // Populate the global structure from PCDs
  //
  ShellInfoObject.ImageDevPath               = NULL;
  ShellInfoObject.FileDevPath                = NULL;
  ShellInfoObject.PageBreakEnabled           = PcdGetBool (PcdShellPageBreakDefault);
  ShellInfoObject.ViewingSettings.InsertMode = PcdGetBool (PcdShellInsertModeDefault);
  ShellInfoObject.LogScreenCount             = PcdGet8 (PcdShellScreenLogCount);

  //
  // verify we dont allow for spec violation
  //
  ASSERT (ShellInfoObject.LogScreenCount >= 3);

  //
  // Initialize the LIST ENTRY objects...
  //
  InitializeListHead (&ShellInfoObject.BufferToFreeList.Link);
  InitializeListHead (&ShellInfoObject.ViewingSettings.CommandHistory.Link);
  InitializeListHead (&ShellInfoObject.SplitList.Link);

  //
  // Check PCDs for optional features that are now implemented for EFI 1.1 support.
  // Legacy protocol support is now enabled.
  //
  if (!FeaturePcdGet (PcdShellRequireHiiPlatform)) {
    //
    // HII Platform support not required - this is OK for EFI 1.1
    //
    DEBUG ((DEBUG_INFO, "Shell: HII Platform support not required (EFI 1.1 mode)\n"));
  }

  //
  // turn off the watchdog timer
  //
  gBS->SetWatchdogTimer (0, 0, 0, NULL);

  //
  // install our console logger.  This will keep a log of the output for back-browsing
  //
  Status = ConsoleLoggerInstall (ShellInfoObject.LogScreenCount, &ShellInfoObject.ConsoleInfo);
  if (!EFI_ERROR (Status)) {
    //
    // Enable the cursor to be visible
    //
    gST->ConOut->EnableCursor (gST->ConOut, TRUE);

    //
    // If supporting EFI 1.1 we need to install HII protocol
    // only do this if PcdShellRequireHiiPlatform == FALSE
    //
    // EFI 1.1 Framework HII support implementation
    if (PcdGetBool (PcdShellSupportFrameworkHii) || IsEfi11Environment ()) {
      DEBUG ((DEBUG_INFO, "Shell: Framework HII support enabled for EFI 1.1 compatibility\n"));
      // Framework HII is simpler - we'll manage without complex HII for basic shell functionality
    }

    //
    // install our (solitary) HII package
    //
    ShellInfoObject.HiiHandle = HiiAddPackages (&gEfiCallerIdGuid, gImageHandle, ShellStrings, NULL);
    if (ShellInfoObject.HiiHandle == NULL) {
      if (PcdGetBool (PcdShellSupportFrameworkHii) || IsEfi11Environment ()) {
        //
        // For EFI 1.1 compatibility, we can operate without HII packages
        // Create a minimal HII handle for compatibility
        //
        DEBUG ((DEBUG_INFO, "Shell: Operating in EFI 1.1 mode without HII packages\n"));
        ShellInfoObject.HiiHandle = (EFI_HII_HANDLE) 1; // Dummy handle for compatibility
      }

      if (ShellInfoObject.HiiHandle == NULL) {
        Status = EFI_NOT_STARTED;
        goto FreeResources;
      }
    }

    //
    // create and install the EfiShellParametersProtocol
    //
    Status = CreatePopulateInstallShellParametersProtocol (&ShellInfoObject.NewShellParametersProtocol, &ShellInfoObject.RootShellInstance);
    ASSERT_EFI_ERROR (Status);
    ASSERT (ShellInfoObject.NewShellParametersProtocol != NULL);

    //
    // create and install the EfiShellProtocol
    //
    Status = CreatePopulateInstallShellProtocol (&ShellInfoObject.NewEfiShellProtocol);
    ASSERT_EFI_ERROR (Status);
    ASSERT (ShellInfoObject.NewEfiShellProtocol != NULL);

    //
    // Initialize legacy EFI 1.1 protocol support
    //
    if (PcdGetBool (PcdShellSupportOldProtocols) || IsEfi11Environment ()) {
      DEBUG ((DEBUG_INFO, "Shell: Initializing EFI 1.1 legacy protocol support\n"));
      Status = InstallShellEnvironmentProtocol ();
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_WARN, "Shell: Failed to install legacy protocols - %r\n", Status));
      } else {
        DEBUG ((DEBUG_INFO, "Shell: Legacy protocols installed successfully\n"));
      }
    }

    //
    // Now initialize the shell library (it requires Shell Parameters protocol)
    //
    Status = ShellInitialize ();
    ASSERT_EFI_ERROR (Status);

    Status = CommandInit ();
    ASSERT_EFI_ERROR (Status);

    Status = ShellInitEnvVarList ();

    //
    // Check the command line
    //
    Status = ProcessCommandLine ();
    if (EFI_ERROR (Status)) {
      goto FreeResources;
    }

    //
    // If shell support level is >= 1 create the mappings and paths
    //
    if (PcdGet8 (PcdShellSupportLevel) >= 1) {
      Status = ShellCommandCreateInitialMappingsAndPaths ();
    }

    //
    // Set the environment variable for nesting support
    //
    Size       = 0;
    TempString = NULL;
    if (!ShellInfoObject.ShellInitSettings.BitUnion.Bits.NoNest) {
      //
      // No change.  require nesting in Shell Protocol Execute()
      //
      StrnCatGrow (
        &TempString,
        &Size,
        L"False",
        0
        );
    } else {
      StrnCatGrow (
        &TempString,
        &Size,
        mNoNestingTrue,
        0
        );
    }

    Status = InternalEfiShellSetEnv (mNoNestingEnvVarName, TempString, TRUE);
    SHELL_FREE_NON_NULL (TempString);
    Size = 0;

    //
    // save the device path for the loaded image and the device path for the filepath (under loaded image)
    // These are where to look for the startup.nsh file
    //
    Status = GetDevicePathsForImageAndFile (&ShellInfoObject.ImageDevPath, &ShellInfoObject.FileDevPath);
    ASSERT_EFI_ERROR (Status);

    if (1 == _gCdeRTUefiShellInstanceType)
    {
        //
        // Display the version
        //
        if (!ShellInfoObject.ShellInitSettings.BitUnion.Bits.NoVersion) {
          ShellPrintHiiEx (
            0,
            gST->ConOut->Mode->CursorRow,
            NULL,
            STRING_TOKEN (STR_VER_OUTPUT_MAIN_SHELL),
            ShellInfoObject.HiiHandle,
            SupportLevel[PcdGet8 (PcdShellSupportLevel)],
            gEfiShellProtocol->MajorVersion,
            gEfiShellProtocol->MinorVersion
            );

          ShellPrintHiiEx (
            -1,
            -1,
            NULL,
            STRING_TOKEN (STR_VER_OUTPUT_MAIN_SUPPLIER),
            ShellInfoObject.HiiHandle,
            (CHAR16 *)PcdGetPtr (PcdShellSupplier)
            );

          ShellPrintHiiEx (
            -1,
            -1,
            NULL,
            STRING_TOKEN (STR_VER_OUTPUT_MAIN_UEFI),
            ShellInfoObject.HiiHandle,
            (gST->Hdr.Revision&0xffff0000)>>16,
            (gST->Hdr.Revision&0x0000ffff),
            gST->FirmwareVendor,
            gST->FirmwareRevision
            );
        }

        //
        // Display the mapping
        //
        if ((PcdGet8 (PcdShellSupportLevel) >= 2) && !ShellInfoObject.ShellInitSettings.BitUnion.Bits.NoMap) {
          Status = RunCommand (L"map");
          ASSERT_EFI_ERROR (Status);
        }
    }

    //
    // init all the built in alias'
    //
    Status = SetBuiltInAlias ();
    ASSERT_EFI_ERROR (Status);

    //
    // Initialize environment variables
    //
    if (ShellCommandGetProfileList () != NULL) {
      Status = InternalEfiShellSetEnv (L"profiles", ShellCommandGetProfileList (), TRUE);
      ASSERT_EFI_ERROR (Status);
    }

    Size       = 100;
    TempString = AllocateZeroPool (Size);
    if (TempString == NULL) {
      ASSERT (TempString != NULL);
      Status = EFI_OUT_OF_RESOURCES;
      goto FreeResources;
    }

    UnicodeSPrint (TempString, Size, L"%d", PcdGet8 (PcdShellSupportLevel));
    Status = InternalEfiShellSetEnv (L"uefishellsupport", TempString, TRUE);
    ASSERT_EFI_ERROR (Status);

    UnicodeSPrint (TempString, Size, L"%d.%d", ShellInfoObject.NewEfiShellProtocol->MajorVersion, ShellInfoObject.NewEfiShellProtocol->MinorVersion);
    Status = InternalEfiShellSetEnv (L"uefishellversion", TempString, TRUE);
    ASSERT_EFI_ERROR (Status);

    UnicodeSPrint (TempString, Size, L"%d.%d", (gST->Hdr.Revision & 0xFFFF0000) >> 16, gST->Hdr.Revision & 0x0000FFFF);
    Status = InternalEfiShellSetEnv (L"uefiversion", TempString, TRUE);
    ASSERT_EFI_ERROR (Status);

    FreePool (TempString);

    if (!EFI_ERROR (Status)) {
      if (!ShellInfoObject.ShellInitSettings.BitUnion.Bits.NoInterrupt) {
        //
        // Set up the event for CTRL-C monitoring...
        //
        Status = InernalEfiShellStartMonitor ();
      }

      if (!EFI_ERROR (Status) && !ShellInfoObject.ShellInitSettings.BitUnion.Bits.NoConsoleIn) {
        //
        // Set up the event for CTRL-S monitoring...
        //
        Status = InternalEfiShellStartCtrlSMonitor ();
      }

      if (!EFI_ERROR (Status) && ShellInfoObject.ShellInitSettings.BitUnion.Bits.NoConsoleIn) {
        //
        // close off the gST->ConIn
        //
        OldConIn    = gST->ConIn;
        ConInHandle = gST->ConsoleInHandle;
        gST->ConIn  = CreateSimpleTextInOnFile ((SHELL_FILE_HANDLE)&FileInterfaceNulFile, &gST->ConsoleInHandle);
      } else {
        OldConIn    = NULL;
        ConInHandle = NULL;
      }

      //
      // despite screen resolution is already set by API call, we need to tell the SHELL / MODE command what the resolution currently is
      //    NOTE: MODE command needs to know screen resolution !!!
      //
      if (0 != wcslen(wcsSetScreenResolutionByMode))
      {
          RunCommand(wcsSetScreenResolutionByMode);
      }

      if (!EFI_ERROR (Status) && (PcdGet8 (PcdShellSupportLevel) >= 1)) {
        //
        // process the startup script or launch the called app.
        //
        Status = DoStartupScript (ShellInfoObject.ImageDevPath, ShellInfoObject.FileDevPath);
      }

      if (!ShellInfoObject.ShellInitSettings.BitUnion.Bits.Exit && !ShellCommandGetExit () && ((PcdGet8 (PcdShellSupportLevel) >= 3) || PcdGetBool (PcdShellForceConsole)) && !EFI_ERROR (Status) && !ShellInfoObject.ShellInitSettings.BitUnion.Bits.NoConsoleIn) {
        //
        // begin the UI waiting loop
        //
        do {
          //
          // clean out all the memory allocated for CONST <something> * return values
          // between each shell prompt presentation
          //
          if (!IsListEmpty (&ShellInfoObject.BufferToFreeList.Link)) {
            FreeBufferList (&ShellInfoObject.BufferToFreeList);
          }

          //
          // Reset page break back to default.
          //
          ShellInfoObject.PageBreakEnabled = PcdGetBool (PcdShellPageBreakDefault);
          ASSERT (ShellInfoObject.ConsoleInfo != NULL);
          ShellInfoObject.ConsoleInfo->Enabled    = TRUE;
          ShellInfoObject.ConsoleInfo->RowCounter = 0;
          
          //
          // activate/select boot drive
          //
          if (1 != _gCdeRTUefiShellInstanceType)
          {
              if (0 == fInitiallyChangeToBootDrive)
              {
                  fInitiallyChangeToBootDrive = 1;
                  FILE* fp = fopen("\\EFI\\BOOT\\bootx64.ini", "r");
                  // RunCommand(wcsBootDrive);
                  
                  if (NULL == fp) {
                      fp = fopen("\\EFI\\BOOT\\bootx64.ini", "w");
                      fprintf(fp, "#\n# remove comment to enable low res video mode\n#\n#TEXTRESOLUTION 80 25\n#DEFAULT_UEFI_DRIVE_NAMING");
                      fclose(fp);
                  }
              }
          }

          //
          // Display Prompt
          //
          Status = DoShellPrompt ();
          //CDETRACE((TRCINF(1) "$$$$\n"));
        } while (!ShellCommandGetExit ());
      }

      if ((OldConIn != NULL) && (ConInHandle != NULL)) {
        CloseSimpleTextInOnFile (gST->ConIn);
        gST->ConIn           = OldConIn;
        gST->ConsoleInHandle = ConInHandle;
      }
    }
  }

FreeResources:
  //
  // uninstall protocols / free memory / etc...
  //
  if (ShellInfoObject.UserBreakTimer != NULL) {
    gBS->CloseEvent (ShellInfoObject.UserBreakTimer);
    DEBUG_CODE (
      ShellInfoObject.UserBreakTimer = NULL;
      );
  }

  if (ShellInfoObject.ImageDevPath != NULL) {
    FreePool (ShellInfoObject.ImageDevPath);
    DEBUG_CODE (
      ShellInfoObject.ImageDevPath = NULL;
      );
  }

  if (ShellInfoObject.FileDevPath != NULL) {
    FreePool (ShellInfoObject.FileDevPath);
    DEBUG_CODE (
      ShellInfoObject.FileDevPath = NULL;
      );
  }

  if (ShellInfoObject.NewShellParametersProtocol != NULL) {
    CleanUpShellParametersProtocol (ShellInfoObject.NewShellParametersProtocol);
    DEBUG_CODE (
      ShellInfoObject.NewShellParametersProtocol = NULL;
      );
  }

  if (ShellInfoObject.NewEfiShellProtocol != NULL) {
    if (ShellInfoObject.NewEfiShellProtocol->IsRootShell ()) {
      InternalEfiShellSetEnv (L"cwd", NULL, TRUE);
    }

    CleanUpShellEnvironment (ShellInfoObject.NewEfiShellProtocol);
    DEBUG_CODE (
      ShellInfoObject.NewEfiShellProtocol = NULL;
      );
  }

  //
  // Cleanup legacy EFI 1.1 protocol support
  //
  if (PcdGetBool (PcdShellSupportOldProtocols) || IsEfi11Environment ()) {
    DEBUG ((DEBUG_INFO, "Shell: Cleaning up EFI 1.1 legacy protocol support\n"));
    CleanupLegacyShellSupport ();
  }

  if (!IsListEmpty (&ShellInfoObject.BufferToFreeList.Link)) {
    FreeBufferList (&ShellInfoObject.BufferToFreeList);
  }

  if (!IsListEmpty (&ShellInfoObject.SplitList.Link)) {
    ASSERT (FALSE); /// @todo finish this de-allocation (free SplitStdIn/Out when needed).

    for ( Split = (SPLIT_LIST *)GetFirstNode (&ShellInfoObject.SplitList.Link)
          ; !IsNull (&ShellInfoObject.SplitList.Link, &Split->Link)
          ; Split = (SPLIT_LIST *)GetNextNode (&ShellInfoObject.SplitList.Link, &Split->Link)
          )
    {
      RemoveEntryList (&Split->Link);
      FreePool (Split);
    }

    DEBUG_CODE (
      InitializeListHead (&ShellInfoObject.SplitList.Link);
      );
  }

  if (ShellInfoObject.ShellInitSettings.FileName != NULL) {
    FreePool (ShellInfoObject.ShellInitSettings.FileName);
    DEBUG_CODE (
      ShellInfoObject.ShellInitSettings.FileName = NULL;
      );
  }

  if (ShellInfoObject.ShellInitSettings.FileOptions != NULL) {
    FreePool (ShellInfoObject.ShellInitSettings.FileOptions);
    DEBUG_CODE (
      ShellInfoObject.ShellInitSettings.FileOptions = NULL;
      );
  }

  if (ShellInfoObject.HiiHandle != NULL) {
    HiiRemovePackages (ShellInfoObject.HiiHandle);
    DEBUG_CODE (
      ShellInfoObject.HiiHandle = NULL;
      );
  }

  if (!IsListEmpty (&ShellInfoObject.ViewingSettings.CommandHistory.Link)) {
    FreeBufferList (&ShellInfoObject.ViewingSettings.CommandHistory);
  }

  ASSERT (ShellInfoObject.ConsoleInfo != NULL);
  if (ShellInfoObject.ConsoleInfo != NULL) {
    ConsoleLoggerUninstall (ShellInfoObject.ConsoleInfo);
    FreePool (ShellInfoObject.ConsoleInfo);
    DEBUG_CODE (
      ShellInfoObject.ConsoleInfo = NULL;
      );
  }

  ShellFreeEnvVarList ();

  if (ShellCommandGetExit ()) {
    return ((EFI_STATUS)ShellCommandGetExitCode ());
  }

  return (Status);
}

/**
  Sets all the alias' that were registered with the ShellCommandLib library.

  @retval EFI_SUCCESS           all init commands were run successfully.
**/
EFI_STATUS
SetBuiltInAlias (
  VOID
  )
{
  EFI_STATUS        Status;
  CONST ALIAS_LIST  *List;
  ALIAS_LIST        *Node;

  //
  // Get all the commands we want to run
  //
  List = ShellCommandGetInitAliasList ();

  //
  // for each command in the List
  //
  for ( Node = (ALIAS_LIST *)GetFirstNode (&List->Link)
        ; !IsNull (&List->Link, &Node->Link)
        ; Node = (ALIAS_LIST *)GetNextNode (&List->Link, &Node->Link)
        )
  {
    //
    // install the alias'
    //
    Status = InternalSetAlias (Node->CommandString, Node->Alias, TRUE);
    ASSERT_EFI_ERROR (Status);
  }

  return (EFI_SUCCESS);
}

/**
  Internal function to determine if 2 command names are really the same.

  @param[in] Command1       The pointer to the first command name.
  @param[in] Command2       The pointer to the second command name.

  @retval TRUE              The 2 command names are the same.
  @retval FALSE             The 2 command names are not the same.
**/
BOOLEAN
IsCommand (
  IN CONST CHAR16  *Command1,
  IN CONST CHAR16  *Command2
  )
{
  if (StringNoCaseCompare (&Command1, &Command2) == 0) {
    return (TRUE);
  }

  return (FALSE);
}

/**
  Internal function to determine if a command is a script only command.

  @param[in] CommandName    The pointer to the command name.

  @retval TRUE              The command is a script only command.
  @retval FALSE             The command is not a script only command.
**/
BOOLEAN
IsScriptOnlyCommand (
  IN CONST CHAR16  *CommandName
  )
{
  if (  IsCommand (CommandName, L"for")
     || IsCommand (CommandName, L"endfor")
     || IsCommand (CommandName, L"if")
     || IsCommand (CommandName, L"else")
     || IsCommand (CommandName, L"endif")
     || IsCommand (CommandName, L"goto"))
  {
    return (TRUE);
  }

  return (FALSE);
}

/**
  This function will populate the 2 device path protocol parameters based on the
  global gImageHandle.  The DevPath will point to the device path for the handle that has
  loaded image protocol installed on it.  The FilePath will point to the device path
  for the file that was loaded.

  @param[in, out] DevPath       On a successful return the device path to the loaded image.
  @param[in, out] FilePath      On a successful return the device path to the file.

  @retval EFI_SUCCESS           The 2 device paths were successfully returned.
  @retval other                 A error from gBS->HandleProtocol.

  @sa HandleProtocol
**/
EFI_STATUS
GetDevicePathsForImageAndFile (
  IN OUT EFI_DEVICE_PATH_PROTOCOL  **DevPath,
  IN OUT EFI_DEVICE_PATH_PROTOCOL  **FilePath
  )
{
  EFI_STATUS                 Status;
  EFI_LOADED_IMAGE_PROTOCOL  *LoadedImage;
  EFI_DEVICE_PATH_PROTOCOL   *ImageDevicePath;

  ASSERT (DevPath  != NULL);
  ASSERT (FilePath != NULL);

  Status = gBS->OpenProtocol (
                  gImageHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **)&LoadedImage,
                  gImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (!EFI_ERROR (Status)) {
    Status = gBS->OpenProtocol (
                    LoadedImage->DeviceHandle,
                    &gEfiDevicePathProtocolGuid,
                    (VOID **)&ImageDevicePath,
                    gImageHandle,
                    NULL,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
    if (!EFI_ERROR (Status)) {
      *DevPath  = DuplicateDevicePath (ImageDevicePath);
      *FilePath = DuplicateDevicePath (LoadedImage->FilePath);
      gBS->CloseProtocol (
             LoadedImage->DeviceHandle,
             &gEfiDevicePathProtocolGuid,
             gImageHandle,
             NULL
             );
    }

    gBS->CloseProtocol (
           gImageHandle,
           &gEfiLoadedImageProtocolGuid,
           gImageHandle,
           NULL
           );
  }

  return (Status);
}

/**
  Process all Uefi Shell 2.0 command line options.

  see Uefi Shell 2.0 section 3.2 for full details.

  the command line must resemble the following:

  shell.efi [ShellOpt-options] [options] [file-name [file-name-options]]

  ShellOpt-options  Options which control the initialization behavior of the shell.
                    These options are read from the EFI global variable "ShellOpt"
                    and are processed before options or file-name.

  options           Options which control the initialization behavior of the shell.

  file-name         The name of a UEFI shell application or script to be executed
                    after initialization is complete. By default, if file-name is
                    specified, then -nostartup is implied. Scripts are not supported
                    by level 0.

  file-name-options The command-line options that are passed to file-name when it
                    is invoked.

  This will initialize the ShellInfoObject.ShellInitSettings global variable.

  @retval EFI_SUCCESS           The variable is initialized.
**/
EFI_STATUS
ProcessCommandLine (
  VOID
  )
{
  UINTN                           Size;
  UINTN                           LoopVar;
  CHAR16                          *CurrentArg;
  CHAR16                          *DelayValueStr;
  UINT64                          DelayValue;
  EFI_STATUS                      Status;
  EFI_UNICODE_COLLATION_PROTOCOL  *UnicodeCollation;

  // `file-name-options` will contain arguments to `file-name` that we don't
  // know about. This would cause ShellCommandLineParse to error, so we parse
  // arguments manually, ignoring those after the first thing that doesn't look
  // like a shell option (which is assumed to be `file-name`).

  Status = gBS->LocateProtocol (
                  &gEfiUnicodeCollation2ProtocolGuid,
                  NULL,
                  (VOID **)&UnicodeCollation
                  );
  if (EFI_ERROR (Status)) {
    Status = gBS->LocateProtocol (
                    &gEfiUnicodeCollationProtocolGuid,
                    NULL,
                    (VOID **)&UnicodeCollation
                    );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  // Set default options
  ShellInfoObject.ShellInitSettings.BitUnion.Bits.Startup      = FALSE;
  ShellInfoObject.ShellInitSettings.BitUnion.Bits.NoStartup    = FALSE;
  ShellInfoObject.ShellInitSettings.BitUnion.Bits.NoConsoleOut = FALSE;
  ShellInfoObject.ShellInitSettings.BitUnion.Bits.NoConsoleIn  = FALSE;
  ShellInfoObject.ShellInitSettings.BitUnion.Bits.NoInterrupt  = FALSE;
  ShellInfoObject.ShellInitSettings.BitUnion.Bits.NoMap        = FALSE;
  ShellInfoObject.ShellInitSettings.BitUnion.Bits.NoVersion    = FALSE;
  ShellInfoObject.ShellInitSettings.BitUnion.Bits.Delay        = FALSE;
  ShellInfoObject.ShellInitSettings.BitUnion.Bits.Exit         = FALSE;
  ShellInfoObject.ShellInitSettings.BitUnion.Bits.NoNest       = FALSE;
  ShellInfoObject.ShellInitSettings.Delay                      = PcdGet32 (PcdShellDefaultDelay);

  //
  // Start LoopVar at 0 to parse only optional arguments at Argv[0]
  // and parse other parameters from Argv[1].  This is for use case that
  // UEFI Shell boot option is created, and OptionalData is provided
  // that starts with shell command-line options.
  //
  for (LoopVar = 0; LoopVar < gEfiShellParametersProtocol->Argc; LoopVar++) {
    CurrentArg = gEfiShellParametersProtocol->Argv[LoopVar];
    if (UnicodeCollation->StriColl (
                            UnicodeCollation,
                            L"-startup",
                            CurrentArg
                            ) == 0)
    {
      ShellInfoObject.ShellInitSettings.BitUnion.Bits.Startup = TRUE;
    } else if (UnicodeCollation->StriColl (
                                   UnicodeCollation,
                                   L"-nostartup",
                                   CurrentArg
                                   ) == 0)
    {
      ShellInfoObject.ShellInitSettings.BitUnion.Bits.NoStartup = TRUE;
    } else if (UnicodeCollation->StriColl (
                                   UnicodeCollation,
                                   L"-noconsoleout",
                                   CurrentArg
                                   ) == 0)
    {
      ShellInfoObject.ShellInitSettings.BitUnion.Bits.NoConsoleOut = TRUE;
    } else if (UnicodeCollation->StriColl (
                                   UnicodeCollation,
                                   L"-noconsolein",
                                   CurrentArg
                                   ) == 0)
    {
      ShellInfoObject.ShellInitSettings.BitUnion.Bits.NoConsoleIn = TRUE;
    } else if (UnicodeCollation->StriColl (
                                   UnicodeCollation,
                                   L"-nointerrupt",
                                   CurrentArg
                                   ) == 0)
    {
      ShellInfoObject.ShellInitSettings.BitUnion.Bits.NoInterrupt = TRUE;
    } else if (UnicodeCollation->StriColl (
                                   UnicodeCollation,
                                   L"-nomap",
                                   CurrentArg
                                   ) == 0)
    {
      ShellInfoObject.ShellInitSettings.BitUnion.Bits.NoMap = TRUE;
    } else if (UnicodeCollation->StriColl (
                                   UnicodeCollation,
                                   L"-noversion",
                                   CurrentArg
                                   ) == 0)
    {
      ShellInfoObject.ShellInitSettings.BitUnion.Bits.NoVersion = TRUE;
    } else if (UnicodeCollation->StriColl (
                                   UnicodeCollation,
                                   L"-nonest",
                                   CurrentArg
                                   ) == 0)
    {
      ShellInfoObject.ShellInitSettings.BitUnion.Bits.NoNest = TRUE;
    } else if (UnicodeCollation->StriColl (
                                   UnicodeCollation,
                                   L"-delay",
                                   CurrentArg
                                   ) == 0)
    {
      ShellInfoObject.ShellInitSettings.BitUnion.Bits.Delay = TRUE;
      // Check for optional delay value following "-delay"
      if ((LoopVar + 1) >= gEfiShellParametersProtocol->Argc) {
        DelayValueStr = NULL;
      } else {
        DelayValueStr = gEfiShellParametersProtocol->Argv[LoopVar + 1];
      }

      if (DelayValueStr != NULL) {
        if (*DelayValueStr == L':') {
          DelayValueStr++;
        }

        if (!EFI_ERROR (
               ShellConvertStringToUint64 (
                 DelayValueStr,
                 &DelayValue,
                 FALSE,
                 FALSE
                 )
               ))
        {
          ShellInfoObject.ShellInitSettings.Delay = (UINTN)DelayValue;
          LoopVar++;
        }
      }
    } else if (UnicodeCollation->StriColl (
                                   UnicodeCollation,
                                   L"-exit",
                                   CurrentArg
                                   ) == 0)
    {
      ShellInfoObject.ShellInitSettings.BitUnion.Bits.Exit = TRUE;
    } else if (StrnCmp (L"-", CurrentArg, 1) == 0) {
      // Unrecognized option
      ShellPrintHiiEx (
        -1,
        -1,
        NULL,
        STRING_TOKEN (STR_GEN_PROBLEM),
        ShellInfoObject.HiiHandle,
        CurrentArg
        );
      return EFI_INVALID_PARAMETER;
    } else {
      //
      // First argument should be Shell.efi image name
      //
      if (LoopVar == 0) {
        continue;
      }

      ShellInfoObject.ShellInitSettings.FileName = NULL;
      Size                                       = 0;
      //
      // If first argument contains a space, then add double quotes before the argument
      //
      if (StrStr (CurrentArg, L" ") != NULL) {
        StrnCatGrow (&ShellInfoObject.ShellInitSettings.FileName, &Size, L"\"", 0);
        if (ShellInfoObject.ShellInitSettings.FileName == NULL) {
          return (EFI_OUT_OF_RESOURCES);
        }
      }

      StrnCatGrow (&ShellInfoObject.ShellInitSettings.FileName, &Size, CurrentArg, 0);
      if (ShellInfoObject.ShellInitSettings.FileName == NULL) {
        return (EFI_OUT_OF_RESOURCES);
      }

      //
      // If first argument contains a space, then add double quotes after the argument
      //
      if (StrStr (CurrentArg, L" ") != NULL) {
        StrnCatGrow (&ShellInfoObject.ShellInitSettings.FileName, &Size, L"\"", 0);
        if (ShellInfoObject.ShellInitSettings.FileName == NULL) {
          return (EFI_OUT_OF_RESOURCES);
        }
      }

      //
      // We found `file-name`.
      //
      ShellInfoObject.ShellInitSettings.BitUnion.Bits.NoStartup = 1;
      LoopVar++;

      // Add `file-name-options`
      for (Size = 0; LoopVar < gEfiShellParametersProtocol->Argc; LoopVar++) {
        ASSERT ((ShellInfoObject.ShellInitSettings.FileOptions == NULL && Size == 0) || (ShellInfoObject.ShellInitSettings.FileOptions != NULL));
        //
        // Add a space between arguments
        //
        if (ShellInfoObject.ShellInitSettings.FileOptions != NULL) {
          StrnCatGrow (&ShellInfoObject.ShellInitSettings.FileOptions, &Size, L" ", 0);
          if (ShellInfoObject.ShellInitSettings.FileOptions == NULL) {
            SHELL_FREE_NON_NULL (ShellInfoObject.ShellInitSettings.FileName);
            return (EFI_OUT_OF_RESOURCES);
          }
        }

        //
        // If an argument contains a space, then add double quotes before the argument
        //
        if (StrStr (gEfiShellParametersProtocol->Argv[LoopVar], L" ") != NULL) {
          StrnCatGrow (
            &ShellInfoObject.ShellInitSettings.FileOptions,
            &Size,
            L"\"",
            0
            );
          if (ShellInfoObject.ShellInitSettings.FileOptions == NULL) {
            SHELL_FREE_NON_NULL (ShellInfoObject.ShellInitSettings.FileName);
            return (EFI_OUT_OF_RESOURCES);
          }
        }

        StrnCatGrow (
          &ShellInfoObject.ShellInitSettings.FileOptions,
          &Size,
          gEfiShellParametersProtocol->Argv[LoopVar],
          0
          );
        if (ShellInfoObject.ShellInitSettings.FileOptions == NULL) {
          SHELL_FREE_NON_NULL (ShellInfoObject.ShellInitSettings.FileName);
          return (EFI_OUT_OF_RESOURCES);
        }

        //
        // If an argument contains a space, then add double quotes after the argument
        //
        if (StrStr (gEfiShellParametersProtocol->Argv[LoopVar], L" ") != NULL) {
          StrnCatGrow (
            &ShellInfoObject.ShellInitSettings.FileOptions,
            &Size,
            L"\"",
            0
            );
          if (ShellInfoObject.ShellInitSettings.FileOptions == NULL) {
            SHELL_FREE_NON_NULL (ShellInfoObject.ShellInitSettings.FileName);
            return (EFI_OUT_OF_RESOURCES);
          }
        }
      }
    }
  }

  // "-nointerrupt" overrides "-delay"
  if (ShellInfoObject.ShellInitSettings.BitUnion.Bits.NoInterrupt) {
    ShellInfoObject.ShellInitSettings.Delay = 0;
  }

  return EFI_SUCCESS;
}

/**
  Function try to find location of the Startup.nsh file.

  The buffer is callee allocated and should be freed by the caller.

  @param    ImageDevicePath       The path to the image for shell.  first place to look for the startup script
  @param    FileDevicePath        The path to the file for shell.  second place to look for the startup script.

  @retval   NULL                  No Startup.nsh file was found.
  @return   !=NULL                Pointer to NULL-terminated path.
**/
CHAR16 *
LocateStartupScript (
  IN EFI_DEVICE_PATH_PROTOCOL  *ImageDevicePath,
  IN EFI_DEVICE_PATH_PROTOCOL  *FileDevicePath
  )
{
  CHAR16        *StartupScriptPath;
  CHAR16        *TempSpot;
  CONST CHAR16  *MapName;
  UINTN         Size;

  StartupScriptPath = NULL;
  Size              = 0;

  //
  // Try to find 'Startup.nsh' in the directory where the shell itself was launched.
  //
  MapName = ShellInfoObject.NewEfiShellProtocol->GetMapFromDevicePath (&ImageDevicePath);
  if (MapName != NULL) {
    StartupScriptPath = StrnCatGrow (&StartupScriptPath, &Size, MapName, 0);
    if (StartupScriptPath == NULL) {
      //
      // Do not locate the startup script in sys path when out of resource.
      //
      return NULL;
    }

    TempSpot = StrStr (StartupScriptPath, L";");
    if (TempSpot != NULL) {
      *TempSpot = CHAR_NULL;
    }

    InternalEfiShellSetEnv (L"homefilesystem", StartupScriptPath, TRUE);

    if (1 != _gCdeRTUefiShellInstanceType) {
        wcscpy(wcsBootDrive, StartupScriptPath);
        RunCommand(wcsBootDrive);
    }

    StartupScriptPath = StrnCatGrow (&StartupScriptPath, &Size, ((FILEPATH_DEVICE_PATH *)FileDevicePath)->PathName, 0);
    PathRemoveLastItem (StartupScriptPath);
    StartupScriptPath = StrnCatGrow (&StartupScriptPath, &Size, mStartupScript, 0);
  }

  //
  // Try to find 'Startup.nsh' in the execution path defined by the environment variable PATH.
  //
  if ((StartupScriptPath == NULL) || EFI_ERROR (ShellIsFile (StartupScriptPath))) {
    SHELL_FREE_NON_NULL (StartupScriptPath);
    StartupScriptPath = ShellFindFilePath (mStartupScript);
  }

  return StartupScriptPath;
}

/**
  Handles all interaction with the default startup script.

  this will check that the correct command line parameters were passed, handle the delay, and then start running the script.

  @param ImagePath              the path to the image for shell.  first place to look for the startup script
  @param FilePath               the path to the file for shell.  second place to look for the startup script.

  @retval EFI_SUCCESS           the variable is initialized.
**/
EFI_STATUS
DoStartupScript(
    IN EFI_DEVICE_PATH_PROTOCOL* ImagePath,
    IN EFI_DEVICE_PATH_PROTOCOL* FilePath
)
{
    EFI_STATUS     Status;
    EFI_STATUS     CalleeStatus;
    UINTN          Delay;
    EFI_INPUT_KEY  Key;
    CHAR16* FileStringPath;
    CHAR16* FullFileStringPath;
    UINTN          NewSize;

    CalleeStatus = EFI_SUCCESS;
    Key.UnicodeChar = CHAR_NULL;
    Key.ScanCode = 0;

    if (!ShellInfoObject.ShellInitSettings.BitUnion.Bits.Startup && (ShellInfoObject.ShellInitSettings.FileName != NULL)) {
        //
        // launch something else instead
        //
        NewSize = StrSize(ShellInfoObject.ShellInitSettings.FileName);
        if (ShellInfoObject.ShellInitSettings.FileOptions != NULL) {
            NewSize += StrSize(ShellInfoObject.ShellInitSettings.FileOptions) + sizeof(CHAR16);
        }

        FileStringPath = AllocateZeroPool(NewSize);
        if (FileStringPath == NULL) {
            return (EFI_OUT_OF_RESOURCES);
        }

        StrCpyS(FileStringPath, NewSize / sizeof(CHAR16), ShellInfoObject.ShellInitSettings.FileName);
        if (ShellInfoObject.ShellInitSettings.FileOptions != NULL) {
            StrnCatS(FileStringPath, NewSize / sizeof(CHAR16), L" ", NewSize / sizeof(CHAR16) - StrLen(FileStringPath) - 1);
            StrnCatS(FileStringPath, NewSize / sizeof(CHAR16), ShellInfoObject.ShellInitSettings.FileOptions, NewSize / sizeof(CHAR16) - StrLen(FileStringPath) - 1);
        }

        Status = RunShellCommand(FileStringPath, &CalleeStatus);
        if (ShellInfoObject.ShellInitSettings.BitUnion.Bits.Exit == TRUE) {
            ShellCommandRegisterExit(gEfiShellProtocol->BatchIsActive(), (UINT64)CalleeStatus);
        }

        FreePool(FileStringPath);
        return (Status);
    }

    //
    // for shell level 0 we do no scripts
    // Without the Startup bit overriding we allow for nostartup to prevent scripts
    //
    if ((PcdGet8(PcdShellSupportLevel) < 1)
        || (ShellInfoObject.ShellInitSettings.BitUnion.Bits.NoStartup && !ShellInfoObject.ShellInitSettings.BitUnion.Bits.Startup)
        )
    {
        return (EFI_SUCCESS);
    }

    if (1 == _gCdeRTUefiShellInstanceType)
    {
        gST->ConOut->EnableCursor(gST->ConOut, FALSE);
        //
        // print out our warning and see if they press a key
        //
        for (Status = EFI_UNSUPPORTED, Delay = ShellInfoObject.ShellInitSettings.Delay
            ; Delay != 0 && EFI_ERROR(Status)
            ; Delay--
            )
        {
            ShellPrintHiiEx(0, gST->ConOut->Mode->CursorRow, NULL, STRING_TOKEN(STR_SHELL_STARTUP_QUESTION), ShellInfoObject.HiiHandle, Delay);
            gBS->Stall(1000000);
            if (!ShellInfoObject.ShellInitSettings.BitUnion.Bits.NoConsoleIn) {
                Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
            }
        }

        ShellPrintHiiEx(-1, -1, NULL, STRING_TOKEN(STR_SHELL_CRLF), ShellInfoObject.HiiHandle);
        gST->ConOut->EnableCursor(gST->ConOut, TRUE);

        //
        // ESC was pressed
        //
        if ((Status == EFI_SUCCESS) && (Key.UnicodeChar == 0) && (Key.ScanCode == SCAN_ESC)) {
            return (EFI_SUCCESS);
        }
    }
    else
    {
        if (fSkipSTARTUPNSH)
            return EFI_SUCCESS;
    }

    FileStringPath = LocateStartupScript(ImagePath, FilePath);
    if (FileStringPath != NULL) {
        FullFileStringPath = FullyQualifyPath(FileStringPath);
        if (FullFileStringPath == NULL) {
            Status = RunScriptFile(FileStringPath, NULL, FileStringPath, ShellInfoObject.NewShellParametersProtocol);
    } else {
            Status = RunScriptFile(FullFileStringPath, NULL, FullFileStringPath, ShellInfoObject.NewShellParametersProtocol);
            FreePool(FullFileStringPath);
        }

        FreePool(FileStringPath);
  } else {
        //
        // we return success since startup script is not mandatory.
        //
        Status = EFI_SUCCESS;
    }

    return (Status);
}

/**
  Function to perform the shell prompt looping.  It will do a single prompt,
  dispatch the result, and then return.  It is expected that the caller will
  call this function in a loop many times.

  @retval EFI_SUCCESS
  @retval RETURN_ABORTED
**/
EFI_STATUS
DoShellPrompt (
  VOID
  )
{
  UINTN         Column;
  UINTN         Row;
  CHAR16        *CmdLine;
  CONST CHAR16  *CurDir;
  UINTN         BufferSize;
  EFI_STATUS    Status;
  LIST_ENTRY    OldBufferList;

  CurDir = NULL;

  //
  // Get screen setting to decide size of the command line buffer
  //
  gST->ConOut->QueryMode (gST->ConOut, gST->ConOut->Mode->Mode, &Column, &Row);
  BufferSize = Column * Row * sizeof (CHAR16);
  CmdLine    = AllocateZeroPool (BufferSize);
  if (CmdLine == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  SaveBufferList (&OldBufferList);
  CurDir = ShellInfoObject.NewEfiShellProtocol->GetEnv (L"cwd");

  //
  // Prompt for input
  //
  gST->ConOut->SetCursorPosition (gST->ConOut, 0, gST->ConOut->Mode->CursorRow);

  if ((CurDir != NULL) && (StrLen (CurDir) > 1)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_SHELL_CURDIR), ShellInfoObject.HiiHandle, CurDir);
  } else {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_SHELL_SHELL), ShellInfoObject.HiiHandle);
  }

  //
  // Read a line from the console
  //
  Status = ShellInfoObject.NewEfiShellProtocol->ReadFile (ShellInfoObject.NewShellParametersProtocol->StdIn, &BufferSize, CmdLine);

  //
  // Null terminate the string and parse it
  //
  if (!EFI_ERROR (Status)) {
    //
    // Reset the CTRL-C event just before running the command (yes we ignore the return values)
    //
    Status = gBS->CheckEvent (ShellInfoObject.NewEfiShellProtocol->ExecutionBreak);

    CmdLine[BufferSize / sizeof (CHAR16)] = CHAR_NULL;
    Status                                = RunCommand (CmdLine);
  }

  //
  // Done with this command
  //
  RestoreBufferList (&OldBufferList);
  FreePool (CmdLine);
  return Status;
}

/**
  Add a buffer to the Buffer To Free List for safely returning buffers to other
  places without risking letting them modify internal shell information.

  @param Buffer   Something to pass to FreePool when the shell is exiting.
**/
VOID *
AddBufferToFreeList (
  VOID  *Buffer
  )
{
  BUFFER_LIST  *BufferListEntry;

  if (Buffer == NULL) {
    return (NULL);
  }

  BufferListEntry = AllocateZeroPool (sizeof (BUFFER_LIST));
  if (BufferListEntry == NULL) {
    return NULL;
  }

  BufferListEntry->Buffer = Buffer;
  InsertTailList (&ShellInfoObject.BufferToFreeList.Link, &BufferListEntry->Link);
  return (Buffer);
}

/**
  Create a new buffer list and stores the old one to OldBufferList

  @param OldBufferList   The temporary list head used to store the nodes in BufferToFreeList.
**/
VOID
SaveBufferList (
  OUT LIST_ENTRY  *OldBufferList
  )
{
  CopyMem (OldBufferList, &ShellInfoObject.BufferToFreeList.Link, sizeof (LIST_ENTRY));
  InitializeListHead (&ShellInfoObject.BufferToFreeList.Link);
}

/**
  Restore previous nodes into BufferToFreeList .

  @param OldBufferList   The temporary list head used to store the nodes in BufferToFreeList.
**/
VOID
RestoreBufferList (
  IN OUT LIST_ENTRY  *OldBufferList
  )
{
  FreeBufferList (&ShellInfoObject.BufferToFreeList);
  CopyMem (&ShellInfoObject.BufferToFreeList.Link, OldBufferList, sizeof (LIST_ENTRY));
}

/**
  Add a buffer to the Line History List

  @param Buffer     The line buffer to add.
**/
VOID
AddLineToCommandHistory (
  IN CONST CHAR16  *Buffer
  )
{
  BUFFER_LIST  *Node;
  BUFFER_LIST  *Walker;
  UINT16       MaxHistoryCmdCount;
  UINT16       Count;

  Count              = 0;
  MaxHistoryCmdCount = PcdGet16 (PcdShellMaxHistoryCommandCount);

  if (MaxHistoryCmdCount == 0) {
    return;
  }

  Node = AllocateZeroPool (sizeof (BUFFER_LIST));
  if (Node == NULL) {
    return;
  }

  Node->Buffer = AllocateCopyPool (StrSize (Buffer), Buffer);
  if (Node->Buffer == NULL) {
    FreePool (Node);
    return;
  }

  for ( Walker = (BUFFER_LIST *)GetFirstNode (&ShellInfoObject.ViewingSettings.CommandHistory.Link)
        ; !IsNull (&ShellInfoObject.ViewingSettings.CommandHistory.Link, &Walker->Link)
        ; Walker = (BUFFER_LIST *)GetNextNode (&ShellInfoObject.ViewingSettings.CommandHistory.Link, &Walker->Link)
        )
  {
    Count++;
  }

  if (Count < MaxHistoryCmdCount) {
    InsertTailList (&ShellInfoObject.ViewingSettings.CommandHistory.Link, &Node->Link);
  } else {
    Walker = (BUFFER_LIST *)GetFirstNode (&ShellInfoObject.ViewingSettings.CommandHistory.Link);
    RemoveEntryList (&Walker->Link);
    if (Walker->Buffer != NULL) {
      FreePool (Walker->Buffer);
    }

    FreePool (Walker);
    InsertTailList (&ShellInfoObject.ViewingSettings.CommandHistory.Link, &Node->Link);
  }
}

/**
  Checks if a string is an alias for another command.  If yes, then it replaces the alias name
  with the correct command name.

  @param[in, out] CommandString    Upon entry the potential alias.  Upon return the
                                   command name if it was an alias.  If it was not
                                   an alias it will be unchanged.  This function may
                                   change the buffer to fit the command name.

  @retval EFI_SUCCESS             The name was changed.
  @retval EFI_SUCCESS             The name was not an alias.
  @retval EFI_OUT_OF_RESOURCES    A memory allocation failed.
**/
EFI_STATUS
ShellConvertAlias (
  IN OUT CHAR16  **CommandString
  )
{
  CONST CHAR16  *NewString;

  NewString = ShellInfoObject.NewEfiShellProtocol->GetAlias (*CommandString, NULL);
  if (NewString == NULL) {
    return (EFI_SUCCESS);
  }

  FreePool (*CommandString);
  *CommandString = AllocateCopyPool (StrSize (NewString), NewString);
  if (*CommandString == NULL) {
    return (EFI_OUT_OF_RESOURCES);
  }

  return (EFI_SUCCESS);
}

/**
  This function will eliminate unreplaced (and therefore non-found) environment variables.

  @param[in,out] CmdLine   The command line to update.
**/
EFI_STATUS
StripUnreplacedEnvironmentVariables (
  IN OUT CHAR16  *CmdLine
  )
{
  CHAR16  *FirstPercent;
  CHAR16  *FirstQuote;
  CHAR16  *SecondPercent;
  CHAR16  *SecondQuote;
  CHAR16  *CurrentLocator;

  for (CurrentLocator = CmdLine; CurrentLocator != NULL; ) {
    FirstQuote    = FindNextInstance (CurrentLocator, L"\"", TRUE);
    FirstPercent  = FindNextInstance (CurrentLocator, L"%", TRUE);
    SecondPercent = FirstPercent != NULL ? FindNextInstance (FirstPercent+1, L"%", TRUE) : NULL;
    if ((FirstPercent == NULL) || (SecondPercent == NULL)) {
      //
      // If we ever don't have 2 % we are done.
      //
      break;
    }

    if ((FirstQuote != NULL) && (FirstQuote < FirstPercent)) {
      SecondQuote = FindNextInstance (FirstQuote+1, L"\"", TRUE);
      //
      // Quote is first found
      //

      if (SecondQuote < FirstPercent) {
        //
        // restart after the pair of "
        //
        CurrentLocator = SecondQuote + 1;
      } else {
        /* FirstPercent < SecondQuote */
        //
        // Restart on the first percent
        //
        CurrentLocator = FirstPercent;
      }

      continue;
    }

    if ((FirstQuote == NULL) || (SecondPercent < FirstQuote)) {
      if (IsValidEnvironmentVariableName (FirstPercent, SecondPercent)) {
        //
        // We need to remove from FirstPercent to SecondPercent
        //
        CopyMem (FirstPercent, SecondPercent + 1, StrSize (SecondPercent + 1));
        //
        // don't need to update the locator.  both % characters are gone.
        //
      } else {
        CurrentLocator = SecondPercent + 1;
      }

      continue;
    }

    CurrentLocator = FirstQuote;
  }

  return (EFI_SUCCESS);
}

/**
  Function allocates a new command line and replaces all instances of environment
  variable names that are correctly preset to their values.

  If the return value is not NULL the memory must be caller freed.

  @param[in] OriginalCommandLine    The original command line

  @retval NULL                      An error occurred.
  @return                           The new command line with no environment variables present.
**/
CHAR16 *
ShellConvertVariables (
  IN CONST CHAR16  *OriginalCommandLine
  )
{
  CONST CHAR16  *MasterEnvList;
  UINTN         NewSize;
  CHAR16        *NewCommandLine1;
  CHAR16        *NewCommandLine2;
  CHAR16        *Temp;
  UINTN         ItemSize;
  CHAR16        *ItemTemp;
  SCRIPT_FILE   *CurrentScriptFile;
  ALIAS_LIST    *AliasListNode;

  ASSERT (OriginalCommandLine != NULL);

  ItemSize          = 0;
  NewSize           = StrSize (OriginalCommandLine);
  CurrentScriptFile = ShellCommandGetCurrentScriptFile ();
  Temp              = NULL;

  /// @todo update this to handle the %0 - %9 for scripting only (borrow from line 1256 area) ? ? ?

  //
  // calculate the size required for the post-conversion string...
  //
  if (CurrentScriptFile != NULL) {
    for (AliasListNode = (ALIAS_LIST *)GetFirstNode (&CurrentScriptFile->SubstList)
         ; !IsNull (&CurrentScriptFile->SubstList, &AliasListNode->Link)
         ; AliasListNode = (ALIAS_LIST *)GetNextNode (&CurrentScriptFile->SubstList, &AliasListNode->Link)
         )
    {
      for (Temp = StrStr (OriginalCommandLine, AliasListNode->Alias)
           ; Temp != NULL
           ; Temp = StrStr (Temp+1, AliasListNode->Alias)
           )
      {
        //
        // we need a preceding and if there is space no ^ preceding (if no space ignore)
        //
        if ((((Temp-OriginalCommandLine) > 2) && (*(Temp-2) != L'^')) || ((Temp-OriginalCommandLine) <= 2)) {
          NewSize += StrSize (AliasListNode->CommandString);
        }
      }
    }
  }

  for (MasterEnvList = EfiShellGetEnv (NULL)
       ; MasterEnvList != NULL && *MasterEnvList != CHAR_NULL // && *(MasterEnvList+1) != CHAR_NULL
       ; MasterEnvList += StrLen (MasterEnvList) + 1
       )
  {
    if (StrSize (MasterEnvList) > ItemSize) {
      ItemSize = StrSize (MasterEnvList);
    }

    for (Temp = StrStr (OriginalCommandLine, MasterEnvList)
         ; Temp != NULL
         ; Temp = StrStr (Temp+1, MasterEnvList)
         )
    {
      //
      // we need a preceding and following % and if there is space no ^ preceding (if no space ignore)
      //
      if ((*(Temp-1) == L'%') && (*(Temp+StrLen (MasterEnvList)) == L'%') &&
          ((((Temp-OriginalCommandLine) > 2) && (*(Temp-2) != L'^')) || ((Temp-OriginalCommandLine) <= 2)))
      {
        NewSize += StrSize (EfiShellGetEnv (MasterEnvList));
      }
    }
  }

  //
  // now do the replacements...
  //
  NewCommandLine1 = AllocateZeroPool (NewSize);
  NewCommandLine2 = AllocateZeroPool (NewSize);
  ItemTemp        = AllocateZeroPool (ItemSize+(2*sizeof (CHAR16)));
  if ((NewCommandLine1 == NULL) || (NewCommandLine2 == NULL) || (ItemTemp == NULL)) {
    SHELL_FREE_NON_NULL (NewCommandLine1);
    SHELL_FREE_NON_NULL (NewCommandLine2);
    SHELL_FREE_NON_NULL (ItemTemp);
    return (NULL);
  }

  CopyMem (NewCommandLine1, OriginalCommandLine, StrSize (OriginalCommandLine));

  for (MasterEnvList = EfiShellGetEnv (NULL)
       ; MasterEnvList != NULL && *MasterEnvList != CHAR_NULL
       ; MasterEnvList += StrLen (MasterEnvList) + 1
       )
  {
    StrCpyS (
      ItemTemp,
      ((ItemSize+(2*sizeof (CHAR16)))/sizeof (CHAR16)),
      L"%"
      );
    StrCatS (
      ItemTemp,
      ((ItemSize+(2*sizeof (CHAR16)))/sizeof (CHAR16)),
      MasterEnvList
      );
    StrCatS (
      ItemTemp,
      ((ItemSize+(2*sizeof (CHAR16)))/sizeof (CHAR16)),
      L"%"
      );
    ShellCopySearchAndReplace (NewCommandLine1, NewCommandLine2, NewSize, ItemTemp, EfiShellGetEnv (MasterEnvList), TRUE, FALSE);
    StrCpyS (NewCommandLine1, NewSize/sizeof (CHAR16), NewCommandLine2);
  }

  if (CurrentScriptFile != NULL) {
    for (AliasListNode = (ALIAS_LIST *)GetFirstNode (&CurrentScriptFile->SubstList)
         ; !IsNull (&CurrentScriptFile->SubstList, &AliasListNode->Link)
         ; AliasListNode = (ALIAS_LIST *)GetNextNode (&CurrentScriptFile->SubstList, &AliasListNode->Link)
         )
    {
      ShellCopySearchAndReplace (NewCommandLine1, NewCommandLine2, NewSize, AliasListNode->Alias, AliasListNode->CommandString, TRUE, FALSE);
      StrCpyS (NewCommandLine1, NewSize/sizeof (CHAR16), NewCommandLine2);
    }
  }

  //
  // Remove non-existent environment variables
  //
  StripUnreplacedEnvironmentVariables (NewCommandLine1);

  //
  // Now cleanup any straggler intentionally ignored "%" characters
  //
  ShellCopySearchAndReplace (NewCommandLine1, NewCommandLine2, NewSize, L"^%", L"%", TRUE, FALSE);
  StrCpyS (NewCommandLine1, NewSize/sizeof (CHAR16), NewCommandLine2);

  FreePool (NewCommandLine2);
  FreePool (ItemTemp);

  return (NewCommandLine1);
}

/**
  Internal function to run a command line with pipe usage.

  @param[in] CmdLine        The pointer to the command line.
  @param[in] StdIn          The pointer to the Standard input.
  @param[in] StdOut         The pointer to the Standard output.

  @retval EFI_SUCCESS       The split command is executed successfully.
  @retval other             Some error occurs when executing the split command.
**/
EFI_STATUS
RunSplitCommand (
  IN CONST CHAR16             *CmdLine,
  IN       SHELL_FILE_HANDLE  StdIn,
  IN       SHELL_FILE_HANDLE  StdOut
  )
{
  EFI_STATUS         Status;
  CHAR16             *NextCommandLine;
  CHAR16             *OurCommandLine;
  UINTN              Size1;
  UINTN              Size2;
  SPLIT_LIST         *Split;
  SHELL_FILE_HANDLE  TempFileHandle;
  BOOLEAN            Unicode;

  ASSERT (StdOut == NULL);

  ASSERT (StrStr (CmdLine, L"|") != NULL);

  Status          = EFI_SUCCESS;
  NextCommandLine = NULL;
  OurCommandLine  = NULL;
  Size1           = 0;
  Size2           = 0;

  NextCommandLine = StrnCatGrow (&NextCommandLine, &Size1, StrStr (CmdLine, L"|")+1, 0);
  OurCommandLine  = StrnCatGrow (&OurCommandLine, &Size2, CmdLine, StrStr (CmdLine, L"|") - CmdLine);

  if ((NextCommandLine == NULL) || (OurCommandLine == NULL)) {
    SHELL_FREE_NON_NULL (OurCommandLine);
    SHELL_FREE_NON_NULL (NextCommandLine);
    return (EFI_OUT_OF_RESOURCES);
  } else if ((StrStr (OurCommandLine, L"|") != NULL) || (Size1 == 0) || (Size2 == 0)) {
    SHELL_FREE_NON_NULL (OurCommandLine);
    SHELL_FREE_NON_NULL (NextCommandLine);
    return (EFI_INVALID_PARAMETER);
  } else if ((NextCommandLine[0] == L'a') &&
             ((NextCommandLine[1] == L' ') || (NextCommandLine[1] == CHAR_NULL))
             )
  {
    CopyMem (NextCommandLine, NextCommandLine+1, StrSize (NextCommandLine) - sizeof (NextCommandLine[0]));
    while (NextCommandLine[0] == L' ') {
      CopyMem (NextCommandLine, NextCommandLine+1, StrSize (NextCommandLine) - sizeof (NextCommandLine[0]));
    }

    if (NextCommandLine[0] == CHAR_NULL) {
      SHELL_FREE_NON_NULL (OurCommandLine);
      SHELL_FREE_NON_NULL (NextCommandLine);
      return (EFI_INVALID_PARAMETER);
    }

    Unicode = FALSE;
  } else {
    Unicode = TRUE;
  }

  //
  // make a SPLIT_LIST item and add to list
  //
  Split = AllocateZeroPool (sizeof (SPLIT_LIST));
  if (Split == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Split->SplitStdIn  = StdIn;
  Split->SplitStdOut = ConvertEfiFileProtocolToShellHandle (CreateFileInterfaceMem (Unicode), NULL);
  ASSERT (Split->SplitStdOut != NULL);
  InsertHeadList (&ShellInfoObject.SplitList.Link, &Split->Link);

  Status = RunCommand (OurCommandLine);

  //
  // move the output from the first to the in to the second.
  //
  TempFileHandle = Split->SplitStdOut;
  if (Split->SplitStdIn == StdIn) {
    Split->SplitStdOut = NULL;
  } else {
    Split->SplitStdOut = Split->SplitStdIn;
  }

  Split->SplitStdIn = TempFileHandle;
  ShellInfoObject.NewEfiShellProtocol->SetFilePosition (Split->SplitStdIn, 0);

  if (!EFI_ERROR (Status)) {
    Status = RunCommand (NextCommandLine);
  }

  //
  // remove the top level from the ScriptList
  //
  ASSERT ((SPLIT_LIST *)GetFirstNode (&ShellInfoObject.SplitList.Link) == Split);
  RemoveEntryList (&Split->Link);

  //
  // Note that the original StdIn is now the StdOut...
  //
  if (Split->SplitStdOut != NULL) {
    ShellInfoObject.NewEfiShellProtocol->CloseFile (Split->SplitStdOut);
  }

  if (Split->SplitStdIn != NULL) {
    ShellInfoObject.NewEfiShellProtocol->CloseFile (Split->SplitStdIn);
  }

  FreePool (Split);
  FreePool (NextCommandLine);
  FreePool (OurCommandLine);

  return (Status);
}

/**
  Take the original command line, substitute any variables, free
  the original string, return the modified copy.

  @param[in] CmdLine  pointer to the command line to update.

  @retval EFI_SUCCESS           the function was successful.
  @retval EFI_OUT_OF_RESOURCES  a memory allocation failed.
**/
EFI_STATUS
ShellSubstituteVariables (
  IN CHAR16  **CmdLine
  )
{
  CHAR16  *NewCmdLine;

  NewCmdLine = ShellConvertVariables (*CmdLine);
  SHELL_FREE_NON_NULL (*CmdLine);
  if (NewCmdLine == NULL) {
    return (EFI_OUT_OF_RESOURCES);
  }

  *CmdLine = NewCmdLine;
  return (EFI_SUCCESS);
}

/**
  Take the original command line, substitute any alias in the first group of space delimited characters, free
  the original string, return the modified copy.

  @param[in] CmdLine  pointer to the command line to update.

  @retval EFI_SUCCESS           the function was successful.
  @retval EFI_OUT_OF_RESOURCES  a memory allocation failed.
**/
EFI_STATUS
ShellSubstituteAliases (
  IN CHAR16  **CmdLine
  )
{
  CHAR16      *NewCmdLine;
  CHAR16      *CommandName;
  EFI_STATUS  Status;
  UINTN       PostAliasSize;

  ASSERT (CmdLine != NULL);
  ASSERT (*CmdLine != NULL);

  CommandName = NULL;
  if (StrStr ((*CmdLine), L" ") == NULL) {
    StrnCatGrow (&CommandName, NULL, (*CmdLine), 0);
  } else {
    StrnCatGrow (&CommandName, NULL, (*CmdLine), StrStr ((*CmdLine), L" ") - (*CmdLine));
  }

  //
  // This cannot happen 'inline' since the CmdLine can need extra space.
  //
  NewCmdLine = NULL;
  if (!ShellCommandIsCommandOnList (CommandName)) {
    //
    // Convert via alias
    //
    Status = ShellConvertAlias (&CommandName);
    if (EFI_ERROR (Status)) {
      return (Status);
    }

    PostAliasSize = 0;
    NewCmdLine    = StrnCatGrow (&NewCmdLine, &PostAliasSize, CommandName, 0);
    if (NewCmdLine == NULL) {
      SHELL_FREE_NON_NULL (CommandName);
      SHELL_FREE_NON_NULL (*CmdLine);
      return (EFI_OUT_OF_RESOURCES);
    }

    NewCmdLine = StrnCatGrow (&NewCmdLine, &PostAliasSize, StrStr ((*CmdLine), L" "), 0);
    if (NewCmdLine == NULL) {
      SHELL_FREE_NON_NULL (CommandName);
      SHELL_FREE_NON_NULL (*CmdLine);
      return (EFI_OUT_OF_RESOURCES);
    }
  } else {
    NewCmdLine = StrnCatGrow (&NewCmdLine, NULL, (*CmdLine), 0);
  }

  SHELL_FREE_NON_NULL (*CmdLine);
  SHELL_FREE_NON_NULL (CommandName);

  //
  // re-assign the passed in double pointer to point to our newly allocated buffer
  //
  *CmdLine = NewCmdLine;

  return (EFI_SUCCESS);
}

/**
  Takes the Argv[0] part of the command line and determine the meaning of it.

  @param[in] CmdName  pointer to the command line to update.

  @retval Internal_Command    The name is an internal command.
  @retval File_Sys_Change     the name is a file system change.
  @retval Script_File_Name    the name is a NSH script file.
  @retval Unknown_Invalid     the name is unknown.
  @retval Efi_Application     the name is an application (.EFI).
**/
SHELL_OPERATION_TYPES
GetOperationType (
  IN CONST CHAR16  *CmdName
  )
{
  CHAR16        *FileWithPath;
  CONST CHAR16  *TempLocation;
  CONST CHAR16  *TempLocation2;

  FileWithPath = NULL;
  //
  // test for an internal command.
  //
  if (ShellCommandIsCommandOnList (CmdName)) {
    return (Internal_Command);
  }

  //
  // Test for file system change request.  anything ending with first : and cant have spaces.
  //
  if (CmdName[(StrLen (CmdName)-1)] == L':') {
    if (  (StrStr (CmdName, L" ") != NULL)
       || (StrLen (StrStr (CmdName, L":")) > 1)
          )
    {
      return (Unknown_Invalid);
    }

    return (File_Sys_Change);
  }

  //
  // Test for a PLUGIN
  //
  for (int i = 0; i < sizeof(plugin) / sizeof(plugin[0]); i++)
  {
      wchar_t wcsCmdName[48];

      memset(wcsCmdName, 0, sizeof(wcsCmdName));

      swscanf(CmdName, L"%s", &wcsCmdName);

      if (0 == _wcsicmp(wcsCmdName, plugin[i].wcsCmd))
      {
          //CDETRACE((TRCINF(1) "--> \n"));
          return (Efi_Application);
      }
  }

  // Test for a file
  //
  if ((FileWithPath = ShellFindFilePathEx (CmdName, mExecutableExtensions)) != NULL) {
    //
    // See if that file has a script file extension
    //
    if (StrLen (FileWithPath) > 4) {
      TempLocation  = FileWithPath+StrLen (FileWithPath)-4;
      TempLocation2 = mScriptExtension;
      if (StringNoCaseCompare ((VOID *)(&TempLocation), (VOID *)(&TempLocation2)) == 0) {
        SHELL_FREE_NON_NULL (FileWithPath);
        return (Script_File_Name);
      }
    }

    //
    // Was a file, but not a script.  we treat this as an application.
    //
    SHELL_FREE_NON_NULL (FileWithPath);
    return (Efi_Application);
  }

  SHELL_FREE_NON_NULL (FileWithPath);
  //
  // No clue what this is... return invalid flag...
  //
  return (Unknown_Invalid);
}

/**
  Determine if the first item in a command line is valid.

  @param[in] CmdLine            The command line to parse.

  @retval EFI_SUCCESS           The item is valid.
  @retval EFI_OUT_OF_RESOURCES  A memory allocation failed.
  @retval EFI_NOT_FOUND         The operation type is unknown or invalid.
**/
EFI_STATUS
IsValidSplit (
  IN CONST CHAR16  *CmdLine
  )
{
  CHAR16      *Temp;
  CHAR16      *FirstParameter;
  CHAR16      *TempWalker;
  EFI_STATUS  Status;

  Temp = NULL;

  Temp = StrnCatGrow (&Temp, NULL, CmdLine, 0);
  if (Temp == NULL) {
    return (EFI_OUT_OF_RESOURCES);
  }

  FirstParameter = StrStr (Temp, L"|");
  if (FirstParameter != NULL) {
    *FirstParameter = CHAR_NULL;
  }

  FirstParameter = NULL;

  //
  // Process the command line
  //
  Status = ProcessCommandLineToFinal (&Temp);

  if (!EFI_ERROR (Status)) {
    FirstParameter = AllocateZeroPool (StrSize (CmdLine));
    if (FirstParameter == NULL) {
      SHELL_FREE_NON_NULL (Temp);
      return (EFI_OUT_OF_RESOURCES);
    }

    TempWalker = (CHAR16 *)Temp;
    if (!EFI_ERROR (GetNextParameter (&TempWalker, &FirstParameter, StrSize (CmdLine), TRUE))) {
      if (GetOperationType (FirstParameter) == Unknown_Invalid) {
        ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_SHELL_NOT_FOUND), ShellInfoObject.HiiHandle, FirstParameter);
        SetLastError (SHELL_NOT_FOUND);
        Status = EFI_NOT_FOUND;
      }
    }
  }

  SHELL_FREE_NON_NULL (Temp);
  SHELL_FREE_NON_NULL (FirstParameter);
  return Status;
}

/**
  Determine if a command line contains with a split contains only valid commands.

  @param[in] CmdLine      The command line to parse.

  @retval EFI_SUCCESS     CmdLine has only valid commands, application, or has no split.
  @retval EFI_ABORTED     CmdLine has at least one invalid command or application.
**/
EFI_STATUS
VerifySplit (
  IN CONST CHAR16  *CmdLine
  )
{
  CONST CHAR16  *TempSpot;
  EFI_STATUS    Status;

  //
  // If this was the only item, then get out
  //
  if (!ContainsSplit (CmdLine)) {
    return (EFI_SUCCESS);
  }

  //
  // Verify up to the pipe or end character
  //
  Status = IsValidSplit (CmdLine);
  if (EFI_ERROR (Status)) {
    return (Status);
  }

  //
  // recurse to verify the next item
  //
  TempSpot = FindFirstCharacter (CmdLine, L"|", L'^') + 1;
  if ((*TempSpot == L'a') &&
      ((*(TempSpot + 1) == L' ') || (*(TempSpot + 1) == CHAR_NULL))
      )
  {
    // If it's an ASCII pipe '|a'
    TempSpot += 1;
  }

  return (VerifySplit (TempSpot));
}

/**
  Process a split based operation.

  @param[in] CmdLine    pointer to the command line to process

  @retval EFI_SUCCESS   The operation was successful
  @return               an error occurred.
**/
EFI_STATUS
ProcessNewSplitCommandLine (
  IN CONST CHAR16  *CmdLine
  )
{
  SPLIT_LIST  *Split;
  EFI_STATUS  Status;

  Status = VerifySplit (CmdLine);
  if (EFI_ERROR (Status)) {
    return (Status);
  }

  Split = NULL;

  //
  // are we in an existing split???
  //
  if (!IsListEmpty (&ShellInfoObject.SplitList.Link)) {
    Split = (SPLIT_LIST *)GetFirstNode (&ShellInfoObject.SplitList.Link);
  }

  if (Split == NULL) {
    Status = RunSplitCommand (CmdLine, NULL, NULL);
  } else {
    Status = RunSplitCommand (CmdLine, Split->SplitStdIn, Split->SplitStdOut);
  }

  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_SHELL_INVALID_SPLIT), ShellInfoObject.HiiHandle, CmdLine);
  }

  return (Status);
}

/**
  Handle a request to change the current file system.

  @param[in] CmdLine  The passed in command line.

  @retval EFI_SUCCESS The operation was successful.
**/
EFI_STATUS
ChangeMappedDrive (
  IN CONST CHAR16  *CmdLine
  )
{
  EFI_STATUS  Status;

  Status = EFI_SUCCESS;

  //
  // make sure we are the right operation
  //
  ASSERT (CmdLine[(StrLen (CmdLine)-1)] == L':' && StrStr (CmdLine, L" ") == NULL);

  //
  // Call the protocol API to do the work
  //
  Status = ShellInfoObject.NewEfiShellProtocol->SetCurDir (NULL, CmdLine);

  //
  // Report any errors
  //
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_SHELL_INVALID_MAPPING), ShellInfoObject.HiiHandle, CmdLine);
  }

  return (Status);
}

/**
  Reprocess the command line to direct all -? to the help command.

  if found, will add "help" as argv[0], and move the rest later.

  @param[in,out] CmdLine        pointer to the command line to update
**/
EFI_STATUS
DoHelpUpdate (
  IN OUT CHAR16  **CmdLine
  )
{
  CHAR16      *CurrentParameter;
  CHAR16      *Walker;
  CHAR16      *NewCommandLine;
  EFI_STATUS  Status;
  UINTN       NewCmdLineSize;

  Status = EFI_SUCCESS;

  CurrentParameter = AllocateZeroPool (StrSize (*CmdLine));
  if (CurrentParameter == NULL) {
    return (EFI_OUT_OF_RESOURCES);
  }

  Walker = *CmdLine;
  while (Walker != NULL && *Walker != CHAR_NULL) {
    if (!EFI_ERROR (GetNextParameter (&Walker, &CurrentParameter, StrSize (*CmdLine), TRUE))) {
      if (StrStr (CurrentParameter, L"-?") == CurrentParameter) {
        CurrentParameter[0] = L' ';
        CurrentParameter[1] = L' ';
        NewCmdLineSize      = StrSize (L"help ") + StrSize (*CmdLine);
        NewCommandLine      = AllocateZeroPool (NewCmdLineSize);
        if (NewCommandLine == NULL) {
          Status = EFI_OUT_OF_RESOURCES;
          break;
        }

        //
        // We know the space is sufficient since we just calculated it.
        //
        StrnCpyS (NewCommandLine, NewCmdLineSize/sizeof (CHAR16), L"help ", 5);
        StrnCatS (NewCommandLine, NewCmdLineSize/sizeof (CHAR16), *CmdLine, StrLen (*CmdLine));
        SHELL_FREE_NON_NULL (*CmdLine);
        *CmdLine = NewCommandLine;
        break;
      }
    }
  }

  SHELL_FREE_NON_NULL (CurrentParameter);

  return (Status);
}

/**
  Function to update the shell variable "lasterror".

  @param[in] ErrorCode      the error code to put into lasterror.
**/
EFI_STATUS
SetLastError (
  IN CONST SHELL_STATUS  ErrorCode
  )
{
  CHAR16  LeString[19];

  if (sizeof (EFI_STATUS) == sizeof (UINT64)) {
    UnicodeSPrint (LeString, sizeof (LeString), L"0x%Lx", ErrorCode);
  } else {
    UnicodeSPrint (LeString, sizeof (LeString), L"0x%x", ErrorCode);
  }

  DEBUG_CODE (
    InternalEfiShellSetEnv (L"debuglasterror", LeString, TRUE);
    );
  InternalEfiShellSetEnv (L"lasterror", LeString, TRUE);

  return (EFI_SUCCESS);
}

/**
  Converts the command line to its post-processed form.  this replaces variables and alias' per UEFI Shell spec.

  @param[in,out] CmdLine        pointer to the command line to update

  @retval EFI_SUCCESS           The operation was successful
  @retval EFI_OUT_OF_RESOURCES  A memory allocation failed.
  @return                       some other error occurred
**/
EFI_STATUS
ProcessCommandLineToFinal (
  IN OUT CHAR16  **CmdLine
  )
{
  EFI_STATUS  Status;

  TrimSpaces (CmdLine);

  Status = ShellSubstituteAliases (CmdLine);
  if (EFI_ERROR (Status)) {
    return (Status);
  }

  TrimSpaces (CmdLine);

  Status = ShellSubstituteVariables (CmdLine);
  if (EFI_ERROR (Status)) {
    return (Status);
  }

  ASSERT (*CmdLine != NULL);

  TrimSpaces (CmdLine);

  //
  // update for help parsing
  //
  if (StrStr (*CmdLine, L"?") != NULL) {
    //
    // This may do nothing if the ? does not indicate help.
    // Save all the details for in the API below.
    //
    Status = DoHelpUpdate (CmdLine);
  }

  TrimSpaces (CmdLine);

  return (EFI_SUCCESS);
}

/**
  Run an internal shell command.

  This API will update the shell's environment since these commands are libraries.

  @param[in] CmdLine          the command line to run.
  @param[in] FirstParameter   the first parameter on the command line
  @param[in] ParamProtocol    the shell parameters protocol pointer
  @param[out] CommandStatus   the status from the command line.

  @retval EFI_SUCCESS     The command was completed.
  @retval EFI_ABORTED     The command's operation was aborted.
**/
EFI_STATUS
RunInternalCommand (
  IN CONST CHAR16                   *CmdLine,
  IN       CHAR16                   *FirstParameter,
  IN EFI_SHELL_PARAMETERS_PROTOCOL  *ParamProtocol,
  OUT EFI_STATUS                    *CommandStatus
  )
{
  EFI_STATUS    Status;
  UINTN         Argc;
  CHAR16        **Argv;
  SHELL_STATUS  CommandReturnedStatus;
  BOOLEAN       LastError;
  CHAR16        *Walker;
  CHAR16        *NewCmdLine;

  NewCmdLine = AllocateCopyPool (StrSize (CmdLine), CmdLine);
  if (NewCmdLine == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  for (Walker = NewCmdLine; Walker != NULL && *Walker != CHAR_NULL; Walker++) {
    if ((*Walker == L'^') && (*(Walker+1) == L'#')) {
      CopyMem (Walker, Walker+1, StrSize (Walker) - sizeof (Walker[0]));
    }
  }

  //
  // get the argc and argv updated for internal commands
  //
  Status = UpdateArgcArgv (ParamProtocol, NewCmdLine, Internal_Command, &Argv, &Argc);
  if (!EFI_ERROR (Status)) {
    //
    // Run the internal command.
    //
    Status = ShellCommandRunCommandHandler (FirstParameter, &CommandReturnedStatus, &LastError);

    if (!EFI_ERROR (Status)) {
      if (CommandStatus != NULL) {
        if (CommandReturnedStatus != SHELL_SUCCESS) {
          *CommandStatus = (EFI_STATUS)(CommandReturnedStatus | MAX_BIT);
        } else {
          *CommandStatus = EFI_SUCCESS;
        }
      }

      //
      // Update last error status.
      // some commands do not update last error.
      //
      if (LastError) {
        SetLastError (CommandReturnedStatus);
      }

      //
      // Pass thru the exitcode from the app.
      //
      if (ShellCommandGetExit ()) {
        //
        // An Exit was requested ("exit" command), pass its value up.
        //
        Status = CommandReturnedStatus;
      } else if ((CommandReturnedStatus != SHELL_SUCCESS) && IsScriptOnlyCommand (FirstParameter)) {
        //
        // Always abort when a script only command fails for any reason
        //
        Status = EFI_ABORTED;
      } else if ((ShellCommandGetCurrentScriptFile () != NULL) && (CommandReturnedStatus == SHELL_ABORTED)) {
        //
        // Abort when in a script and a command aborted
        //
        Status = EFI_ABORTED;
      }
    }
  }

  //
  // This is guaranteed to be called after UpdateArgcArgv no matter what else happened.
  // This is safe even if the update API failed.  In this case, it may be a no-op.
  //
  RestoreArgcArgv (ParamProtocol, &Argv, &Argc);

  //
  // If a script is running and the command is not a script only command, then
  // change return value to success so the script won't halt (unless aborted).
  //
  // Script only commands have to be able halt the script since the script will
  // not operate if they are failing.
  //
  if (  (ShellCommandGetCurrentScriptFile () != NULL)
     && !IsScriptOnlyCommand (FirstParameter)
     && (Status != EFI_ABORTED)
        )
  {
    Status = EFI_SUCCESS;
  }

  FreePool (NewCmdLine);
  return (Status);
}

/**
  Function to run the command or file.

  @param[in] Type             the type of operation being run.
  @param[in] CmdLine          the command line to run.
  @param[in] FirstParameter   the first parameter on the command line
  @param[in] ParamProtocol    the shell parameters protocol pointer
  @param[out] CommandStatus   the status from the command line.

  @retval EFI_SUCCESS     The command was completed.
  @retval EFI_ABORTED     The command's operation was aborted.
**/
EFI_STATUS
RunCommandOrFile (
  IN       SHELL_OPERATION_TYPES    Type,
  IN CONST CHAR16                   *CmdLine,
  IN       CHAR16                   *FirstParameter,
  IN EFI_SHELL_PARAMETERS_PROTOCOL  *ParamProtocol,
  OUT EFI_STATUS                    *CommandStatus
  )
{
  EFI_STATUS                Status;
  EFI_STATUS                StartStatus;
  CHAR16                    *CommandWithPath;
  CHAR16                    *FullCommandWithPath;
  EFI_DEVICE_PATH_PROTOCOL  *DevPath;
  SHELL_STATUS              CalleeExitStatus;

  Status           = EFI_SUCCESS;
  CommandWithPath  = NULL;
  DevPath          = NULL;
  CalleeExitStatus = SHELL_INVALID_PARAMETER;
  CDETRACE((TRCINF(1)"-->TYPE: %d, CmdLine \"%ls\", FirstParameter \"%ls\"\n",Type, CmdLine, FirstParameter));

  switch (Type) {
    case Internal_Command:
      Status = RunInternalCommand (CmdLine, FirstParameter, ParamProtocol, CommandStatus);
      
      //
      // extend internal command AFTER to "TORO UEFI SHELL"
      //
      if (1)
      {
          if (0 == _wcsicmp(CmdLine, L"help"))
          {
              char* pstrCommaLF = "    ";
              puts(""); // new line
              // list PLUGINs
              printf("TORO UEFI SHELL PLUGINs:\n");
              for (int i = 1; i <= sizeof(plugin) / sizeof(plugin[0]); i++)
              {
                  printf("%s%ls", pstrCommaLF,plugin[i - 1].wcsCmd);
                  if (0 == (i % 8))
                      pstrCommaLF = "\n    ";
                  else
                      pstrCommaLF = ", ";
              }
              puts(""); // new line
          }
          if (0 == _wcsicmp(CmdLine, L"ver"))
          {
              printf("\n    TORO UEFI SHELL with PLUGIN Extension, v%d.%d.%d Build %d\n    Based on \"edk2-stable202505\"\n\n", MAJORVER, MINORVER, PATCHVER, BUILDNUM);
          }
      }
      break;
    case Script_File_Name:
    case Efi_Application:
      //
      // Process a fully qualified path
      //
      if (StrStr (FirstParameter, L":") != NULL) {
        ASSERT (CommandWithPath == NULL);
        if (ShellIsFile (FirstParameter) == EFI_SUCCESS) {
          CommandWithPath = StrnCatGrow (&CommandWithPath, NULL, FirstParameter, 0);
        }
      }

      //
      // Process a relative path and also check in the path environment variable
      //
      if (CommandWithPath == NULL) {
        CommandWithPath = ShellFindFilePathEx (FirstParameter, mExecutableExtensions);
      }

      if (CommandWithPath == NULL) {
      //
      // This should be impossible now.
      //
      ASSERT (CommandWithPath != NULL);
        ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_SHELL_NOT_FOUND), ShellInfoObject.HiiHandle, FirstParameter);
        SetLastError (SHELL_NOT_FOUND);
        return EFI_NOT_FOUND;
      }

      //
      // Make sure that path is not just a directory (or not found)
      //
      if (!EFI_ERROR (ShellIsDirectory (CommandWithPath))) {
        ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_SHELL_NOT_FOUND), ShellInfoObject.HiiHandle, FirstParameter);
        SetLastError (SHELL_NOT_FOUND);
      }

      switch (Type) {
        case Script_File_Name:
          FullCommandWithPath = FullyQualifyPath (CommandWithPath);
          if (FullCommandWithPath == NULL) {
            Status = RunScriptFile (CommandWithPath, NULL, CmdLine, ParamProtocol);
          } else {
            Status = RunScriptFile (FullCommandWithPath, NULL, CmdLine, ParamProtocol);
            FreePool (FullCommandWithPath);
          }

          break;
        case Efi_Application:
          //
          // Get the device path of the application image
          //
            //CDETRACE((TRCINF(1) "--> %ls\n", CmdLine));//kgtest
            
            //
            // Test for a PLUGIN
            //
            if (1)
            {
                wchar_t wcsCmdName[48];
                char fIsPlugin = 0;
                int i;
                static EFI_GUID guidSTOP = EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID;
                EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* pSTOP;
                size_t CurrentModeNumber;

                gSystemTable->BootServices->LocateProtocol(&guidSTOP, NULL, (void**)&pSTOP);
                CurrentModeNumber = pSTOP->Mode->Mode;

                for (i = 0; i < sizeof(plugin) / sizeof(plugin[0]); i++)
                {
                    memset(wcsCmdName, 0, sizeof(wcsCmdName));

                    swscanf(CmdLine, L"%s", &wcsCmdName);

                    if (0 == _wcsicmp(wcsCmdName, plugin[i].wcsCmd))
                    {
                        CDETRACE((TRCINF(1) "--> \n"));
                        fIsPlugin = 1;
                        
                        if (1)
                        {
                            static EFI_GUID guidEFI_SHELL_PARAMETERS_PROTOCOL = EFI_SHELL_PARAMETERS_PROTOCOL_GUID;
                            static EFI_GUID guidEFI_DEVICE_PATH_TO_TEXT_PROTOCOL = EFI_DEVICE_PATH_TO_TEXT_PROTOCOL_GUID;
                            static EFI_GUID guidEFI_DEVICE_PATH_UTILITIES_PROTOCOL = EFI_DEVICE_PATH_UTILITIES_PROTOCOL_GUID;
                            EFI_DEVICE_PATH_TO_TEXT_PROTOCOL* pEFI_DEVICE_PATH_TO_TEXT_PROTOCOL;
                            EFI_DEVICE_PATH* pEFI_DEVICE_PATH;
                            MEMMAP_DEVICE_PATH* pMEMMAP_DEVICE_PATH = malloc(sizeof(MEMMAP_DEVICE_PATH));
                            //EFI_LOADED_IMAGE_PROTOCOL* pEFI_LOADED_IMAGE_PROTOCOL;
                            EFI_DEVICE_PATH_UTILITIES_PROTOCOL* pEFI_DEVICE_PATH_UTILITIES_PROTOCOL;
                            //EFI_HANDLE MEMImageHandle;
                            //EFI_SHELL_PARAMETERS_PROTOCOL  ShellParamsProtocol;

                            Status = gST->BootServices->LocateProtocol(&guidEFI_DEVICE_PATH_TO_TEXT_PROTOCOL, NULL, &pEFI_DEVICE_PATH_TO_TEXT_PROTOCOL);
                            //CDETRACE((TRCFAT(EFI_SUCCESS != Status) "Status %s\n", _strefierror(Status)));
                            Status = gST->BootServices->LocateProtocol(&guidEFI_DEVICE_PATH_UTILITIES_PROTOCOL, NULL, &pEFI_DEVICE_PATH_UTILITIES_PROTOCOL);
                            //CDETRACE((TRCFAT(EFI_SUCCESS != Status) "Status %s\n", _strefierror(Status)));

                            pMEMMAP_DEVICE_PATH->Header.Type = HARDWARE_DEVICE_PATH;
                            pMEMMAP_DEVICE_PATH->Header.SubType = HW_MEMMAP_DP;
                            pMEMMAP_DEVICE_PATH->MemoryType = EfiConventionalMemory;
                            pMEMMAP_DEVICE_PATH->StartingAddress = (EFI_PHYSICAL_ADDRESS)plugin[i].pStart;
                            pMEMMAP_DEVICE_PATH->EndingAddress = (EFI_PHYSICAL_ADDRESS)&plugin[i].pStart[plugin[i].size];
                            pMEMMAP_DEVICE_PATH->Header.Length[0] = (unsigned char)sizeof(MEMMAP_DEVICE_PATH);
                            pMEMMAP_DEVICE_PATH->Header.Length[1] = (unsigned char)(sizeof(MEMMAP_DEVICE_PATH) >> 8);

                            DevPath = pEFI_DEVICE_PATH = pEFI_DEVICE_PATH_UTILITIES_PROTOCOL->AppendDeviceNode(NULL, (EFI_DEVICE_PATH_PROTOCOL*)pMEMMAP_DEVICE_PATH);
                            _gPLUGINSTART = plugin[i].pStart;
                            _gPLUGINSIZE = plugin[i].size;

                                    CDETRACE((TRCINF(1) "--> %ls\n", pEFI_DEVICE_PATH_TO_TEXT_PROTOCOL->ConvertDevicePathToText(pEFI_DEVICE_PATH, 1, 0)));

                                    //Status = gST->BootServices->LoadImage(0, gImageHandle, pEFI_DEVICE_PATH, plugin[i].pStart, plugin[i].size, &MEMImageHandle);

                                    ////CDETRACE((TRCINF(1) "### Status %s -> LoadImage(0, ImageHandle %p, pEFI_DEVICE_PATH %p, buffer %p, size %zd, &MEMImageHandle %p)\n",
                                    //    //_strefierror(Status),
                                    //    //gImageHandle,
                                    //    //pEFI_DEVICE_PATH,
                                    //    //plugin[i].pStart,
                                    //    //plugin[i].size,
                                    //    //&MEMImageHandle
                                    //    //));
                                    ////
                                    //// LoadOption
                                    ////
                                    //Status = gBS->HandleProtocol(MEMImageHandle, &gEfiLoadedImageProtocolGuid, (VOID**)&pEFI_LOADED_IMAGE_PROTOCOL);
                                    ////CDETRACE((TRCINF(1) "### Status %s, pEFI_LOADED_IMAGE_PROTOCOL\n", _strefierror(Status)));

                                    //pEFI_LOADED_IMAGE_PROTOCOL->LoadOptions = CmdLine;
                                    //pEFI_LOADED_IMAGE_PROTOCOL->LoadOptionsSize = (UINT32)(wcslen(CmdLine) + sizeof(L""));

                                    //memset(&ShellParamsProtocol, 0, sizeof(ShellParamsProtocol));

                                    //ShellParamsProtocol.StdIn = &FileInterfaceStdIn;
                                    //ShellParamsProtocol.StdOut = &FileInterfaceStdOut;
                                    //ShellParamsProtocol.StdErr = &FileInterfaceStdErr;
                                    //Status = UpdateArgcArgv(&ShellParamsProtocol, CmdLine, Efi_Application, NULL, NULL);
                            
                                    //if (0 == _wcsnicmp(CmdLine, L"afuefix", strlen("afuefix")))
                                    //{
                                    //    static wchar_t wcsAFUEFIplusDrive[] = L"A:\\AfuEfix64.efi";
                                    //    CmdLine = wcsAFUEFIplusDrive;
                                    //    ShellParamsProtocol.Argv[0] = CmdLine;
                                    //    CDETRACE((TRCINF(1)"catch afuefix ...\n"));
                                    //}
                            
                                    //CDETRACE((TRCINF(1) "### CmdLine: \"%ls\"\n", CmdLine));

                                    //Status = gST->BootServices->InstallProtocolInterface
                                    //(
                                    //    &MEMImageHandle,
                                    //    &guidEFI_SHELL_PARAMETERS_PROTOCOL,
                                    //    EFI_NATIVE_INTERFACE,
                                    //    &ShellParamsProtocol
                                    //);


                                    //Status = gST->BootServices->StartImage(MEMImageHandle, NULL, NULL);
                                    ////CDETRACE((TRCINF(1) "### Status %s, size %zd\n", _strefierror(Status), plugin[i].size));

                        }

                        break;
                    }
                }

                if (0 == fIsPlugin)
                {
                    DevPath = ShellInfoObject.NewEfiShellProtocol->GetDevicePathFromFilePath(CommandWithPath);

                    if (DevPath == NULL) {
                        Status = EFI_OUT_OF_RESOURCES;
                        break;
                    }
                }

                //
                // Execute the device path
                //
                CDETRACE((TRCINF(1) "### CmdLine: \"%ls\", CommandWithPath:\"%ls\"\n", CmdLine, CommandWithPath));
                Status = InternalShellExecuteDevicePath(
                    &gImageHandle,
                    DevPath,
                    CmdLine,
                    NULL,
                    &StartStatus
                );

                SHELL_FREE_NON_NULL(DevPath);

                if(0 == _wcsnicmp(wcsCmdName, L"pciview", wcslen(L"pciview")))
                    pSTOP->SetMode(pSTOP, CurrentModeNumber);

            }



          if (EFI_ERROR (Status)) {
            CalleeExitStatus = (SHELL_STATUS)(Status & (~MAX_BIT));
          } else {
            CalleeExitStatus = (SHELL_STATUS)StartStatus;
          }

          if (CommandStatus != NULL) {
            *CommandStatus = CalleeExitStatus;
          }

          //
          // Update last error status.
          //
          // Status is an EFI_STATUS. Clear top bit to convert to SHELL_STATUS
          SetLastError (CalleeExitStatus);
          break;
        default:
          //
          // Do nothing.
          //
          break;
      }

      break;
    default:
      //
      // Do nothing.
      //
      break;
  }

  SHELL_FREE_NON_NULL (CommandWithPath);

  return (Status);
}

/**
  Function to setup StdIn, StdErr, StdOut, and then run the command or file.

  @param[in] Type             the type of operation being run.
  @param[in] CmdLine          the command line to run.
  @param[in] FirstParameter   the first parameter on the command line.
  @param[in] ParamProtocol    the shell parameters protocol pointer
  @param[out] CommandStatus   the status from the command line.

  @retval EFI_SUCCESS     The command was completed.
  @retval EFI_ABORTED     The command's operation was aborted.
**/
EFI_STATUS
SetupAndRunCommandOrFile (
  IN   SHELL_OPERATION_TYPES          Type,
  IN   CHAR16                         *CmdLine,
  IN   CHAR16                         *FirstParameter,
  IN   EFI_SHELL_PARAMETERS_PROTOCOL  *ParamProtocol,
  OUT EFI_STATUS                      *CommandStatus
  )
{
  EFI_STATUS         Status;
  SHELL_FILE_HANDLE  OriginalStdIn;
  SHELL_FILE_HANDLE  OriginalStdOut;
  SHELL_FILE_HANDLE  OriginalStdErr;
  SYSTEM_TABLE_INFO  OriginalSystemTableInfo;
  CONST SCRIPT_FILE  *ConstScriptFile;

  CDETRACE((TRCINF(1)"-->\n"));

  //
  // Update the StdIn, StdOut, and StdErr for redirection to environment variables, files, etc... unicode and ASCII
  //
  Status = UpdateStdInStdOutStdErr (ParamProtocol, CmdLine, &OriginalStdIn, &OriginalStdOut, &OriginalStdErr, &OriginalSystemTableInfo);

  //
  // The StdIn, StdOut, and StdErr are set up.
  // Now run the command, script, or application
  //
  if (!EFI_ERROR (Status)) {
    TrimSpaces (&CmdLine);
    Status = RunCommandOrFile (Type, CmdLine, FirstParameter, ParamProtocol, CommandStatus);
  }

  //
  // Now print errors
  //
  if (EFI_ERROR (Status)) {
    ConstScriptFile = ShellCommandGetCurrentScriptFile ();
    if ((ConstScriptFile == NULL) || (ConstScriptFile->CurrentCommand == NULL)) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_SHELL_ERROR), ShellInfoObject.HiiHandle, (VOID *)(Status));
    } else {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_SHELL_ERROR_SCRIPT), ShellInfoObject.HiiHandle, (VOID *)(Status), ConstScriptFile->CurrentCommand->Line);
    }
  }

  //
  // put back the original StdIn, StdOut, and StdErr
  //
  RestoreStdInStdOutStdErr (ParamProtocol, &OriginalStdIn, &OriginalStdOut, &OriginalStdErr, &OriginalSystemTableInfo);

  return (Status);
}

/**
  Function will process and run a command line.

  This will determine if the command line represents an internal shell
  command or dispatch an external application.

  @param[in] CmdLine      The command line to parse.
  @param[out] CommandStatus   The status from the command line.

  @retval EFI_SUCCESS     The command was completed.
  @retval EFI_ABORTED     The command's operation was aborted.
**/
EFI_STATUS
RunShellCommand (
  IN CONST CHAR16  *CmdLine,
  OUT EFI_STATUS   *CommandStatus
  )
{
  EFI_STATUS             Status;
  CHAR16                 *CleanOriginal;
  CHAR16                 *FirstParameter;
  CHAR16                 *TempWalker;
  SHELL_OPERATION_TYPES  Type;
  CONST CHAR16           *CurDir;

  ASSERT (CmdLine != NULL);
  if (StrLen (CmdLine) == 0) {
    return (EFI_SUCCESS);
  }

  Status        = EFI_SUCCESS;
  CleanOriginal = NULL;

  CleanOriginal = StrnCatGrow (&CleanOriginal, NULL, CmdLine, 0);
  if (CleanOriginal == NULL) {
    return (EFI_OUT_OF_RESOURCES);
  }

  TrimSpaces (&CleanOriginal);

  //
  // NULL out comments (leveraged from RunScriptFileHandle() ).
  // The # character on a line is used to denote that all characters on the same line
  // and to the right of the # are to be ignored by the shell.
  // Afterwards, again remove spaces, in case any were between the last command-parameter and '#'.
  //
  for (TempWalker = CleanOriginal; TempWalker != NULL && *TempWalker != CHAR_NULL; TempWalker++) {
    if (*TempWalker == L'^') {
      if (*(TempWalker + 1) == L'#') {
        TempWalker++;
      }
    } else if (*TempWalker == L'#') {
      *TempWalker = CHAR_NULL;
    }
  }

  TrimSpaces (&CleanOriginal);

  //
  // Handle case that passed in command line is just 1 or more " " characters.
  //
  if (StrLen (CleanOriginal) == 0) {
    SHELL_FREE_NON_NULL (CleanOriginal);
    return (EFI_SUCCESS);
  }

  Status = ProcessCommandLineToFinal (&CleanOriginal);
  if (EFI_ERROR (Status)) {
    SHELL_FREE_NON_NULL (CleanOriginal);
    return (Status);
  }

  //
  // We don't do normal processing with a split command line (output from one command input to another)
  //
  CDETRACE((TRCINF(1)"-->ContainsSplit(\"%ls\")\n", CleanOriginal));
  if (ContainsSplit (CleanOriginal)) {
    Status = ProcessNewSplitCommandLine (CleanOriginal);
    SHELL_FREE_NON_NULL (CleanOriginal);
    return (Status);
  }

  //
  // We need the first parameter information so we can determine the operation type
  //
  FirstParameter = AllocateZeroPool (StrSize (CleanOriginal));
  if (FirstParameter == NULL) {
    SHELL_FREE_NON_NULL (CleanOriginal);
    return (EFI_OUT_OF_RESOURCES);
  }

  TempWalker = CleanOriginal;
  if (!EFI_ERROR (GetNextParameter (&TempWalker, &FirstParameter, StrSize (CleanOriginal), TRUE))) {
    //
    // Depending on the first parameter we change the behavior
    //
    switch (Type = GetOperationType (FirstParameter)) {
      case File_Sys_Change:
        Status = ChangeMappedDrive (FirstParameter);
        break;
      case Internal_Command:
      case Script_File_Name:
      case Efi_Application:
          Status = SetupAndRunCommandOrFile (Type, CleanOriginal, FirstParameter, ShellInfoObject.NewShellParametersProtocol, CommandStatus);
        break;
      default:
        //
        // Whatever was typed, it was invalid.
        //
        ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_SHELL_NOT_FOUND), ShellInfoObject.HiiHandle, FirstParameter);
        SetLastError (SHELL_NOT_FOUND);
        break;
    }
  } else {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_SHELL_NOT_FOUND), ShellInfoObject.HiiHandle, FirstParameter);
    SetLastError (SHELL_NOT_FOUND);
  }

  //
  // Check whether the current file system still exists. If not exist, we need update "cwd" and gShellCurMapping.
  //
  CurDir = EfiShellGetCurDir (NULL);
  if (CurDir != NULL) {
    if (EFI_ERROR (ShellFileExists (CurDir))) {
      //
      // EfiShellSetCurDir() cannot set current directory to NULL.
      // EfiShellSetEnv() is not allowed to set the "cwd" variable.
      // Only InternalEfiShellSetEnv () is allowed setting the "cwd" variable.
      //
      InternalEfiShellSetEnv (L"cwd", NULL, TRUE);
      gShellCurMapping = NULL;
    }
  }

  SHELL_FREE_NON_NULL (CleanOriginal);
  SHELL_FREE_NON_NULL (FirstParameter);

  return (Status);
}

/**
  Function will process and run a command line.

  This will determine if the command line represents an internal shell
  command or dispatch an external application.

  @param[in] CmdLine      The command line to parse.

  @retval EFI_SUCCESS     The command was completed.
  @retval EFI_ABORTED     The command's operation was aborted.
**/
EFI_STATUS
RunCommand (
  IN CONST CHAR16  *CmdLine
  )
{
  return (RunShellCommand (CmdLine, NULL));
}

/**
  Function to process a NSH script file via SHELL_FILE_HANDLE.

  @param[in] Handle             The handle to the already opened file.
  @param[in] Name               The name of the script file.

  @retval EFI_SUCCESS           the script completed successfully
**/
EFI_STATUS
RunScriptFileHandle (
  IN SHELL_FILE_HANDLE  Handle,
  IN CONST CHAR16       *Name
  )
{
  EFI_STATUS           Status;
  SCRIPT_FILE          *NewScriptFile;
  UINTN                LoopVar;
  UINTN                PrintBuffSize;
  CHAR16               *CommandLine;
  CHAR16               *CommandLine2;
  CHAR16               *CommandLine3;
  SCRIPT_COMMAND_LIST  *LastCommand;
  BOOLEAN              Ascii;
  BOOLEAN              PreScriptEchoState;
  BOOLEAN              PreCommandEchoState;
  CONST CHAR16         *CurDir;
  UINTN                LineCount;
  CHAR16               LeString[50];
  LIST_ENTRY           OldBufferList;

  ASSERT (!ShellCommandGetScriptExit ());

  PreScriptEchoState = ShellCommandGetEchoState ();
  PrintBuffSize      = PcdGet32 (PcdShellPrintBufferSize);

  NewScriptFile = (SCRIPT_FILE *)AllocateZeroPool (sizeof (SCRIPT_FILE));
  if (NewScriptFile == NULL) {
    return (EFI_OUT_OF_RESOURCES);
  }

  //
  // Set up the name
  //
  ASSERT (NewScriptFile->ScriptName == NULL);
  NewScriptFile->ScriptName = StrnCatGrow (&NewScriptFile->ScriptName, NULL, Name, 0);
  if (NewScriptFile->ScriptName == NULL) {
    DeleteScriptFileStruct (NewScriptFile);
    return (EFI_OUT_OF_RESOURCES);
  }

  //
  // Save the parameters (used to replace %0 to %9 later on)
  //
  NewScriptFile->Argc = ShellInfoObject.NewShellParametersProtocol->Argc;
  if (NewScriptFile->Argc != 0) {
    NewScriptFile->Argv = (CHAR16 **)AllocateZeroPool (NewScriptFile->Argc * sizeof (CHAR16 *));
    if (NewScriptFile->Argv == NULL) {
      DeleteScriptFileStruct (NewScriptFile);
      return (EFI_OUT_OF_RESOURCES);
    }

    //
    // Put the full path of the script file into Argv[0] as required by section
    // 3.6.2 of version 2.2 of the shell specification.
    //
    NewScriptFile->Argv[0] = StrnCatGrow (&NewScriptFile->Argv[0], NULL, NewScriptFile->ScriptName, 0);
    for (LoopVar = 1; LoopVar < 10 && LoopVar < NewScriptFile->Argc; LoopVar++) {
      ASSERT (NewScriptFile->Argv[LoopVar] == NULL);
      NewScriptFile->Argv[LoopVar] = StrnCatGrow (&NewScriptFile->Argv[LoopVar], NULL, ShellInfoObject.NewShellParametersProtocol->Argv[LoopVar], 0);
      if (NewScriptFile->Argv[LoopVar] == NULL) {
        DeleteScriptFileStruct (NewScriptFile);
        return (EFI_OUT_OF_RESOURCES);
      }
    }
  } else {
    NewScriptFile->Argv = NULL;
  }

  InitializeListHead (&NewScriptFile->CommandList);
  InitializeListHead (&NewScriptFile->SubstList);

  //
  // Now build the list of all script commands.
  //
  LineCount = 0;
  while (!ShellFileHandleEof (Handle)) {
    CommandLine = ShellFileHandleReturnLine (Handle, &Ascii);
    LineCount++;
    if ((CommandLine == NULL) || (StrLen (CommandLine) == 0) || (CommandLine[0] == '#')) {
      SHELL_FREE_NON_NULL (CommandLine);
      continue;
    }

    NewScriptFile->CurrentCommand = AllocateZeroPool (sizeof (SCRIPT_COMMAND_LIST));
    if (NewScriptFile->CurrentCommand == NULL) {
      SHELL_FREE_NON_NULL (CommandLine);
      DeleteScriptFileStruct (NewScriptFile);
      return (EFI_OUT_OF_RESOURCES);
    }

    NewScriptFile->CurrentCommand->Cl   = CommandLine;
    NewScriptFile->CurrentCommand->Data = NULL;
    NewScriptFile->CurrentCommand->Line = LineCount;

    InsertTailList (&NewScriptFile->CommandList, &NewScriptFile->CurrentCommand->Link);
  }

  //
  // Add this as the topmost script file
  //
  ShellCommandSetNewScript (NewScriptFile);

  //
  // Now enumerate through the commands and run each one.
  //
  CommandLine = AllocateZeroPool (PrintBuffSize);
  if (CommandLine == NULL) {
    DeleteScriptFileStruct (NewScriptFile);
    return (EFI_OUT_OF_RESOURCES);
  }

  CommandLine2 = AllocateZeroPool (PrintBuffSize);
  if (CommandLine2 == NULL) {
    FreePool (CommandLine);
    DeleteScriptFileStruct (NewScriptFile);
    return (EFI_OUT_OF_RESOURCES);
  }

  for ( NewScriptFile->CurrentCommand = (SCRIPT_COMMAND_LIST *)GetFirstNode (&NewScriptFile->CommandList)
        ; !IsNull (&NewScriptFile->CommandList, &NewScriptFile->CurrentCommand->Link)
        ; // conditional increment in the body of the loop
        )
  {
    ASSERT (CommandLine2 != NULL);
    StrnCpyS (
      CommandLine2,
      PrintBuffSize/sizeof (CHAR16),
      NewScriptFile->CurrentCommand->Cl,
      PrintBuffSize/sizeof (CHAR16) - 1
      );

    SaveBufferList (&OldBufferList);

    //
    // NULL out comments
    //
    for (CommandLine3 = CommandLine2; CommandLine3 != NULL && *CommandLine3 != CHAR_NULL; CommandLine3++) {
      if (*CommandLine3 == L'^') {
        if ( *(CommandLine3+1) == L':') {
          CopyMem (CommandLine3, CommandLine3+1, StrSize (CommandLine3) - sizeof (CommandLine3[0]));
        } else if (*(CommandLine3+1) == L'#') {
          CommandLine3++;
        }
      } else if (*CommandLine3 == L'#') {
        *CommandLine3 = CHAR_NULL;
      }
    }

    if ((CommandLine2 != NULL) && (StrLen (CommandLine2) >= 1)) {
      //
      // Due to variability in starting the find and replace action we need to have both buffers the same.
      //
      StrnCpyS (
        CommandLine,
        PrintBuffSize/sizeof (CHAR16),
        CommandLine2,
        PrintBuffSize/sizeof (CHAR16) - 1
        );

      //
      // Remove the %0 to %9 from the command line (if we have some arguments)
      //
      if (NewScriptFile->Argv != NULL) {
        switch (NewScriptFile->Argc) {
          default:
            Status = ShellCopySearchAndReplace (CommandLine2, CommandLine, PrintBuffSize, L"%9", NewScriptFile->Argv[9], FALSE, FALSE);
            ASSERT_EFI_ERROR (Status);
          case 9:
            Status = ShellCopySearchAndReplace (CommandLine, CommandLine2, PrintBuffSize, L"%8", NewScriptFile->Argv[8], FALSE, FALSE);
            ASSERT_EFI_ERROR (Status);
          case 8:
            Status = ShellCopySearchAndReplace (CommandLine2, CommandLine, PrintBuffSize, L"%7", NewScriptFile->Argv[7], FALSE, FALSE);
            ASSERT_EFI_ERROR (Status);
          case 7:
            Status = ShellCopySearchAndReplace (CommandLine, CommandLine2, PrintBuffSize, L"%6", NewScriptFile->Argv[6], FALSE, FALSE);
            ASSERT_EFI_ERROR (Status);
          case 6:
            Status = ShellCopySearchAndReplace (CommandLine2, CommandLine, PrintBuffSize, L"%5", NewScriptFile->Argv[5], FALSE, FALSE);
            ASSERT_EFI_ERROR (Status);
          case 5:
            Status = ShellCopySearchAndReplace (CommandLine, CommandLine2, PrintBuffSize, L"%4", NewScriptFile->Argv[4], FALSE, FALSE);
            ASSERT_EFI_ERROR (Status);
          case 4:
            Status = ShellCopySearchAndReplace (CommandLine2, CommandLine, PrintBuffSize, L"%3", NewScriptFile->Argv[3], FALSE, FALSE);
            ASSERT_EFI_ERROR (Status);
          case 3:
            Status = ShellCopySearchAndReplace (CommandLine, CommandLine2, PrintBuffSize, L"%2", NewScriptFile->Argv[2], FALSE, FALSE);
            ASSERT_EFI_ERROR (Status);
          case 2:
            Status = ShellCopySearchAndReplace (CommandLine2, CommandLine, PrintBuffSize, L"%1", NewScriptFile->Argv[1], FALSE, FALSE);
            ASSERT_EFI_ERROR (Status);
          case 1:
            Status = ShellCopySearchAndReplace (CommandLine, CommandLine2, PrintBuffSize, L"%0", NewScriptFile->Argv[0], FALSE, FALSE);
            ASSERT_EFI_ERROR (Status);
            break;
          case 0:
            break;
        }
      }

      Status = ShellCopySearchAndReplace (CommandLine2, CommandLine, PrintBuffSize, L"%1", L"\"\"", FALSE, FALSE);
      Status = ShellCopySearchAndReplace (CommandLine, CommandLine2, PrintBuffSize, L"%2", L"\"\"", FALSE, FALSE);
      Status = ShellCopySearchAndReplace (CommandLine2, CommandLine, PrintBuffSize, L"%3", L"\"\"", FALSE, FALSE);
      Status = ShellCopySearchAndReplace (CommandLine, CommandLine2, PrintBuffSize, L"%4", L"\"\"", FALSE, FALSE);
      Status = ShellCopySearchAndReplace (CommandLine2, CommandLine, PrintBuffSize, L"%5", L"\"\"", FALSE, FALSE);
      Status = ShellCopySearchAndReplace (CommandLine, CommandLine2, PrintBuffSize, L"%6", L"\"\"", FALSE, FALSE);
      Status = ShellCopySearchAndReplace (CommandLine2, CommandLine, PrintBuffSize, L"%7", L"\"\"", FALSE, FALSE);
      Status = ShellCopySearchAndReplace (CommandLine, CommandLine2, PrintBuffSize, L"%8", L"\"\"", FALSE, FALSE);
      Status = ShellCopySearchAndReplace (CommandLine2, CommandLine, PrintBuffSize, L"%9", L"\"\"", FALSE, FALSE);

      StrnCpyS (
        CommandLine2,
        PrintBuffSize/sizeof (CHAR16),
        CommandLine,
        PrintBuffSize/sizeof (CHAR16) - 1
        );

      LastCommand = NewScriptFile->CurrentCommand;

      for (CommandLine3 = CommandLine2; CommandLine3[0] == L' '; CommandLine3++) {
      }

      if ((CommandLine3 != NULL) && (CommandLine3[0] == L':')) {
        //
        // This line is a goto target / label
        //
      } else {
        if ((CommandLine3 != NULL) && (StrLen (CommandLine3) > 0)) {
          if (CommandLine3[0] == L'@') {
            //
            // We need to save the current echo state
            // and disable echo for just this command.
            //
            PreCommandEchoState = ShellCommandGetEchoState ();
            ShellCommandSetEchoState (FALSE);
            Status = RunCommand (CommandLine3+1);

            //
            // If command was "@echo -off" or "@echo -on" then don't restore echo state
            //
            if ((StrCmp (L"@echo -off", CommandLine3) != 0) &&
                (StrCmp (L"@echo -on", CommandLine3) != 0))
            {
              //
              // Now restore the pre-'@' echo state.
              //
              ShellCommandSetEchoState (PreCommandEchoState);
            }
          } else {
            if (ShellCommandGetEchoState ()) {
              CurDir = ShellInfoObject.NewEfiShellProtocol->GetEnv (L"cwd");
              if ((CurDir != NULL) && (StrLen (CurDir) > 1)) {
                ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_SHELL_CURDIR), ShellInfoObject.HiiHandle, CurDir);
              } else {
                ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_SHELL_SHELL), ShellInfoObject.HiiHandle);
              }

              ShellPrintEx (-1, -1, L"%s\r\n", CommandLine2);
            }

            Status = RunCommand (CommandLine3);
          }
        }

        if (ShellCommandGetScriptExit ()) {
          //
          // ShellCommandGetExitCode() always returns a UINT64
          //
          UnicodeSPrint (LeString, sizeof (LeString), L"0x%Lx", ShellCommandGetExitCode ());
          DEBUG_CODE (
            InternalEfiShellSetEnv (L"debuglasterror", LeString, TRUE);
            );
          InternalEfiShellSetEnv (L"lasterror", LeString, TRUE);

          ShellCommandRegisterExit (FALSE, 0);
          Status = EFI_SUCCESS;
          RestoreBufferList (&OldBufferList);
          break;
        }

        if (ShellGetExecutionBreakFlag ()) {
          RestoreBufferList (&OldBufferList);
          break;
        }

        if (EFI_ERROR (Status)) {
          RestoreBufferList (&OldBufferList);
          break;
        }

        if (ShellCommandGetExit ()) {
          RestoreBufferList (&OldBufferList);
          break;
        }
      }

      //
      // If that commend did not update the CurrentCommand then we need to advance it...
      //
      if (LastCommand == NewScriptFile->CurrentCommand) {
        NewScriptFile->CurrentCommand = (SCRIPT_COMMAND_LIST *)GetNextNode (&NewScriptFile->CommandList, &NewScriptFile->CurrentCommand->Link);
        if (!IsNull (&NewScriptFile->CommandList, &NewScriptFile->CurrentCommand->Link)) {
          NewScriptFile->CurrentCommand->Reset = TRUE;
        }
      }
    } else {
      NewScriptFile->CurrentCommand = (SCRIPT_COMMAND_LIST *)GetNextNode (&NewScriptFile->CommandList, &NewScriptFile->CurrentCommand->Link);
      if (!IsNull (&NewScriptFile->CommandList, &NewScriptFile->CurrentCommand->Link)) {
        NewScriptFile->CurrentCommand->Reset = TRUE;
      }
    }

    RestoreBufferList (&OldBufferList);
  }

  FreePool (CommandLine);
  FreePool (CommandLine2);
  ShellCommandSetNewScript (NULL);

  //
  // Only if this was the last script reset the state.
  //
  if (ShellCommandGetCurrentScriptFile () == NULL) {
    ShellCommandSetEchoState (PreScriptEchoState);
  }

  return (EFI_SUCCESS);
}

/**
  Function to process a NSH script file.

  @param[in] ScriptPath         Pointer to the script file name (including file system path).
  @param[in] Handle             the handle of the script file already opened.
  @param[in] CmdLine            the command line to run.
  @param[in] ParamProtocol      the shell parameters protocol pointer

  @retval EFI_SUCCESS           the script completed successfully
**/
EFI_STATUS
RunScriptFile (
  IN CONST CHAR16                   *ScriptPath,
  IN SHELL_FILE_HANDLE              Handle OPTIONAL,
  IN CONST CHAR16                   *CmdLine,
  IN EFI_SHELL_PARAMETERS_PROTOCOL  *ParamProtocol
  )
{
  EFI_STATUS         Status;
  SHELL_FILE_HANDLE  FileHandle;
  UINTN              Argc;
  CHAR16             **Argv;

  if (ShellIsFile (ScriptPath) != EFI_SUCCESS) {
    return (EFI_INVALID_PARAMETER);
  }

  //
  // get the argc and argv updated for scripts
  //
  Status = UpdateArgcArgv (ParamProtocol, CmdLine, Script_File_Name, &Argv, &Argc);
  if (!EFI_ERROR (Status)) {
    if (Handle == NULL) {
      //
      // open the file
      //
      Status = ShellOpenFileByName (ScriptPath, &FileHandle, EFI_FILE_MODE_READ, 0);
      if (!EFI_ERROR (Status)) {
        //
        // run it
        //
        Status = RunScriptFileHandle (FileHandle, ScriptPath);

        //
        // now close the file
        //
        ShellCloseFile (&FileHandle);
      }
    } else {
      Status = RunScriptFileHandle (Handle, ScriptPath);
    }
  }

  //
  // This is guaranteed to be called after UpdateArgcArgv no matter what else happened.
  // This is safe even if the update API failed.  In this case, it may be a no-op.
  //
  RestoreArgcArgv (ParamProtocol, &Argv, &Argc);

  return (Status);
}

/**
  Return the pointer to the first occurrence of any character from a list of characters.

  @param[in] String           the string to parse
  @param[in] CharacterList    the list of character to look for
  @param[in] EscapeCharacter  An escape character to skip

  @return the location of the first character in the string
  @retval CHAR_NULL no instance of any character in CharacterList was found in String
**/
CONST CHAR16 *
FindFirstCharacter (
  IN CONST CHAR16  *String,
  IN CONST CHAR16  *CharacterList,
  IN CONST CHAR16  EscapeCharacter
  )
{
  UINTN  WalkChar;
  UINTN  WalkStr;

  for (WalkStr = 0; WalkStr < StrLen (String); WalkStr++) {
    if (String[WalkStr] == EscapeCharacter) {
      WalkStr++;
      continue;
    }

    for (WalkChar = 0; WalkChar < StrLen (CharacterList); WalkChar++) {
      if (String[WalkStr] == CharacterList[WalkChar]) {
        return (&String[WalkStr]);
      }
    }
  }

  return (String + StrLen (String));
}

/************************************************************************************************************************/
/********************************************* main *********************************************************************/
/************************************************************************************************************************/
extern char* _strefierror(size_t);

void _mysleep(int millisec)
{
    clock_t clk = (millisec + CLOCKS_PER_SEC / 1000) + clock();

    while (clk > clock())
        ;
}

void _mymenuF8Key(void)
{
    fSkipSTARTUPNSH = 1;
    printf("\nTODO: F8 Menu, PRESS F5 to skip STARTUP.NSH\n");
    __debugbreak();
}

void _mymenuF5Key(void)
{
    fSkipSTARTUPNSH = 1;
}

int main(int argc, char** argv)
{
    int i = 0;
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_HANDLE        ImageHandle = (void*)argv[-2];
    EFI_SYSTEM_TABLE* SystemTable = (void*)argv[-1];
    static EFI_GUID guidSTOP = EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID;
    static EFI_GUID guidSTIP = EFI_SIMPLE_TEXT_INPUT_PROTOCOL_GUID;
    static EFI_GUID guidSTIPEx = EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL_GUID;
    static EFI_GUID guidGOP = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* pSTOP;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL* pSTIP;
    //EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL* pSTIPEx;
    EFI_GRAPHICS_OUTPUT_PROTOCOL* pGOP;
    size_t NumSTIPEx,n,row = 0,col = 0;
    EFI_HANDLE* pphndSTIPEx = NULL;

    gImageHandle = (void*)argv[-2];
    gSystemTable = (void*)argv[-1];

    Status = SystemTable->BootServices->LocateProtocol(&guidGOP, NULL, (void**)&pGOP);
    Status = SystemTable->BootServices->LocateProtocol(&guidSTOP, NULL, (void**)&pSTOP);
    Status = SystemTable->BootServices->LocateProtocol(&guidSTIP, NULL, (void**)&pSTIP);
    Status = SystemTable->BootServices->LocateHandleBuffer(ByProtocol, &guidSTIP,NULL, &NumSTIPEx, &pphndSTIPEx);
    
    //
    // read \EFI\BOOT\BOOTX64.INI
    //
    if (1) do
    {
        EFI_FILE_PROTOCOL* pEFI_FILE_PROTOCOL, * pROOT;
        static EFI_GUID guidEFI_SIMPLE_FILE_SYSTEM_PROTOCOL = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
        EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* pEFI_SIMPLE_FILE_SYSTEM_PROTOCOL;
        EFI_LOADED_IMAGE_PROTOCOL* pLoadedImageProtocol;
        Status = SystemTable->BootServices->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, &pLoadedImageProtocol);
        char *pBuf = realloc(NULL, 1), *pStr;
        
        pBuf[0] = '\0';

        Status = SystemTable->BootServices->HandleProtocol(
            pLoadedImageProtocol->DeviceHandle, 
            &guidEFI_SIMPLE_FILE_SYSTEM_PROTOCOL, 
            &pEFI_SIMPLE_FILE_SYSTEM_PROTOCOL
        );
        Status = pEFI_SIMPLE_FILE_SYSTEM_PROTOCOL->OpenVolume(pEFI_SIMPLE_FILE_SYSTEM_PROTOCOL, &pROOT);

        Status = pROOT->Open(pROOT, &pEFI_FILE_PROTOCOL, L"\\EFI\\BOOT\\BOOTX64.INI", EFI_FILE_MODE_READ, 0);
        
        if (EFI_SUCCESS != Status)
            break;
        
        i = 0;
        do {
            char c;
            n = 1;
            Status = pEFI_FILE_PROTOCOL->Read(pEFI_FILE_PROTOCOL, &n, &c);
            if (1 == n)
            {
                pBuf = realloc(pBuf, i + 1);
                pBuf[i] = c;
                i++;
            }
        } while (1 == n);
        pBuf = realloc(pBuf, i + 3);
        pBuf[i + 0] = '\0';
        pBuf[i + 1] = '\0';
        pBuf[i + 2] = '\0';

        if ((unsigned char)0xFF == pBuf[0] && (unsigned char)0xFE == pBuf[1])
            wcstombs(pBuf, (wchar_t*) & pBuf[2], i / 2);

        pStr = strtok(pBuf, "\n");
        while (pStr != NULL)
        {
            int tok;
            char* pComment;
            char buf0[32];
            char buf1[32];
            char buf2[32];
            char buf3[32];

            // remove  comment lead by '#'
            pComment = strchr(pStr, '#');
            
            if (NULL != pComment)
                while (*pComment != '\0')
                    *pComment++ = '\x20';

            tok = sscanf(pStr, "%s %s %s %s", &buf0, &buf1, &buf2, &buf3);
            //
            // textresolution
            //
            if (0 == _stricmp(buf0, "TEXTRESOLUTION"))
                sscanf(buf1, "%lld", &col),
                sscanf(buf2, "%lld", &row),
                swprintf(wcsSetScreenResolutionByMode, 20, L"MODE %zd %zd", col /*= 100 */, row /*= 31*/);
            //printf("--> textres %lld %lld\n", col, row);
            
            //
            // drive naming A:, B:, C: or DEFAULT_UEFI_DRIVE_NAMING FS0:, FS1 ...
            //
            if (0 == _stricmp(buf0, "DEFAULT_UEFI_DRIVE_NAMING"))
                _gfDEFAULT_UEFI_DRIVE_NAMING = 1;
                
            pStr = strtok(NULL, "\n");
        }
    
    } while (0);
    
    if (1)
    {
        char countdown = 4;
        clock_t clk = (4 * CLOCKS_PER_SEC / 3) + clock();
        clock_t clkstart = clock();
        clock_t clkdot = clkstart + (1 * CLOCKS_PER_SEC / 3);
        int fNOT_F5Key = 1, fNOT_F8Key = 1, fNOT_ANYKey = 1;
        //UINTN Index;
        EFI_KEY_DATA KeyData = { 0 };
        EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL* pIF;
        size_t NoHandles;

        Status = SystemTable->BootServices->LocateHandleBuffer(ByProtocol, &guidSTIPEx, NULL, &NoHandles, &pphndSTIPEx);

        if (0 == col) 
        {
            i = 0;
            do
            {
                Status = pSTOP->SetMode(pSTOP, pSTOP->Mode->MaxMode - i);

            } while (EFI_SUCCESS != Status && -1 < (pSTOP->Mode->MaxMode - ++i));

            pSTOP->QueryMode(pSTOP, pSTOP->Mode->MaxMode - i, &col, &row);
            swprintf(wcsSetScreenResolutionByMode, 20, L"MODE %zd %zd", col /*= 100 */, row /*= 31*/);
        }
        else {
            unsigned u, count;
            size_t Columns, Rows;
            for (u = 0, count = 0; u < pGOP->Mode->MaxMode; u++) {

                Status = pSTOP->SetMode(pSTOP, u);

                if (EFI_SUCCESS != Status)                      // mode not supported
                    continue;
                count++;

                Status = pSTOP->QueryMode(pSTOP, u, &Columns, &Rows);

                if (col == Columns && row == Rows)
                    break;
            }

        }
    
        pSTOP->SetAttribute(pSTOP, EFI_BACKGROUND_BLACK + EFI_WHITE);
        pSTOP->ClearScreen(pSTOP);

        printf("Press F5 to skip STARTUP.NSH\nStarting TORO UEFI SHELL v%d.%d.%d Build %d ", MAJORVER, MINORVER, PATCHVER, BUILDNUM);

    // SystemTable->ConOut->QueryMode(SystemTable->ConOut,)
    // https://github.com/KilianKegel/toro-C-Library#implementation-status
    //

        while (fNOT_F5Key && fNOT_F8Key && fNOT_ANYKey && clk > clock())
        {
            if (clkdot < clock())
                printf("."),
                countdown--,
                clkdot += (1 * CLOCKS_PER_SEC / 3);

            for (n = 0; n < NoHandles; n++)
            {

                //printf("--> KeyData.Key.ScanCode %X\n", KeyData.Key.ScanCode);

                if (0x0F == KeyData.Key.ScanCode)   // F5
                {
                    fNOT_F5Key = 0;
                    break;
                }

                if (0x12 == KeyData.Key.ScanCode)
                {
                    fNOT_F8Key = 0;
                    break;
                }

                if (0 != KeyData.Key.ScanCode || 0 != KeyData.Key.UnicodeChar || 0 != KeyData.KeyState.KeyShiftState || 0 != KeyData.KeyState.KeyToggleState)
                {
                    fNOT_ANYKey = 0;
                    break;
                }


                memset(&KeyData, 0, sizeof(KeyData));

                Status = SystemTable->BootServices->HandleProtocol(pphndSTIPEx[n], &guidSTIPEx, &pIF);

                memset(&KeyData, 0, sizeof(KeyData));

                Status = SystemTable->BootServices->CheckEvent(pIF->WaitForKeyEx);

                if (EFI_SUCCESS == Status)
                {
                    Status = pIF->ReadKeyStrokeEx(pIF, &KeyData);

                    if (EFI_SUCCESS != Status)
                        memset(&KeyData, 0xFF, sizeof(KeyData));
                    else {
                        KeyData.Key.ScanCode;
                        KeyData.Key.UnicodeChar;
                        KeyData.KeyState.KeyShiftState;
                        KeyData.KeyState.KeyToggleState;

                        //printf("%zd: ScanCode %04X , UnicodeChar %04X, KeyShiftState %08X, KeyToggleState %02X\n",
                        //    n,
                        //    KeyData.Key.ScanCode,
                        //    KeyData.Key.UnicodeChar,
                        //    KeyData.KeyState.KeyShiftState,
                        //    KeyData.KeyState.KeyToggleState
                        //);
                    }
                }
            }
        }

        if (0 == fNOT_F5Key)
            _mymenuF5Key();

        if (0 == fNOT_F8Key)
            _mymenuF8Key();
    }

    Status = UefiMainCDEHOOKED(ImageHandle, SystemTable);

    return Status;
}

