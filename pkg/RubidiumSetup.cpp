// Copyright 2026 Metaversal Corporation. All rights reserved.

#include <nlohmann/json.hpp>
#include <curl/curl.h>

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <string>
#include <fstream>
#include <sstream>
#include <chrono>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <bcrypt.h>
#include <commctrl.h>
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "comctl32.lib")
#else
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#ifndef RUBIDIUM_CDN_URL
#define RUBIDIUM_CDN_URL "https://cdn.rp1.com/rubidium/"
#endif

#ifndef RUBIDIUM_PLATFORM
#define RUBIDIUM_PLATFORM "windows-x64"
#endif

// macOS/Linux curl is BoringSSL-backed with no OS trust store. Sneeze's
// generated cacert_data.cpp (compiled into RubidiumSetup on those platforms)
// provides Mozilla's CA bundle. Windows uses Schannel + the Windows store.
#ifdef RUBIDIUM_SETUP_EMBED_CACERT
namespace SNEEZE
{
   extern const char* const         g_szCaCertPem;
   extern const unsigned long       g_nCaCertPemLen;
}
#endif

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------

static std::string g_sRubidiumPath;

static std::string GetUpdatesDir ();
static std::string GetUpdatesJsonPath ();
static void        Log (const char* szFormat, ...);

static nlohmann::json LoadUpdatesJson ();
static void           SaveUpdatesJson (const nlohmann::json& jData);

static void ApplyCaBundle (CURL* pCurl);
static bool FetchManifest (const std::string& sUrl, std::string& sResponse);
static bool ParseManifest (const std::string& sJson, const std::string& sPlatform,
                           std::string& sVersion, std::string& sDownloadUrl,
                           std::string& sSha256, std::string& sNotes);
static bool DownloadFile (const std::string& sUrl, const std::string& sDestPath,
                          bool bShowProgress);
static bool VerifySha256 (const std::string& sFilePath, const std::string& sExpectedHash);
static int  CompareVersions (const std::string& sA, const std::string& sB);
static bool LaunchInstaller (const std::string& sInstallerPath);

static int RunFreshInstall ();
static int RunCheck (const std::string& sCurrentVersion, bool bForce);
static int RunApply ();

// ---------------------------------------------------------------------------
// Win32 progress dialog (fresh install mode only)
// ---------------------------------------------------------------------------

#ifdef _WIN32

static const int STEP_COUNT    = 4;
static const int STEP_MANIFEST = 0;
static const int STEP_DOWNLOAD = 1;
static const int STEP_VERIFY   = 2;
static const int STEP_LAUNCH   = 3;

static const int IDC_STEP_BASE = 200;
static const int IDC_PROGRESS  = 102;

static HWND g_hDialog      = nullptr;
static HWND g_hProgressBar = nullptr;
static HWND g_aStepLabels[STEP_COUNT] = {};
static bool g_aStepDone[STEP_COUNT]   = {};

static const wchar_t* g_aStepTexts[STEP_COUNT] =
{
   L"  Fetching release information",
   L"  Downloading",
   L"  Verifying download",
   L"  Launching installer"
};

static void PumpMessages ()
{
   MSG pMsg;
   while (PeekMessageA (&pMsg, nullptr, 0, 0, PM_REMOVE))
   {
      TranslateMessage (&pMsg);
      DispatchMessageA (&pMsg);
   }
}

static LRESULT CALLBACK DialogProc (HWND hWnd, UINT nMsg, WPARAM wParam, LPARAM lParam)
{
   if (nMsg == WM_CLOSE)
   {
      PostQuitMessage (0);
      return 0;
   }

   if (nMsg == WM_CTLCOLORSTATIC)
   {
      HDC hDc    = (HDC)wParam;
      HWND hCtrl = (HWND)lParam;
      for (int i = 0; i < STEP_COUNT; i++)
      {
         if (hCtrl == g_aStepLabels[i])
         {
            if (g_aStepDone[i])
               SetTextColor (hDc, RGB (34, 139, 34));

            SetBkMode (hDc, TRANSPARENT);
            return (LRESULT)GetStockObject (HOLLOW_BRUSH);
         }
      }
   }

   return DefWindowProcA (hWnd, nMsg, wParam, lParam);
}

static HWND CreateProgressDialog ()
{
   INITCOMMONCONTROLSEX pIcc = {};
   pIcc.dwSize = sizeof (pIcc);
   pIcc.dwICC  = ICC_PROGRESS_CLASS;
   InitCommonControlsEx (&pIcc);

   WNDCLASSEXA pWc = {};
   pWc.cbSize        = sizeof (pWc);
   pWc.lpfnWndProc   = DialogProc;
   pWc.hInstance      = GetModuleHandleA (nullptr);
   pWc.hCursor        = LoadCursor (nullptr, IDC_ARROW);
   pWc.hbrBackground  = (HBRUSH)(COLOR_WINDOW + 1);
   pWc.lpszClassName  = "RubidiumSetupClass";
   pWc.hIcon          = LoadIconA (pWc.hInstance, "IDI_ICON1");
   RegisterClassExA (&pWc);

   int nScreenW = GetSystemMetrics (SM_CXSCREEN);
   int nScreenH = GetSystemMetrics (SM_CYSCREEN);
   int nWinW = 420;
   int nWinH = 210;

   g_hDialog = CreateWindowExA (
      0, "RubidiumSetupClass", "Rubidium Setup",
      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
      (nScreenW - nWinW) / 2, (nScreenH - nWinH) / 2,
      nWinW, nWinH,
      nullptr, nullptr, pWc.hInstance, nullptr
   );

   for (int i = 0; i < STEP_COUNT; i++)
   {
      g_aStepLabels[i] = CreateWindowExW (
         0, L"STATIC", g_aStepTexts[i],
         WS_CHILD | WS_VISIBLE | SS_LEFT,
         20, 15 + i * 25, 370, 20,
         g_hDialog, (HMENU)(INT_PTR)(IDC_STEP_BASE + i), pWc.hInstance, nullptr
      );
      g_aStepDone[i] = false;
   }

   g_hProgressBar = CreateWindowExA (
      0, PROGRESS_CLASSA, nullptr,
      WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
      20, 15 + STEP_COUNT * 25 + 10, 370, 25,
      g_hDialog, (HMENU)(INT_PTR)IDC_PROGRESS, pWc.hInstance, nullptr
   );

   SendMessageA (g_hProgressBar, PBM_SETRANGE, 0, MAKELPARAM (0, 100));
   SendMessageA (g_hProgressBar, PBM_SETPOS, 0, 0);

   ShowWindow (g_hDialog, SW_SHOW);
   UpdateWindow (g_hDialog);

   return g_hDialog;
}

static void BeginStep (int nStep)
{
   if (nStep < 0  ||  nStep >= STEP_COUNT)
      return;

   wchar_t szText[256] = {};
   GetWindowTextW (g_aStepLabels[nStep], szText, 256);

   std::wstring sNew = szText;
   sNew += L"...";

   SetWindowTextW (g_aStepLabels[nStep], sNew.c_str ());
   PumpMessages ();
}

static void MarkStepComplete (int nStep)
{
   if (nStep < 0  ||  nStep >= STEP_COUNT)
      return;

   g_aStepDone[nStep] = true;

   wchar_t szText[256] = {};
   GetWindowTextW (g_aStepLabels[nStep], szText, 256);

   std::wstring sNew = szText;
   sNew += L"  \u2714";

   SetWindowTextW (g_aStepLabels[nStep], sNew.c_str ());
   InvalidateRect (g_aStepLabels[nStep], nullptr, TRUE);
   PumpMessages ();
}

static void SetStepText (int nStep, const wchar_t* szText)
{
   if (nStep < 0  ||  nStep >= STEP_COUNT)
      return;

   std::wstring sText = L"  ";
   sText += szText;
   SetWindowTextW (g_aStepLabels[nStep], sText.c_str ());
   PumpMessages ();
}

static void SetProgress (int nPercent)
{
   if (g_hProgressBar)
      SendMessageA (g_hProgressBar, PBM_SETPOS, nPercent, 0);

   PumpMessages ();
}

#endif // _WIN32

// ---------------------------------------------------------------------------
// curl write callbacks
// ---------------------------------------------------------------------------

static size_t StringWriteCallback (void* pContents, size_t nSize, size_t nCount, void* pUserData)
{
   std::string* pStr = static_cast<std::string*> (pUserData);
   pStr->append (static_cast<char*> (pContents), nSize * nCount);
   return nSize * nCount;
}

struct FILE_DOWNLOAD_CONTEXT
{
   std::ofstream* pFile;
   bool           bShowProgress;
   curl_off_t     nTotalBytes;
};

static size_t FileWriteCallback (void* pContents, size_t nSize, size_t nCount, void* pUserData)
{
   FILE_DOWNLOAD_CONTEXT* pCtx = static_cast<FILE_DOWNLOAD_CONTEXT*> (pUserData);
   pCtx->pFile->write (static_cast<char*> (pContents), nSize * nCount);
   return nSize * nCount;
}

static int ProgressCallback (void* pUserData, curl_off_t nDlTotal, curl_off_t nDlNow,
                             curl_off_t /*nUlTotal*/, curl_off_t /*nUlNow*/)
{
#ifdef _WIN32
   (void) pUserData;
   if (nDlTotal > 0)
   {
      int nPercent = static_cast<int> ((nDlNow * 100) / nDlTotal);
      SetProgress (nPercent);
   }
#else
   (void) pUserData;
   (void) nDlTotal;
   (void) nDlNow;
#endif
   return 0;
}

// ---------------------------------------------------------------------------
// Path helpers
// ---------------------------------------------------------------------------

static std::string GetAppDataDir ()
{
   std::string sPath;

#ifdef _WIN32
   char        szPath[MAX_PATH] = {};

   if (SUCCEEDED (SHGetFolderPathA (nullptr, CSIDL_APPDATA, nullptr, 0, szPath)))
   {
      sPath = szPath;
      std::replace (sPath.begin (), sPath.end (), '\\', '/');
   }
   else
   {
      sPath = ".";
   }
#else
   #ifdef __ANDROID__
      const char* pszStorage = SDL_GetAndroidInternalStoragePath ();
      sPath = pszStorage ? std::string (pszStorage) : std::string (".");
   #elif defined(__APPLE__)
      const char* pszHome = std::getenv ("HOME");
      sPath = pszHome ? std::string (pszHome) + "/Library/Application Support" : std::string (".");
   #else
      const char* pszHome = std::getenv ("HOME");
      if (pszHome == nullptr)
         pszHome = "/tmp";

      sPath = std::string (pszHome) + "/.config";
      std::filesystem::create_directories (sPath);
   #endif
#endif

   return sPath;
}
static void GetRubidiumPath (std::string& sPath)
{
   sPath += "/Metaversal/Rubidium";
   std::filesystem::create_directories (sPath);
}

static std::string GetUpdatesDir ()
{
   std::string sDir = g_sRubidiumPath + "/Updates";

   fs::create_directories (sDir);

   return sDir;
}

static std::string GetUpdatesJsonPath ()
{
   return GetUpdatesDir () + "/Updates.json";
}

static void Log (const char* szFormat, ...)
{
   static std::string sLogPath;
   if (sLogPath.empty ())
      sLogPath = GetUpdatesDir () + "/RubidiumSetup.log";

   FILE* pFile = std::fopen (sLogPath.c_str (), "a");
   if (pFile)
   {
      auto tpNow  = std::chrono::system_clock::now ();
      time_t tmRaw = std::chrono::system_clock::to_time_t (tpNow);
      struct tm tmLocal = {};
#ifdef _WIN32
      localtime_s (&tmLocal, &tmRaw);
#else
      localtime_r (&tmRaw, &tmLocal);
#endif
      char szTimestamp[32] = {};
      std::strftime (szTimestamp, sizeof (szTimestamp), "%Y-%m-%d %H:%M:%S", &tmLocal);
      std::fprintf (pFile, "[%s] ", szTimestamp);

      va_list pArgs;
      va_start (pArgs, szFormat);
      std::vfprintf (pFile, szFormat, pArgs);
      va_end (pArgs);
      std::fputc ('\n', pFile);
      std::fclose (pFile);
   }
}

// ---------------------------------------------------------------------------
// Updates.json persistence
// ---------------------------------------------------------------------------

static nlohmann::json LoadUpdatesJson ()
{
   nlohmann::json jData = nlohmann::json::object ();

   std::ifstream fIn (GetUpdatesJsonPath ());
   if (fIn.is_open ())
   {
      try
      {
         jData = nlohmann::json::parse (fIn);
      }
      catch (const nlohmann::json::exception&)
      {
         jData = nlohmann::json::object ();
      }
   }

   return jData;
}

static void SaveUpdatesJson (const nlohmann::json& jData)
{
   std::string sPath = GetUpdatesJsonPath ();
   fs::create_directories (GetUpdatesDir ());

   std::ofstream fOut (sPath);
   if (fOut.is_open ())
      fOut << jData.dump (3) << "\n";
}

// ---------------------------------------------------------------------------
// Manifest fetch and parse
// ---------------------------------------------------------------------------

static void ApplyCaBundle (CURL* pCurl)
{
#ifdef RUBIDIUM_SETUP_EMBED_CACERT
   static curl_blob caBlob;

   caBlob.data  = const_cast<void*> (static_cast<const void*> (SNEEZE::g_szCaCertPem));
   caBlob.len   = SNEEZE::g_nCaCertPemLen;
   caBlob.flags = CURL_BLOB_NOCOPY;
   curl_easy_setopt (pCurl, CURLOPT_CAINFO_BLOB, &caBlob);
#else
   (void) pCurl;
#endif
}

static bool FetchManifest (const std::string& sUrl, std::string& sResponse)
{
   bool bOk = false;
   CURL* pCurl = curl_easy_init ();

   if (pCurl)
   {
      curl_easy_setopt (pCurl, CURLOPT_URL, sUrl.c_str ());
      curl_easy_setopt (pCurl, CURLOPT_WRITEFUNCTION, StringWriteCallback);
      curl_easy_setopt (pCurl, CURLOPT_WRITEDATA, &sResponse);
      curl_easy_setopt (pCurl, CURLOPT_FOLLOWLOCATION, 1L);
      curl_easy_setopt (pCurl, CURLOPT_TIMEOUT, 30L);
      ApplyCaBundle (pCurl);

      CURLcode nResult = curl_easy_perform (pCurl);
      if (nResult == CURLE_OK)
      {
         long nHttpCode = 0;
         curl_easy_getinfo (pCurl, CURLINFO_RESPONSE_CODE, &nHttpCode);
         bOk = (nHttpCode == 200);
      }
      else
      {
         std::string sErr = "Fetch failed for " + sUrl + " curl=" + std::to_string (nResult) + " (" + curl_easy_strerror (nResult) + ")";
         Log ("FetchManifest: %s", sErr.c_str ());
      }

      curl_easy_cleanup (pCurl);
   }

   return bOk;
}

static bool ParseManifest (const std::string& sJson, const std::string& sPlatform,
                           std::string& sVersion, std::string& sDownloadUrl,
                           std::string& sSha256, std::string& sNotes)
{
   bool bOk = false;

   try
   {
      nlohmann::json jManifest = nlohmann::json::parse (sJson);

      if (jManifest.contains ("current")  &&  jManifest["current"].contains ("stable"))
      {
         const auto& jStable = jManifest["current"]["stable"];

         if (jStable.contains (sPlatform))
         {
            sVersion = jStable[sPlatform].get<std::string> ();

            const auto& jReleases = jManifest["releases"];
            if (jReleases.contains (sPlatform)  &&
                jReleases[sPlatform].contains (sVersion))
            {
               const auto& jEntry = jReleases[sPlatform][sVersion];
               sDownloadUrl = jEntry.value ("url", "");
               sSha256      = jEntry.value ("sha256", "");
               sNotes       = jEntry.value ("notes", "");
            }

            bOk = !sDownloadUrl.empty ();
         }
      }
   }
   catch (const nlohmann::json::exception& e)
   {
      std::fprintf (stderr, "RubidiumSetup: manifest parse error -- %s\n", e.what ());
   }

   return bOk;
}

// ---------------------------------------------------------------------------
// File download
// ---------------------------------------------------------------------------

static bool DownloadFile (const std::string& sUrl, const std::string& sDestPath,
                          bool bShowProgress)
{
   bool bOk = false;
   std::ofstream fOut (sDestPath, std::ios::binary);

   if (fOut.is_open ())
   {
      CURL* pCurl = curl_easy_init ();
      if (pCurl)
      {
         FILE_DOWNLOAD_CONTEXT pCtx = {};
         pCtx.pFile         = &fOut;
         pCtx.bShowProgress = bShowProgress;

         curl_easy_setopt (pCurl, CURLOPT_URL, sUrl.c_str ());
         curl_easy_setopt (pCurl, CURLOPT_WRITEFUNCTION, FileWriteCallback);
         curl_easy_setopt (pCurl, CURLOPT_WRITEDATA, &pCtx);
         curl_easy_setopt (pCurl, CURLOPT_FOLLOWLOCATION, 1L);
         curl_easy_setopt (pCurl, CURLOPT_TIMEOUT, 300L);
         ApplyCaBundle (pCurl);

         if (bShowProgress)
         {
            curl_easy_setopt (pCurl, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
            curl_easy_setopt (pCurl, CURLOPT_XFERINFODATA, nullptr);
            curl_easy_setopt (pCurl, CURLOPT_NOPROGRESS, 0L);
         }

         CURLcode nResult = curl_easy_perform (pCurl);
         if (nResult == CURLE_OK)
         {
            long nHttpCode = 0;
            curl_easy_getinfo (pCurl, CURLINFO_RESPONSE_CODE, &nHttpCode);
            bOk = (nHttpCode == 200);
         }

         curl_easy_cleanup (pCurl);
      }

      fOut.close ();
   }

   if (!bOk  &&  fs::exists (sDestPath))
      fs::remove (sDestPath);

   return bOk;
}

// ---------------------------------------------------------------------------
// SHA-256 verification
// ---------------------------------------------------------------------------

static bool VerifySha256 (const std::string& sFilePath, const std::string& sExpectedHash)
{
   bool bOk = false;

#ifdef _WIN32
   BCRYPT_ALG_HANDLE hAlg = nullptr;
   BCRYPT_HASH_HANDLE hHash = nullptr;
   NTSTATUS nStatus = BCryptOpenAlgorithmProvider (&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);

   if (BCRYPT_SUCCESS (nStatus))
   {
      nStatus = BCryptCreateHash (hAlg, &hHash, nullptr, 0, nullptr, 0, 0);
   }

   if (BCRYPT_SUCCESS (nStatus))
   {
      std::ifstream fIn (sFilePath, std::ios::binary);
      if (fIn.is_open ())
      {
         char aBuffer[8192];
         while (fIn.read (aBuffer, sizeof (aBuffer))  ||  fIn.gcount () > 0)
         {
            BCryptHashData (hHash, reinterpret_cast<PUCHAR> (aBuffer),
               static_cast<ULONG> (fIn.gcount ()), 0);
            if (fIn.eof ()) break;
         }

         UCHAR aDigest[32];
         nStatus = BCryptFinishHash (hHash, aDigest, 32, 0);

         if (BCRYPT_SUCCESS (nStatus))
         {
            char szHex[65] = {};
            for (int i = 0; i < 32; i++)
               std::sprintf (szHex + i * 2, "%02x", aDigest[i]);

            bOk = (sExpectedHash == szHex);
         }
      }
   }

   if (hHash)  BCryptDestroyHash (hHash);
   if (hAlg)   BCryptCloseAlgorithmProvider (hAlg, 0);
#else
   (void) sFilePath;
   (void) sExpectedHash;
   bOk = true;
#endif

   return bOk;
}

// ---------------------------------------------------------------------------
// Version comparison
// ---------------------------------------------------------------------------

static int CompareVersions (const std::string& sA, const std::string& sB)
{
   auto fnParse = [] (const std::string& s, int& nMajor, int& nMinor, int& nPatch)
   {
      nMajor = nMinor = nPatch = 0;
      std::sscanf (s.c_str (), "%d.%d.%d", &nMajor, &nMinor, &nPatch);
   };

   int nAMaj, nAMin, nAPat;
   int nBMaj, nBMin, nBPat;

   fnParse (sA, nAMaj, nAMin, nAPat);
   fnParse (sB, nBMaj, nBMin, nBPat);

   if (nAMaj != nBMaj) return nAMaj - nBMaj;
   if (nAMin != nBMin) return nAMin - nBMin;
   return nAPat - nBPat;
}

// ---------------------------------------------------------------------------
// Launch installer
// ---------------------------------------------------------------------------

#ifndef _WIN32
static std::string GetSetupExeDir ()
{
   std::string sDir = ".";

#ifdef __APPLE__
   char     szBuf[4096];
   uint32_t nSize = sizeof (szBuf);

   if (_NSGetExecutablePath (szBuf, &nSize) == 0)
   {
      char szReal[4096];

      if (realpath (szBuf, szReal))
         sDir = fs::path (szReal).parent_path ().string ();
      else
         sDir = fs::path (szBuf).parent_path ().string ();
   }
#else
   char        szBuf[4096];
   ssize_t     nLen = readlink ("/proc/self/exe", szBuf, sizeof (szBuf) - 1);

   if (nLen > 0)
   {
      szBuf[nLen] = '\0';
      sDir = fs::path (szBuf).parent_path ().string ();
   }
#endif

   return sDir;
}
#endif

#if !defined(_WIN32) && !defined(__APPLE__)
static bool ApplyLinuxTarball (const std::string& sTarball)
{
   std::string     sBinDir      = GetSetupExeDir ();                            // <root>/bin
   std::string     sInstallRoot = fs::path (sBinDir).parent_path ().string (); // <root>
   std::string     sLogPath     = GetUpdatesDir () + "/RubidiumSetup.log";
   std::error_code ec;

   // Extract into a scratch dir first, then normalize: the tarball may be rooted
   // directly at bin/ (CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF) or wrapped in a
   // version-named top dir like "Rubidium-0.0.12-linux-x64/bin/" (CPack's
   // default). Extracting straight over the install root in the latter case
   // silently drops a stray folder beside bin/ and never updates the binary.
   std::string sStage = GetUpdatesDir () + "/stage";
   fs::remove_all (sStage, ec);
   fs::create_directories (sStage, ec);

   std::string sCmd = "tar -xzf \"" + sTarball + "\" -C \"" + sStage + "\" 2>> \"" + sLogPath + "\"";
   int         nRc  = std::system (sCmd.c_str ());
   bool        bOk  = (nRc == 0);

   if (!bOk)
      Log ("apply: tar extract failed (rc=%d)", nRc);

   // Locate the directory that actually contains bin/: either the stage root or
   // a single version-named subdirectory inside it.
   std::string sPayload;

   if (bOk)
   {
      if (fs::exists (fs::path (sStage) / "bin"))
      {
         sPayload = sStage;
      }
      else
      {
         for (const auto& Entry : fs::directory_iterator (sStage, ec))
         {
            if (Entry.is_directory (ec)  &&  fs::exists (Entry.path () / "bin"))
            {
               sPayload = Entry.path ().string ();
               break;
            }
         }
      }

      if (sPayload.empty ())
      {
         bOk = false;
         Log ("apply: extracted archive has no bin/ directory");
      }
   }

   if (bOk)
   {
      // The running RubidiumSetup (and possibly a still-exiting Rubidium) cannot be
      // truncated in place (ETXTBSY). Rename them aside first -- on Linux the dir
      // entry moves immediately and any live inode survives until its process
      // exits -- so the copy below writes fresh files at the original paths.
      std::string sSelf       = sBinDir + "/RubidiumSetup";
      std::string sSelfOld     = sBinDir + "/RubidiumSetup.old";
      std::string sRubidium     = sBinDir + "/Rubidium";
      std::string sRubidiumOld  = sBinDir + "/Rubidium.old";

      fs::remove (sSelfOld,    ec);
      fs::remove (sRubidiumOld, ec);
      fs::rename (sSelf,    sSelfOld,    ec);
      fs::rename (sRubidium, sRubidiumOld, ec);

      // Copy the payload contents over the install root, overwriting in place.
      // cp -a preserves permissions/symlinks; the trailing "/." copies the
      // directory's contents (not the directory itself) into the destination.
      std::string sCopy   = "cp -a \"" + sPayload + "/.\" \"" + sInstallRoot + "\" 2>> \"" + sLogPath + "\"";
      int         nCopyRc = std::system (sCopy.c_str ());
      bOk = (nCopyRc == 0);

      if (bOk)
      {
         // Unlinking a running executable is legal on Linux; the inode stays
         // live until the owning process exits, so nothing is left behind.
         fs::remove (sSelfOld,    ec);
         fs::remove (sRubidiumOld, ec);
         Log ("apply: installed new files under %s", sInstallRoot.c_str ());
      }
      else
      {
         // Restore the originals so a failed copy doesn't leave a broken install.
         fs::rename (sSelfOld,    sSelf,    ec);
         fs::rename (sRubidiumOld, sRubidium, ec);
         Log ("apply: copy into install root failed (rc=%d)", nCopyRc);
      }
   }

   fs::remove_all (sStage, ec);

   return bOk;
}
#endif

#ifdef __APPLE__
static bool ApplyMacDmg (const std::string& sDmg)
{
   // RubidiumSetup runs from <install>/Rubidium.app/Contents/MacOS/RubidiumSetup.
   // The installed bundle is two directories up from that MacOS folder, and the
   // install location is its parent.
   fs::path    MacOsDir   = GetSetupExeDir ();                          // .../Rubidium.app/Contents/MacOS
   fs::path    AppPath     = MacOsDir.parent_path ().parent_path ();    // .../Rubidium.app
   fs::path    InstallDir  = AppPath.parent_path ();                    // .../<install>

   std::string sLogPath  = GetUpdatesDir () + "/RubidiumSetup.log";
   std::string sMountDir  = GetUpdatesDir () + "/mnt";

   fs::path    NewApp = InstallDir / ".Rubidium.app.new";
   fs::path    OldApp = InstallDir / ".Rubidium.app.old";

   std::error_code ec;

   // This process's CWD may sit inside the bundle we are about to move aside
   // and delete. Switch to a directory that survives the swap so the shells
   // spawned by later std::system() calls don't fail their startup getcwd().
   int nChdir = chdir (InstallDir.c_str ());
   (void) nChdir;

   fs::remove_all (sMountDir, ec);
   fs::create_directories (sMountDir, ec);

   // Mount the DMG read-only without opening a Finder window.
   std::string sAttach = "hdiutil attach \"" + sDmg +
                         "\" -nobrowse -readonly -mountpoint \"" + sMountDir +
                         "\" >> \"" + sLogPath + "\" 2>&1";
   bool bOk = std::system (sAttach.c_str ()) == 0;

   // Locate the .app inside the mounted volume.
   fs::path SourceApp;
   if (bOk)
   {
      for (const auto& Entry : fs::directory_iterator (sMountDir, ec))
      {
         if (Entry.path ().extension () == ".app")
         {
            SourceApp = Entry.path ();
            break;
         }
      }

      bOk = !SourceApp.empty ();
      if (!bOk)
         Log ("apply: no .app found in mounted DMG");
   }

   // Copy the new bundle beside the current one (ditto preserves the code
   // signature), then swap it in with renames. Renaming the bundle that
   // contains this running RubidiumSetup is safe on macOS -- the process keeps
   // executing from its already-mapped image.
   if (bOk)
   {
      fs::remove_all (NewApp, ec);
      std::string sDitto = "ditto \"" + SourceApp.string () + "\" \"" + NewApp.string () +
                           "\" >> \"" + sLogPath + "\" 2>&1";
      bOk = std::system (sDitto.c_str ()) == 0;
      if (!bOk)
         Log ("apply: ditto copy failed");
   }

   if (bOk)
   {
      fs::remove_all (OldApp, ec);
      fs::rename (AppPath, OldApp, ec);

      if (ec)
      {
         bOk = false;
         Log ("apply: could not move current bundle aside (%s)", ec.message ().c_str ());
      }
      else
      {
         fs::rename (NewApp, AppPath, ec);

         if (ec)
         {
            // Restore the original bundle so the install isn't left empty.
            std::error_code ecRestore;
            fs::rename (OldApp, AppPath, ecRestore);
            bOk = false;
            Log ("apply: could not swap in new bundle (%s)", ec.message ().c_str ());
         }
         else
         {
            fs::remove_all (OldApp, ec);
         }
      }
   }

   // Always detach the DMG and clean up any leftover temp bundle.
   std::string sDetach = "hdiutil detach \"" + sMountDir +
                         "\" -force >> \"" + sLogPath + "\" 2>&1";
   std::system (sDetach.c_str ());
   fs::remove_all (sMountDir, ec);
   fs::remove_all (NewApp, ec);

   return bOk;
}
#endif

static bool LaunchInstaller (const std::string& sInstallerPath)
{
   bool bOk = false;

#ifdef _WIN32
   HINSTANCE hResult = ShellExecuteA (nullptr, "open", sInstallerPath.c_str (),
      nullptr, nullptr, SW_SHOWNORMAL);
   bOk = reinterpret_cast<intptr_t> (hResult) > 32;
#elif defined(__APPLE__)
   std::string sCmd = "open \"" + sInstallerPath + "\"";
   bOk = std::system (sCmd.c_str ()) == 0;
#else
   std::string sCmd = "xdg-open \"" + sInstallerPath + "\"";
   bOk = std::system (sCmd.c_str ()) == 0;
#endif

   return bOk;
}

// ---------------------------------------------------------------------------
// Mode: Fresh Install (default, no arguments)
// ---------------------------------------------------------------------------

static int RunFreshInstall ()
{
   int nExitCode = 1;

   curl_global_init (CURL_GLOBAL_DEFAULT);

#ifdef _WIN32
   CreateProgressDialog ();
   BeginStep (STEP_MANIFEST);
#endif

   std::string sManifestUrl = std::string (RUBIDIUM_CDN_URL) + "manifest.json";
   std::string sPlatform    = RUBIDIUM_PLATFORM;
   std::string sDestPath;

   std::string sManifestJson;
   bool bOk = FetchManifest (sManifestUrl, sManifestJson);

   std::string sVersion;
   std::string sDownloadUrl;
   std::string sSha256;
   std::string sNotes;

   if (bOk)
      bOk = ParseManifest (sManifestJson, sPlatform, sVersion, sDownloadUrl, sSha256, sNotes);

#ifdef _WIN32
   if (bOk)
      MarkStepComplete (STEP_MANIFEST);
#endif

   if (!bOk)
   {
#ifdef _WIN32
      MessageBoxA (g_hDialog, "Could not find a release for this platform.",
         "Rubidium Setup", MB_OK | MB_ICONERROR);
#endif
   }

   if (bOk)
   {
#ifdef _WIN32
      std::wstring sLabel = L"Downloading Rubidium ";
      for (char c : sVersion)
         sLabel += (wchar_t)c;
      SetStepText (STEP_DOWNLOAD, sLabel.c_str ());
      BeginStep (STEP_DOWNLOAD);
      SetProgress (0);
#endif

      std::string sUpdatesDir = GetUpdatesDir ();
      std::string sFilename;
      size_t nLastSlash = sDownloadUrl.rfind ('/');
      if (nLastSlash != std::string::npos)
         sFilename = sDownloadUrl.substr (nLastSlash + 1);
      else
         sFilename = "Rubidium-setup.exe";

      sDestPath = sUpdatesDir + "/" + sFilename;
      bOk = DownloadFile (sDownloadUrl, sDestPath, true);

#ifdef _WIN32
      if (bOk)
         MarkStepComplete (STEP_DOWNLOAD);
#endif
   }

   if (bOk  &&  !sSha256.empty ())
   {
#ifdef _WIN32
      BeginStep (STEP_VERIFY);
#endif
      bOk = VerifySha256 (sDestPath, sSha256);

#ifdef _WIN32
      if (bOk)
         MarkStepComplete (STEP_VERIFY);
#endif
   }
   else if (bOk)
   {
#ifdef _WIN32
      BeginStep (STEP_VERIFY);
      MarkStepComplete (STEP_VERIFY);
#endif
   }

   if (!bOk  &&  !sDestPath.empty ())
   {
#ifdef _WIN32
      MessageBoxA (g_hDialog, "Download failed or verification error.",
         "Rubidium Setup", MB_OK | MB_ICONERROR);
#endif
      fs::remove (sDestPath);
   }

   if (bOk)
   {
#ifdef _WIN32
      BeginStep (STEP_LAUNCH);
#endif
      // Hand the installed version's release notes to the freshly-installed
      // client so it shows a "Release Notes" popup on its first launch (same
      // mechanism as --apply). Written before launching the installer so the
      // entry is in place regardless of how quickly the installer detaches.
      if (!sVersion.empty ())
      {
         nlohmann::json jUpdates = LoadUpdatesJson ();
         jUpdates["release_notes"] = {
            {"version", sVersion},
            {"notes",   sNotes}
         };
         SaveUpdatesJson (jUpdates);
      }

      bOk = LaunchInstaller (sDestPath);

#ifdef _WIN32
      if (bOk)
         MarkStepComplete (STEP_LAUNCH);
#endif

      if (bOk)
         nExitCode = 0;
   }

   curl_global_cleanup ();

#ifdef _WIN32
   if (g_hDialog)
      DestroyWindow (g_hDialog);
#endif

   return nExitCode;
}

// ---------------------------------------------------------------------------
// Mode: Check (--check VERSION [--force])
// ---------------------------------------------------------------------------

static int RunCheck (const std::string& sCurrentVersion, bool bForce)
{
   int nExitCode = 0;

   Log ("check: current=%s force=%d", sCurrentVersion.c_str (), bForce);

   nlohmann::json jUpdates = LoadUpdatesJson ();

   bool bShouldCheck = bForce;
   if (!bForce)
   {
      int64_t tmLastCheck = jUpdates.value ("last_check", (int64_t)0);
      auto tpNow = std::chrono::system_clock::now ();
      int64_t tmNow = std::chrono::duration_cast<std::chrono::seconds> (
         tpNow.time_since_epoch ()).count ();

      bShouldCheck = ((tmNow - tmLastCheck) >= 24 * 60 * 60);
   }

   if (bShouldCheck)
   {
      curl_global_init (CURL_GLOBAL_DEFAULT);

      std::string sManifestUrl = std::string (RUBIDIUM_CDN_URL) + "manifest.json";
      std::string sPlatform    = RUBIDIUM_PLATFORM;
      Log ("check: manifest url=%s", sManifestUrl.c_str ());

      std::string sManifestJson;
      bool bOk = FetchManifest (sManifestUrl, sManifestJson);

      {
         auto tpNow = std::chrono::system_clock::now ();
         jUpdates["last_check"] = std::chrono::duration_cast<std::chrono::seconds> (
            tpNow.time_since_epoch ()).count ();
         SaveUpdatesJson (jUpdates);
      }

      std::string sVersion;
      std::string sDownloadUrl;
      std::string sSha256;
      std::string sNotes;

      if (bOk)
      {
         bOk = ParseManifest (sManifestJson, sPlatform, sVersion, sDownloadUrl, sSha256, sNotes);
      }

      if (bOk)
      {
         int nCmp = CompareVersions (sVersion, sCurrentVersion);
         bOk = nCmp > 0;
      }

      if (bOk)
      {
         Log ("check: update available %s -> %s", sCurrentVersion.c_str (), sVersion.c_str ());

         std::string sUpdatesDir = GetUpdatesDir ();
         std::string sFilename;
         size_t nLastSlash = sDownloadUrl.rfind ('/');
         if (nLastSlash != std::string::npos)
            sFilename = sDownloadUrl.substr (nLastSlash + 1);
         else
            sFilename = "Rubidium-update.exe";

         std::string sDestPath = sUpdatesDir + "/" + sFilename;

         bOk = DownloadFile (sDownloadUrl, sDestPath, false);

         if (bOk  &&  !sSha256.empty ())
            bOk = VerifySha256 (sDestPath, sSha256);

         if (bOk)
         {
            auto tpNow = std::chrono::system_clock::now ();

            time_t tmRaw = std::chrono::system_clock::to_time_t (tpNow);
            struct tm tmLocal = {};
   #ifdef _WIN32
            gmtime_s (&tmLocal, &tmRaw);
   #else
            gmtime_r (&tmRaw, &tmLocal);
   #endif

            char szDate[32] = {};
            std::sprintf (szDate, "%04d-%02d-%02d",
               tmLocal.tm_year + 1900, tmLocal.tm_mon + 1, tmLocal.tm_mday);

            jUpdates["staged"] = {
               {"version",   sVersion},
               {"installer", sDestPath},
               {"sha256",    sSha256},
               {"notes",     sNotes},
               {"date",      std::string (szDate)}
            };
            SaveUpdatesJson (jUpdates);
            Log ("check: staged %s", sVersion.c_str ());
         }
         else
         {
            fs::remove (sDestPath);
            Log ("check: download/verify failed for %s", sVersion.c_str ());
         }
      }
      else if (!bOk  &&  sManifestJson.empty ())
      {
         Log ("check: manifest fetch failed!!!");
      }
      else
      {
         Log ("check: up to date");
      }

      curl_global_cleanup ();

   } // if (bShouldCheck)
   else
   {
      Log ("check: skipped (within 24h interval)");
   }
   return nExitCode;
}

// ---------------------------------------------------------------------------
// Mode: Apply (--apply)
// ---------------------------------------------------------------------------

static int RunApply ()
{
   int nExitCode = 1;

   nlohmann::json jUpdates = LoadUpdatesJson ();

   std::string sInstallerPath;
   if (jUpdates.contains ("staged"))
      sInstallerPath = jUpdates["staged"].value ("installer", "");

   bool bOk = !sInstallerPath.empty ()  &&  fs::exists (sInstallerPath);

   if (bOk)
   {
#if defined(__APPLE__)
      bOk = ApplyMacDmg (sInstallerPath);
#elif !defined(_WIN32)
      bOk = ApplyLinuxTarball (sInstallerPath);
#else
      bOk = LaunchInstaller (sInstallerPath);
#endif
   }

   if (bOk)
   {
      // Hand the applied version's release notes off to the freshly-installed
      // client. It reads "release_notes" on next launch (when its build version
      // matches) and shows a "Release Notes" popup, then clears the entry.
      std::string sVersion;
      std::string sNotes;
      if (jUpdates.contains ("staged"))
      {
         sVersion = jUpdates["staged"].value ("version", "");
         sNotes   = jUpdates["staged"].value ("notes", "");
      }

      std::error_code ec;
      fs::remove (sInstallerPath, ec);
      jUpdates.erase ("staged");

      if (!sVersion.empty ())
      {
         jUpdates["release_notes"] = {
            {"version", sVersion},
            {"notes",   sNotes}
         };
      }

      SaveUpdatesJson (jUpdates);

#if defined(__APPLE__)
      // Relaunch via LaunchServices, which starts the updated bundle fully
      // detached from this process. GetSetupExeDir() still resolves to the
      // (now-updated) bundle's MacOS folder, so its .app is two levels up.
      fs::path    AppPath   = fs::path (GetSetupExeDir ()).parent_path ().parent_path ();
      std::string sRelaunch = "open \"" + AppPath.string () + "\"";
      std::system (sRelaunch.c_str ());
#elif !defined(_WIN32)
      // Relaunch fully detached from this updater's controlling terminal:
      // setsid starts a new session and stdio is redirected to /dev/null.
      // Otherwise the GUI process inherits the terminal, and on exit its async
      // logger thread blocks flushing to that terminal -- the app then hangs
      // right after "Shutdown complete" (the join in DestroyLogger never returns).
      std::string sRelaunch = "setsid \"" + GetSetupExeDir () +
                              "/Rubidium\" </dev/null >/dev/null 2>&1 &";
      std::system (sRelaunch.c_str ());
#endif
      nExitCode = 0;
   }

   return nExitCode;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

#ifdef _WIN32
int WINAPI WinMain (HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/,
                    LPSTR lpCmdLine, int /*nShowCmd*/)
{
   int nArgc = 0;
   LPWSTR* aArgvW = CommandLineToArgvW (GetCommandLineW (), &nArgc);

   std::vector<std::string> aArgs;
   for (int i = 0; i < nArgc; i++)
   {
      int nLen = WideCharToMultiByte (CP_UTF8, 0, aArgvW[i], -1, nullptr, 0, nullptr, nullptr);
      std::string sArg (nLen - 1, '\0');
      WideCharToMultiByte (CP_UTF8, 0, aArgvW[i], -1, &sArg[0], nLen, nullptr, nullptr);
      aArgs.push_back (sArg);
   }
   LocalFree (aArgvW);
#else
int main (int nArgc, char* aArgv[])
{
   std::vector<std::string> aArgs;
   for (int i = 0; i < nArgc; i++)
      aArgs.push_back (aArgv[i]);
#endif
   int nExitCode = 1;

   g_sRubidiumPath = GetAppDataDir ();
   GetRubidiumPath (g_sRubidiumPath);

   if (aArgs.size () >= 2  &&  aArgs[1] == "--check")
   {
      std::string sCurrentVersion = (aArgs.size () >= 3) ? aArgs[2] : "0.0.0";
      bool bForce = false;
      for (size_t i = 3; i < aArgs.size (); i++)
      {
         if (aArgs[i] == "--force")
            bForce = true;
      }
      nExitCode = RunCheck (sCurrentVersion, bForce);
   }
   else if (aArgs.size () >= 2  &&  aArgs[1] == "--apply")
   {
      Log ("apply: launching staged installer");
      nExitCode = RunApply ();
   }
   else
   {
      Log ("install: fresh install started");
      nExitCode = RunFreshInstall ();
   }

   return nExitCode;
}
