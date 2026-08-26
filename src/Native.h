// Copyright 2026 Metaversal Corporation. All rights reserved.

#ifndef RUBIDIUM_NATIVE_H
#define RUBIDIUM_NATIVE_H

#ifdef RUBIDIUM_PLATFORM_WINDOWS

#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>  
#include <uxtheme.h>
#include <vssym32.h>

// winnt.h defines CONTEXT (_CONTEXT). MSVC preprocesses SNEEZE::CONTEXT in PCH
// and headers included after windows.h to SNEEZE::_CONTEXT.
#ifdef CONTEXT
#undef CONTEXT
#endif

#include "shell/App_Win32.h"
#include "updater/UpdaterWnd.h"
#include "shell/AppFrame_Win32.h"
#include "canvas/Canvas_Win32.h"

#elif defined RUBIDIUM_PLATFORM_IOS

#include "updater/Updater.h"
#include "shell/AppFrame_iOS.h"
#include "canvas/Canvas_macOS.h"

#elif defined RUBIDIUM_PLATFORM_LINUX

#include "updater/UpdaterGeneric.h"
#include "shell/AppFrame_Linux.h"
#include "canvas/Canvas_Linux.h"

#elif defined RUBIDIUM_PLATFORM_MACOS

#include "updater/UpdaterGeneric.h"
#include "shell/AppFrame_Mac.h"
#include "canvas/Canvas_macOS.h"

#elif defined RUBIDIUM_PLATFORM_ANDROID

#include "updater/UpdaterGeneric.h"
#include "shell/AppFrame_Android.h"
#include "canvas/Canvas_Android.h"

#endif

#endif // RUBIDIUM_NATIVE_H
