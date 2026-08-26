// Copyright 2026 Metaversal Corporation. All rights reserved.
//
// Standalone build tool for managing pkg/manifest.json.
//
// Two commands:
//
//    GenerateManifest register <installer> <version> <platform> <cdn_url> <manifest> <notes_file>
//       Computes SHA-256 of the installer, adds a release entry under
//       releases[platform][version]. Reads release notes from <notes_file>
//       (line breaks normalized to paragraph spacing). Does NOT update the current channel index.
//
//    GenerateManifest promote <version> <platform> <channel> <manifest>
//       Points current[channel][platform] to the specified version.
//       The version must already exist under releases[platform][version].
//
// Examples:
//    GenerateManifest register
//       builds/windows-x64/install/release/pkg/Rubidium-0.0.2-windows-x64.exe
//       0.0.2 windows-x64 https://cdn.rp1.com/rubidium/ pkg/manifest.json RELEASE
//
//    GenerateManifest promote 0.0.2 windows-x64 stable pkg/manifest.json

#include <nlohmann/json.hpp>

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#else
#include <openssl/sha.h>
#endif

// ---------------------------------------------------------------------------

static std::string ComputeSha256 (const std::string& sFilePath)
{
   std::string sHash;

#ifdef _WIN32
   BCRYPT_ALG_HANDLE hAlg = nullptr;
   BCRYPT_HASH_HANDLE hHash = nullptr;
   NTSTATUS nStatus = BCryptOpenAlgorithmProvider (&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);

   if (BCRYPT_SUCCESS (nStatus))
      nStatus = BCryptCreateHash (hAlg, &hHash, nullptr, 0, nullptr, 0, 0);

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
            std::ostringstream ss;
            for (int i = 0; i < 32; i++)
               ss << std::hex << std::setfill ('0') << std::setw (2) << static_cast<int> (aDigest[i]);
            sHash = ss.str ();
         }
      }
   }

   if (hHash) BCryptDestroyHash (hHash);
   if (hAlg)  BCryptCloseAlgorithmProvider (hAlg, 0);
#else
   std::ifstream fIn (sFilePath, std::ios::binary);
   if (fIn.is_open ())
   {
      SHA256_CTX Ctx;
      int bOk = (SHA256_Init (&Ctx) == 1);
      char aBuffer[8192];
      while (bOk  &&  (fIn.read (aBuffer, sizeof (aBuffer))  ||  fIn.gcount () > 0))
      {
         if (SHA256_Update (&Ctx, aBuffer, static_cast<size_t> (fIn.gcount ())) != 1)
            bOk = 0;
         if (fIn.eof ())
            break;
      }

      unsigned char aDigest[SHA256_DIGEST_LENGTH];
      if (bOk  &&  SHA256_Final (aDigest, &Ctx) == 1)
      {
         std::ostringstream ss;
         for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
            ss << std::hex << std::setfill ('0') << std::setw (2) << static_cast<int> (aDigest[i]);
         sHash = ss.str ();
      }
   }
#endif

   return sHash;
}

// ---------------------------------------------------------------------------

static std::string TodayString ()
{
   std::time_t tmNow = std::time (nullptr);
   std::tm tmLocal = {};
#ifdef _WIN32
   localtime_s (&tmLocal, &tmNow);
#else
   localtime_r (&tmNow, &tmLocal);
#endif

   char szDate[16];
   std::strftime (szDate, sizeof (szDate), "%Y-%m-%d", &tmLocal);

   return std::string (szDate);
}

// ---------------------------------------------------------------------------

static std::string ExtractFilename (const std::string& sPath)
{
   size_t nSlash = sPath.find_last_of ("/\\");
   if (nSlash != std::string::npos)
      return sPath.substr (nSlash + 1);
   return sPath;
}

// ---------------------------------------------------------------------------

static nlohmann::json ReadManifest (const std::string& sManifestPath)
{
   nlohmann::json jManifest;

   std::ifstream fIn (sManifestPath);
   if (fIn.is_open ())
   {
      jManifest = nlohmann::json::parse (fIn);
   }
   else
   {
      jManifest["current"]  = nlohmann::json::object ();
      jManifest["releases"] = nlohmann::json::object ();
   }

   return jManifest;
}

// ---------------------------------------------------------------------------

static bool ReadNotesFile (const std::string& sNotesPath, std::string& sNotes)
{
   bool bOk = false;

   std::ifstream fIn (sNotesPath);
   if (fIn.is_open ())
   {
      std::ostringstream ss;
      ss << fIn.rdbuf ();
      sNotes = ss.str ();
      bOk    = true;
   }

   return bOk;
}

// ---------------------------------------------------------------------------

static void NormalizeNotesLineEndings (std::string& sNotes)
{
   std::string sOut;
   sOut.reserve (sNotes.size () * 2);

   for (size_t nIndex = 0; nIndex < sNotes.size (); )
   {
      if (nIndex + 1 < sNotes.size ()  &&
          sNotes[nIndex] == '\r'  &&  sNotes[nIndex + 1] == '\n')
      {
         sOut += "\n\n";
         nIndex += 2;
      }
      else if (sNotes[nIndex] == '\r'  ||  sNotes[nIndex] == '\n')
      {
         sOut += "\n\n";
         nIndex += 1;
      }
      else
      {
         sOut += sNotes[nIndex];
         nIndex += 1;
      }
   }

   // Cap runs of blank lines at one paragraph break.
   std::string sCollapsed;
   sCollapsed.reserve (sOut.size ());
   int nConsecutiveNewlines = 0;

   for (char cChar : sOut)
   {
      if (cChar == '\n')
      {
         nConsecutiveNewlines++;
         if (nConsecutiveNewlines <= 2)
            sCollapsed += cChar;
      }
      else
      {
         nConsecutiveNewlines = 0;
         sCollapsed += cChar;
      }
   }

   sNotes = std::move (sCollapsed);
}

// ---------------------------------------------------------------------------

static bool WriteManifest (const std::string& sManifestPath, const nlohmann::json& jManifest)
{
   std::ofstream fOut (sManifestPath);
   if (!fOut.is_open ())
   {
      std::fprintf (stderr, "Failed to write %s\n", sManifestPath.c_str ());
      return false;
   }

   fOut << jManifest.dump (3) << "\n";
   return true;
}

// ---------------------------------------------------------------------------

static void PrintUsage ()
{
   std::fprintf (stderr,
      "Usage:\n"
      "   GenerateManifest register <installer> <version> <platform> <cdn_url> <manifest> <notes_file>\n"
      "   GenerateManifest promote  <version> <platform> <channel> <manifest>\n");
}

// ---------------------------------------------------------------------------

static int DoRegister (int nArgc, char* aArgv[])
{
   if (nArgc < 8)
   {
      PrintUsage ();
      return 1;
   }

   std::string sInstallerPath = aArgv[2];
   std::string sVersion       = aArgv[3];
   std::string sPlatform      = aArgv[4];
   std::string sCdnUrl        = aArgv[5];
   std::string sManifestPath = aArgv[6];
   std::string sNotesPath    = aArgv[7];
   std::string sNotes;

   if (!ReadNotesFile (sNotesPath, sNotes))
   {
      std::fprintf (stderr, "Failed to read release notes file: %s\n", sNotesPath.c_str ());
      return 1;
   }

   NormalizeNotesLineEndings (sNotes);

   if (!sCdnUrl.empty ()  &&  sCdnUrl.back () != '/')
      sCdnUrl += '/';

   nlohmann::json jManifest = ReadManifest (sManifestPath);

   std::printf ("Computing SHA-256 of %s...\n", sInstallerPath.c_str ());
   std::string sSha256 = ComputeSha256 (sInstallerPath);
   if (sSha256.empty ())
   {
      std::fprintf (stderr, "Failed to compute SHA-256\n");
      return 1;
   }
   std::printf ("SHA-256: %s\n", sSha256.c_str ());

   std::string sFilename    = ExtractFilename (sInstallerPath);
   std::string sDownloadUrl = sCdnUrl + "releases/" + sVersion + "/" + sFilename;

   if (!jManifest["releases"].contains (sPlatform))
      jManifest["releases"][sPlatform] = nlohmann::json::object ();

   jManifest["releases"][sPlatform][sVersion] = {
      {"date",   TodayString ()},
      {"url",    sDownloadUrl},
      {"sha256", sSha256},
      {"notes",  sNotes}
   };

   if (!WriteManifest (sManifestPath, jManifest))
      return 1;

   std::printf ("Registered %s %s in %s\n",
      sPlatform.c_str (), sVersion.c_str (), sManifestPath.c_str ());

   return 0;
}

// ---------------------------------------------------------------------------

static int DoPromote (int nArgc, char* aArgv[])
{
   if (nArgc < 6)
   {
      PrintUsage ();
      return 1;
   }

   std::string sVersion      = aArgv[2];
   std::string sPlatform     = aArgv[3];
   std::string sChannel      = aArgv[4];
   std::string sManifestPath = aArgv[5];

   nlohmann::json jManifest = ReadManifest (sManifestPath);

   bool bFound = jManifest["releases"].contains (sPlatform)  &&
                  jManifest["releases"][sPlatform].contains (sVersion);

   if (!bFound)
   {
      std::fprintf (stderr, "No release entry found for %s %s\n",
         sPlatform.c_str (), sVersion.c_str ());
      return 1;
   }

   if (!jManifest["current"].contains (sChannel))
      jManifest["current"][sChannel] = nlohmann::json::object ();
   jManifest["current"][sChannel][sPlatform] = sVersion;

   if (!WriteManifest (sManifestPath, jManifest))
      return 1;

   std::printf ("Promoted %s %s to %s in %s\n",
      sPlatform.c_str (), sVersion.c_str (), sChannel.c_str (), sManifestPath.c_str ());

   return 0;
}

// ---------------------------------------------------------------------------

int main (int nArgc, char* aArgv[])
{
   if (nArgc < 2)
   {
      PrintUsage ();
      return 1;
   }

   std::string sCommand = aArgv[1];
   int nResult = 1;

   if (sCommand == "register")
      nResult = DoRegister (nArgc, aArgv);
   else if (sCommand == "promote")
      nResult = DoPromote (nArgc, aArgv);
   else
   {
      std::fprintf (stderr, "Unknown command: %s\n", sCommand.c_str ());
      PrintUsage ();
   }

   return nResult;
}
