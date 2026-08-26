// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "cli/MsfCli.h"

namespace RUBIDIUM
{

// ---------------------------------------------------------------------------
// File helpers
// ---------------------------------------------------------------------------

static std::string ReadFile (const char* sPath)
{
   std::string sResult;
   std::ifstream File (sPath, std::ios::binary);

   if (File.is_open ())
   {
      std::ostringstream Stream;
      Stream << File.rdbuf ();
      sResult = Stream.str ();
   }

   return sResult;
}

static bool WriteFile (const char* sPath, const std::string& sData)
{
   bool bResult = false;
   std::ofstream File (sPath, std::ios::binary);

   if (File.is_open ())
   {
      File.write (sData.data (), (std::streamsize) sData.size ());
      bResult = File.good ();
   }

   return bResult;
}

static void PrintUsage ()
{
   std::cerr
      << "Usage:\n"
      << "  Sign:    Rubidium --sign --payload <json> --key <key.pem> --cert <cert.pem>\n"
      << "                   [--chain <ca.pem>] [--alg RS256] --out <file.msf>\n"
      << "\n"
      << "  Verify:  Rubidium --verify <file.msf> [--trust <ca.pem>]\n";
}

// ---------------------------------------------------------------------------
// Verify mode
// ---------------------------------------------------------------------------

static void PrintCertChain (const SNEEZE::MSF& msf)
{
   const std::vector<SNEEZE::MSF::CERT>& aCert = msf.Certs ();

   for (size_t i = 0; i < aCert.size (); ++i)
   {
      const SNEEZE::MSF::CERT& Cert = aCert[i];
      const char* sLabel = Cert.bIsCA ? "CA" : "Leaf";

      std::cout << "\n";
      std::cout << "  Certificate [" << i << "] (" << sLabel << ")\n";
      std::cout << "    Subject:    " << Cert.sSubject   << "\n";
      std::cout << "    Issuer:     " << Cert.sIssuer    << "\n";
      std::cout << "    Serial:     " << Cert.sSerial    << "\n";
      std::cout << "    Not Before: " << Cert.sNotBefore << "\n";
      std::cout << "    Not After:  " << Cert.sNotAfter  << "\n";
      std::cout << "    Key:        " << Cert.sKeyType << " " << Cert.nKeyBits << "-bit\n";
   }
}

static int DoVerify (const char* sMsfPath, const std::vector<const char*>& aTrustPath)
{
   int nResult = 1;

   std::string sJws = ReadFile (sMsfPath);

   if (sJws.empty ())
      std::cerr << "Error: cannot read file: " << sMsfPath << "\n";
   else
   {
      SNEEZE::MSF msf;

      for (const char* sTrustPath : aTrustPath)
      {
         std::string sPem = ReadFile (sTrustPath);
         if (!sPem.empty ())
            msf.TrustedCert_Add (sPem);
         else
            std::cerr << "Warning: cannot read trust cert: " << sTrustPath << "\n";
      }

      if (!msf.Parse (sJws, sMsfPath))
         std::cerr << "Error: failed to parse JWS from " << sMsfPath << "\n";
      else
      {
         msf.Signature_Verify ();
         msf.Chain_Verify ();

         std::cout << "File:        " << sMsfPath           << "\n";
         std::cout << "Algorithm:   " << msf.Algorithm ()   << "\n";
         std::cout << "Fingerprint: " << msf.Fingerprint () << "\n";

         std::string sSuccessor = msf.Successor ();
         if (!sSuccessor.empty ())
            std::cout << "Successor:   " << sSuccessor << "\n";

         if (msf.IsSignatureValid ()  &&  msf.IsChainTrusted ())
         {
            std::cout << "Signature:   VERIFIED\n";
            nResult = 0;
         }
         else
         {
            std::cout << "Signature:   FAILED\n";
            if (!msf.SignatureError ().empty ())
               std::cerr << "Sig error:   " << msf.SignatureError () << "\n";
            if (!msf.ChainError ().empty ())
               std::cerr << "Chain error: " << msf.ChainError () << "\n";
         }

         std::cout << "\n--- Certificate Chain ---";
         PrintCertChain (msf);

         std::cout << "\n--- Payload ---\n\n";
         nlohmann::json Payload = msf.Payload ();
         if (!Payload.is_null ())
            std::cout << Payload.dump (3) << "\n";
         else
            std::cout << "(empty)\n";
      }
   }

   return nResult;
}

// ---------------------------------------------------------------------------
// Sign mode
// ---------------------------------------------------------------------------

static int DoSign (const char* sPayloadPath, const char* sKeyPath,
                   const std::vector<const char*>& aCertPath,
                   const std::string& sAlgorithm, const char* sOutPath)
{
   int nResult = 1;

   std::string sPayload = ReadFile (sPayloadPath);
   std::string sKey     = ReadFile (sKeyPath);

   // Parse without exceptions -- a malformed payload returns a discarded
   // value rather than throwing, so a bad --payload file reports cleanly
   // instead of crashing the process.
   nlohmann::json Payload = nlohmann::json::parse (sPayload, nullptr, false);

   if (sPayload.empty ())
      std::cerr << "Error: cannot read payload file: " << sPayloadPath << "\n";
   else if (sKey.empty ())
      std::cerr << "Error: cannot read key file: " << sKeyPath << "\n";
   else if (Payload.is_discarded ())
      std::cerr << "Error: invalid payload JSON: " << sPayloadPath << "\n";
   else
   {
      SNEEZE::MSF msf;

      msf.Payload (Payload);

      bool bCertsOk = true;
      for (const char* sCertPath : aCertPath)
      {
         std::string sCert = ReadFile (sCertPath);
         if (sCert.empty ())
         {
            std::cerr << "Error: cannot read certificate file: " << sCertPath << "\n";
            bCertsOk = false;
            break;
         }
         msf.Cert_Add (sCert);
      }

      if (bCertsOk)
      {
         std::string sJws = msf.Sign (sKey, sAlgorithm);
         if (sJws.empty ())
            std::cerr << "Error: signing failed\n";
         else if (!WriteFile (sOutPath, sJws))
            std::cerr << "Error: cannot write output file: " << sOutPath << "\n";
         else
         {
            std::cout << "Signed " << sOutPath << " (" << sJws.size () << " bytes)\n";
            nResult = 0;
         }
      }
   }

   return nResult;
}

// ---------------------------------------------------------------------------
// Public entry points
// ---------------------------------------------------------------------------

bool MSF_CLI::IsCommand (int nArgc, char** aArgv)
{
   bool bResult = false;

   for (int i = 1; i < nArgc  &&  !bResult; ++i)
   {
      if (strcmp (aArgv[i], "--sign") == 0  ||  strcmp (aArgv[i], "--verify") == 0)
         bResult = true;
   }

   return bResult;
}

int MSF_CLI::Run (int nArgc, char** aArgv)
{
   const char* sPayloadPath = nullptr;
   const char* sKeyPath     = nullptr;
   const char* sOutPath     = nullptr;
   const char* sVerifyPath  = nullptr;
   std::string sAlgorithm   = "RS256";
   std::vector<const char*> aCertPath;
   std::vector<const char*> aTrustPath;
   bool bSign    = false;
   bool bBadArgs = false;

   for (int i = 1; i < nArgc  &&  !bBadArgs; ++i)
   {
      if (strcmp (aArgv[i], "--sign") == 0)
         bSign = true;
      else if (strcmp (aArgv[i], "--verify") == 0  &&  i + 1 < nArgc)
         sVerifyPath = aArgv[++i];
      else if (strcmp (aArgv[i], "--trust") == 0  &&  i + 1 < nArgc)
         aTrustPath.push_back (aArgv[++i]);
      else if (strcmp (aArgv[i], "--payload") == 0  &&  i + 1 < nArgc)
         sPayloadPath = aArgv[++i];
      else if (strcmp (aArgv[i], "--key") == 0  &&  i + 1 < nArgc)
         sKeyPath = aArgv[++i];
      else if (strcmp (aArgv[i], "--cert") == 0  &&  i + 1 < nArgc)
         aCertPath.push_back (aArgv[++i]);
      else if (strcmp (aArgv[i], "--chain") == 0  &&  i + 1 < nArgc)
         aCertPath.push_back (aArgv[++i]);
      else if (strcmp (aArgv[i], "--alg") == 0  &&  i + 1 < nArgc)
         sAlgorithm = aArgv[++i];
      else if (strcmp (aArgv[i], "--out") == 0  &&  i + 1 < nArgc)
         sOutPath = aArgv[++i];
      else
      {
         std::cerr << "Unknown argument: " << aArgv[i] << "\n";
         bBadArgs = true;
      }
   }

   int nResult = 1;

   if (bBadArgs)
      PrintUsage ();
   else if (sVerifyPath)
      nResult = DoVerify (sVerifyPath, aTrustPath);
   else if (!bSign  ||  !sPayloadPath  ||  !sKeyPath  ||  aCertPath.empty ()  ||  !sOutPath)
      PrintUsage ();
   else
      nResult = DoSign (sPayloadPath, sKeyPath, aCertPath, sAlgorithm, sOutPath);

   return nResult;
}

} // namespace RUBIDIUM
