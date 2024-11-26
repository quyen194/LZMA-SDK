// Main.cpp

#include "StdAfx.h"

#include "../../../Common/MyWindows.h"

#if defined(__MINGW32__) || defined(__MINGW64__)
#include <shlwapi.h>
#else
#include <Shlwapi.h>
#endif

#include "../../../../C/DllSecur.h"

#include "../../../Common/MyInitGuid.h"

#include "../../../Common/CommandLineParser.h"
#include "../../../Common/StringConvert.h"
#include "../../../Common/TextConfig.h"

#include "../../../Windows/DLL.h"
#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/NtCheck.h"
#include "../../../Windows/ResourceString.h"

#include "../../ICoder.h"
#include "../../IPassword.h"
#include "../../Archive/IArchive.h"
#include "../../UI/Common/Extract.h"
#include "../../UI/Common/ExitCode.h"
#include "../../UI/Explorer/MyMessages.h"
#include "../../UI/FileManager/MyWindowsNew.h"
#include "../../UI/GUI/ExtractGUI.h"
#include "../../UI/GUI/ExtractRes.h"

#include "7zip/Bundles/SFXWin/Configs.h"

using namespace NWindows;
using namespace NFile;
using namespace NDir;

extern
HINSTANCE g_hInstance;
HINSTANCE g_hInstance;
extern
bool g_DisableUserQuestions;
bool g_DisableUserQuestions;

#define MY_SHELL_EXECUTE

static bool ReadDataString(CFSTR fileName, LPCSTR startID,
    LPCSTR endID, AString &stringResult)
{
  stringResult.Empty();
  NIO::CInFile inFile;
  if (!inFile.Open(fileName))
    return false;
  const size_t kBufferSize = (1 << 12);

  Byte buffer[kBufferSize];
  const unsigned signatureStartSize = MyStringLen(startID);
  const unsigned signatureEndSize = MyStringLen(endID);
  
  size_t numBytesPrev = 0;
  bool writeMode = false;
  UInt64 posTotal = 0;
  for (;;)
  {
    if (posTotal > (1 << 20))
      return (stringResult.IsEmpty());
    const size_t numReadBytes = kBufferSize - numBytesPrev;
    size_t processedSize;
    if (!inFile.ReadFull(buffer + numBytesPrev, numReadBytes, processedSize))
      return false;
    if (processedSize == 0)
      return true;
    const size_t numBytesInBuffer = numBytesPrev + processedSize;
    UInt32 pos = 0;
    for (;;)
    {
      if (writeMode)
      {
        if (pos + signatureEndSize > numBytesInBuffer)
          break;
        if (memcmp(buffer + pos, endID, signatureEndSize) == 0)
          return true;
        const Byte b = buffer[pos];
        if (b == 0)
          return false;
        stringResult += (char)b;
        pos++;
      }
      else
      {
        if (pos + signatureStartSize > numBytesInBuffer)
          break;
        if (memcmp(buffer + pos, startID, signatureStartSize) == 0)
        {
          writeMode = true;
          pos += signatureStartSize;
        }
        else
          pos++;
      }
    }
    numBytesPrev = numBytesInBuffer - pos;
    posTotal += pos;
    memmove(buffer, buffer + pos, numBytesPrev);
  }
}

static char kStartID[] = { ',','!','@','I','n','s','t','a','l','l','@','!','U','T','F','-','8','!', 0 };
static char kEndID[]   = { ',','!','@','I','n','s','t','a','l','l','E','n','d','@','!', 0 };

static struct CInstallIDInit
{
  CInstallIDInit()
  {
    kStartID[0] = ';';
    kEndID[0] = ';';
  }
} g_CInstallIDInit;

#ifndef UNDER_CE

#if !defined(Z7_WIN32_WINNT_MIN) || Z7_WIN32_WINNT_MIN < 0x0500 // win2000
#define Z7_USE_DYN_ComCtl32Version
#endif

#ifdef Z7_USE_DYN_ComCtl32Version
Z7_DIAGNOSTIC_IGNORE_CAST_FUNCTION

static DWORD GetDllVersion(LPCTSTR dllName)
{
  DWORD dwVersion = 0;
  const HINSTANCE hinstDll = LoadLibrary(dllName);
  if (hinstDll)
  {
    const
    DLLGETVERSIONPROC func_DllGetVersion = Z7_GET_PROC_ADDRESS(
    DLLGETVERSIONPROC, hinstDll, "DllGetVersion");
    if (func_DllGetVersion)
    {
      DLLVERSIONINFO dvi;
      ZeroMemory(&dvi, sizeof(dvi));
      dvi.cbSize = sizeof(dvi);
      const HRESULT hr = func_DllGetVersion(&dvi);
      if (SUCCEEDED(hr))
        dwVersion = (DWORD)MAKELONG(dvi.dwMinorVersion, dvi.dwMajorVersion);
    }
    FreeLibrary(hinstDll);
  }
  return dwVersion;
}

#endif
#endif

extern
bool g_LVN_ITEMACTIVATE_Support;
bool g_LVN_ITEMACTIVATE_Support = true;

static const wchar_t * const kUnknownExceptionMessage = L"ERROR: Unknown Error!";

static void ErrorMessageForHRESULT(HRESULT res)
{
  ShowErrorMessage(HResultToMessage(res));
}

static int APIENTRY WinMain2()
{
  // OleInitialize is required for ProgressBar in TaskBar.
#ifndef UNDER_CE
  OleInitialize(NULL);
#endif

#ifndef UNDER_CE
#ifdef Z7_USE_DYN_ComCtl32Version
  {
    const DWORD g_ComCtl32Version = ::GetDllVersion(TEXT("comctl32.dll"));
    g_LVN_ITEMACTIVATE_Support = (g_ComCtl32Version >= MAKELONG(71, 4));
  }
#endif
#endif
  
  UString password;
  bool assumeYes = false;
  bool outputFolderDefined = false;
  FString outputFolder;
  UStringVector commandStrings;
  UString switches;
  #ifdef MY_SHELL_EXECUTE
  UString executeFile, executeParameters;
  #endif
  NCommandLineParser::SplitCommandLine(GetCommandLineW(), commandStrings);

  #ifndef UNDER_CE
  if (commandStrings.Size() > 0)
    commandStrings.Delete(0);
  #endif

  FOR_VECTOR (i, commandStrings)
  {
    const UString &s = commandStrings[i];
    if (s.Len() > 1 && s[0] == '-')
    {
      const wchar_t c = MyCharLower_Ascii(s[1]);
      if (c == 'y')
      {
        assumeYes = true;
        if (s.Len() != 2)
        {
          ShowErrorMessage(L"Bad command");
          return 1;
        }
      }
      else if (c == 'o')
      {
        outputFolder = us2fs(s.Ptr(2));
        NName::NormalizeDirPathPrefix(outputFolder);
        outputFolderDefined = !outputFolder.IsEmpty();
      }
      else if (c == 'p')
      {
        password = s.Ptr(2);
      }
      else
      {
        continue;
      }

      switches.Add_Space_if_NotEmpty();
      switches += s;
    }
  }

  g_DisableUserQuestions = assumeYes;

  FString path;
  NDLL::MyGetModuleFileName(path);

  FString fullPath;
  if (!MyGetFullPathName(path, fullPath))
  {
    ShowErrorMessage(L"Error 1329484");
    return 1;
  }

  AString config;
  if (!ReadDataString(fullPath, kStartID, kEndID, config))
  {
    if (!assumeYes)
      ShowErrorMessage(L"Can't load config info");
    return 1;
  }

  UString dirPrefix ("." STRING_PATH_SEPARATOR);
  UString appLaunched;
  if (!config.IsEmpty())
  {
    CObjectVector<CTextConfigPair> pairs;
    if (!GetTextConfig(config, pairs))
    {
      if (!assumeYes)
        ShowErrorMessage(L"Config failed");
      return 1;
    }

    g_Configs.szTitle = GetTextConfigValue(pairs, "Title");
    const UString installPrompt = GetTextConfigValue(pairs, "BeginPrompt");
    g_Configs.szErrorTitle = GetTextConfigValue(pairs, "ErrorTitle");
    g_Configs.szExtractPathTitle = GetTextConfigValue(pairs, "ExtractPathTitle");
    g_Configs.szExtractPathLabel = GetTextConfigValue(pairs, "ExtractPathLabel");
    const int index = FindTextConfigItem(pairs, "Directory");
    if (index >= 0)
      dirPrefix = pairs[index].String;
    if (!installPrompt.IsEmpty() && !assumeYes)
    {
      if (MessageBoxW(NULL, installPrompt, g_Configs.szTitle, MB_YESNO |
          MB_ICONQUESTION) != IDYES)
        return 0;
    }
    appLaunched = GetTextConfigValue(pairs, "RunProgram");

    #ifdef MY_SHELL_EXECUTE
    executeFile = GetTextConfigValue(pairs, "ExecuteFile");
    executeParameters = GetTextConfigValue(pairs, "ExecuteParameters");
    #endif

    if (g_Configs.szErrorTitle.IsEmpty())
    {
        g_Configs.szErrorTitle = g_Configs.szTitle;
    }
    if (g_Configs.szExtractPathTitle.IsEmpty())
    {
        g_Configs.szExtractPathTitle = g_Configs.szTitle;
    }
  }

  CCodecs *codecs = new CCodecs;
  CMyComPtr<IUnknown> compressCodecsInfo = codecs;
  HRESULT result = codecs->Load();
  if (result != S_OK)
  {
    ErrorMessageForHRESULT(result);
    return 1;
  }

  // COpenCallbackGUI openCallback;

  // openCallback.PasswordIsDefined = !password.IsEmpty();
  // openCallback.Password = password;

  CExtractCallbackImp *ecs = new CExtractCallbackImp;
  CMyComPtr<IFolderArchiveExtractCallback> extractCallback = ecs;
  ecs->Init();

  #ifndef Z7_NO_CRYPTO
  ecs->PasswordIsDefined = !password.IsEmpty();
  ecs->Password = password;
  #endif

  CExtractOptions eo;

  if (dirPrefix.IsEmpty())
  {
    if( !GetOnlyDirPrefix( path, dirPrefix ) )
    {
      ShowErrorMessage(L"Error 1329485");
      return 1;
    }
  }

  eo.OutputDir = outputFolderDefined ? outputFolder : dirPrefix;
  eo.YesToAll = assumeYes;
  eo.OverwriteMode = assumeYes ?
      NExtract::NOverwriteMode::kOverwrite :
      NExtract::NOverwriteMode::kAsk;
  eo.PathMode = NExtract::NPathMode::kFullPaths;
  eo.TestMode = false;
  
  UStringVector v1, v2;
  v1.Add(fs2us(fullPath));
  v2.Add(fs2us(fullPath));
  NWildcard::CCensorNode wildcardCensor;
  wildcardCensor.Add_Wildcard();

  bool messageWasDisplayed = false;
  result = ExtractGUI(codecs,
      CObjectVector<COpenType>(), CIntVector(),
      v1, v2,
      wildcardCensor, eo, (assumeYes ? false: true), messageWasDisplayed, ecs);

  if (result != S_OK)
  {
    if (result == E_ABORT)
    return NExitCode::kUserBreak;

    if (!messageWasDisplayed)
    {
    if (result == S_FALSE)
        ShowErrorMessage(L"Error in archive");
    else
        ErrorMessageForHRESULT(result);
    }

    if (result == E_OUTOFMEMORY)
    return NExitCode::kMemoryError;

    return NExitCode::kFatalError;
  }

  if (!ecs->IsOK())
    return NExitCode::kFatalError;

  HANDLE hProcess = NULL;
#ifdef MY_SHELL_EXECUTE
  if (!executeFile.IsEmpty())
  {
    CSysString filePath (GetSystemString(executeFile));
    SHELLEXECUTEINFO execInfo;
    execInfo.cbSize = sizeof(execInfo);
    execInfo.fMask = SEE_MASK_NOCLOSEPROCESS
      #ifndef UNDER_CE
      | SEE_MASK_FLAG_DDEWAIT
      #endif
      ;
    execInfo.hwnd = NULL;
    execInfo.lpVerb = NULL;
    execInfo.lpFile = filePath;

    if (!switches.IsEmpty())
    {
      executeParameters.Add_Space_if_NotEmpty();
      executeParameters += switches;
    }

    const CSysString parametersSys (GetSystemString(executeParameters));
    if (parametersSys.IsEmpty())
      execInfo.lpParameters = NULL;
    else
      execInfo.lpParameters = parametersSys;

    execInfo.lpDirectory = NULL;
    execInfo.nShow = SW_SHOWNORMAL;
    execInfo.hProcess = NULL;
    /* BOOL success = */ ::ShellExecuteEx(&execInfo);
    UINT32 result = (UINT32)(UINT_PTR)execInfo.hInstApp;
    if (result <= 32)
    {
      if (!assumeYes)
        ShowErrorMessage(L"Cannot open file");
      return 1;
    }
    hProcess = execInfo.hProcess;
  }
  else
#endif
  {
    if (appLaunched.IsEmpty())
    {
      appLaunched = L"setup.exe";
      if (!NFind::DoesFileExist_FollowLink(us2fs(appLaunched)))
      {
        if (!assumeYes)
          ShowErrorMessage(L"Cannot find setup.exe");
        return NExitCode::kWarning;
      }
    }
    
    const UString appNameForError = appLaunched; // actually we need to rtemove parameters also

    if (!switches.IsEmpty())
    {
      appLaunched.Add_Space();
      appLaunched += switches;
    }
    STARTUPINFO startupInfo;
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.lpReserved = NULL;
    startupInfo.lpDesktop = NULL;
    startupInfo.lpTitle = NULL;
    startupInfo.dwFlags = 0;
    startupInfo.cbReserved2 = 0;
    startupInfo.lpReserved2 = NULL;
    
    PROCESS_INFORMATION processInformation;
    
    const CSysString appLaunchedSys (GetSystemString(dirPrefix + appLaunched));
    
    const BOOL createResult = CreateProcess(NULL,
        appLaunchedSys.Ptr_non_const(),
        NULL, NULL, FALSE, 0, NULL, NULL /*tempDir.GetPath() */,
        &startupInfo, &processInformation);
    if (createResult == 0)
    {
      if (!assumeYes)
      {
        // we print name of exe file, if error message is
        // ERROR_BAD_EXE_FORMAT: "%1 is not a valid Win32 application".
        ShowErrorMessage(appNameForError);
      }
      return 1;
    }
    ::CloseHandle(processInformation.hThread);
    hProcess = processInformation.hProcess;
  }
  if (hProcess)
  {
    WaitForSingleObject(hProcess, INFINITE);
    ::CloseHandle(hProcess);
  }

  return NExitCode::kSuccess;
}

#if defined(_WIN32) && defined(_UNICODE) && !defined(_WIN64) && !defined(UNDER_CE)
#define NT_CHECK_FAIL_ACTION ShowErrorMessage(L"Unsupported Windows version"); return NExitCode::kFatalError;
#endif

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE /* hPrevInstance */,
  #ifdef UNDER_CE
  LPWSTR
  #else
  LPSTR
  #endif
  /* lpCmdLine */, int /* nCmdShow */)
{
  g_hInstance = (HINSTANCE)hInstance;

  NT_CHECK

  try
  {
    #ifdef _WIN32
    LoadSecurityDlls();
    #endif

    return WinMain2();
  }
  catch(const CNewException &)
  {
    ErrorMessageForHRESULT(E_OUTOFMEMORY);
    return NExitCode::kMemoryError;
  }
  catch(...)
  {
    ShowErrorMessage(kUnknownExceptionMessage);
    return NExitCode::kFatalError;
  }
}
