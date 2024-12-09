// MyMessages.cpp

#include "StdAfx.h"

#include "MyMessages.h"

#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/ResourceString.h"

#include "../FileManager/LangUtils.h"

#include "7zip/Bundles/SFXWin/Configs.h"

using namespace NWindows;

extern bool g_DisableUserQuestions;

void ShowErrorMessage(HWND window, LPCWSTR message)
{
  UString title = L"7-Zip";

  if (!g_Configs.szErrorTitle.IsEmpty())
  {
    title = g_Configs.szErrorTitle;
  }

  if (!g_DisableUserQuestions)
    ::MessageBoxW(window, message, title.GetBuf(), MB_OK | MB_ICONSTOP);
}

void ShowErrorMessageHwndRes(HWND window, UInt32 resID)
{
  UString s = LangString(resID);
  if (s.IsEmpty())
    s.Add_UInt32(resID);
  ShowErrorMessage(window, s);
}

void ShowErrorMessageRes(UInt32 resID)
{
  ShowErrorMessageHwndRes(NULL, resID);
}

static void ShowErrorMessageDWORD(HWND window, DWORD errorCode)
{
  ShowErrorMessage(window, NError::MyFormatMessage(errorCode));
}

void ShowLastErrorMessage(HWND window)
{
  ShowErrorMessageDWORD(window, ::GetLastError());
}
