// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/network/Network.h"
#include "inspector/common/Collapse.h"

namespace RUBIDIUM
{

static const char* s_sRml =
R"rml(
<div class="detail-section"><span class="section-arrow">&#xE5C5;</span> Preview</div>
<div class="detail-section-body">
   <div id="preview-body" class="detail-preview-body">Select a request to view preview</div>
</div>
)rml";

static const std::vector<const char *> aExtGlb   = { ".glb", ".gltf" };
static const std::vector<const char *> aExtImage = { ".jpg", ".jpeg", ".png", ".gif", ".webp", ".bmp", ".tga", ".psd" };
static const std::vector<const char *> aExtMsf   = { ".msf" };

static bool IsFileExt (const std::vector<const char*>& aExt, const std::string& sUrl)
{
   bool bResult = false;
   std::string sLower;

   UTILS::ToLower (sUrl, sLower);

   for (const char* sExt : aExt)
   {
      size_t nExt = std::strlen (sExt);

      if (sLower.size () >= nExt && sLower.compare (sLower.size () - nExt, nExt, sExt) == 0)
      {
         bResult = true;
         break;
      }
   }

   return bResult;
}

static bool IsGltfType (const std::string& sContentType, const std::string& sUrl, const std::vector<uint8_t>& aData)
{
   bool bResult = false;

   if (sContentType.find ("gltf") != std::string::npos)
   {
      bResult = true;
   }
   else if (aData.size () >= 4 && aData[0] == 'g' && aData[1] == 'l' && aData[2] == 'T' && aData[3] == 'F')
   {
      bResult = true;
   }
   else bResult = IsFileExt (aExtGlb, sUrl);

   return bResult;
}

static bool IsImageType (const std::string& sContentType, const std::string& sUrl)
{
   bool bResult = false;

   if (sContentType.find ("image") != std::string::npos)
   {
      bResult = true;
   }
   else bResult = IsFileExt (aExtImage, sUrl);

   return bResult;
}

static bool IsMsfType (const std::string& sContentType, const std::string& sUrl)
{
   bool bResult = false;

   if (sContentType.compare ("application/jose+msf") == 0)
   {
      bResult = true;
   }
   else bResult = IsFileExt (aExtMsf, sUrl);

   return bResult;
}

static std::string MsfKv (const std::string& sKey, const std::string& sValue)
{
   return "<div class=\"detail-kv\"><div class=\"detail-key\">" + sKey +
      "</div><div class=\"detail-value\">" + UTILS::Escape (sValue, false) + "</div></div>";
}

static std::string MsfSubHeader (const std::string& sText)
{
   return "<div style=\"display: block; font-weight: bold; margin-top: 8dp; color: #202124;\">" +
      UTILS::Escape (sText, false) + "</div>";
}

// Renders the MSS structured view (container / services / modules) followed by
// the raw payload JSON -- the "payload" portion shared by the full MSF report
// and the standalone JOSE+JSON view.
static std::string MsfPayloadRml (SNEEZE::MSF& msf)
{
   std::string sRml;

// std::vector<SNEEZE::MSF::SERVICE> aServices  = msf.Services ();
   std::vector<SNEEZE::MSF::MODULE>  aModules   = msf.Modules ();
   std::string                       sContainer = msf.Container ();

   if (!sContainer.empty () /*|| !aServices.empty ()*/ || !aModules.empty ())
   {
      sRml += MsfSubHeader ("Payload");

      if (!sContainer.empty ())
         sRml += MsfKv ("Container:", sContainer);

/*
      for (const SNEEZE::MSF::SERVICE& svc : aServices)
      {
         sRml += MsfSubHeader (svc.sName.empty () ? "(unnamed service)" : svc.sName);
         if (!svc.sType.empty ())     sRml += MsfKv ("Type:", svc.sType);
         if (!svc.sEndpoint.empty ()) sRml += MsfKv ("Endpoint:", svc.sEndpoint);
      }
*/

      for (const SNEEZE::MSF::MODULE& mod : aModules)
      {
         sRml += MsfKv ("Module:", mod.sUrl);
         if (!mod.sHash.empty ())
            sRml += MsfKv ("Hash:", mod.sHash);
      }
   }

   std::string sJson = msf.Payload ().dump (2);
   sRml += MsfSubHeader ("Payload (Raw)");
   sRml += "<div class=\"detail-preview-body detail-json\" style=\"white-space: pre-wrap;\">" + UTILS::Escape (sJson, false) + "</div>";

   return sRml;
}

IW_NETWORK_DETAIL_PREVIEW::IW_NETWORK_DETAIL_PREVIEW ()
{
}

bool IW_NETWORK_DETAIL_PREVIEW::Initialize (Rml::Element* pContainer)
{
   m_pContainer = pContainer;

   m_pContainer->SetInnerRML (s_sRml);

   COLLAPSE::Attach (m_pContainer, this, m_apSections);

   return true;
}

void IW_NETWORK_DETAIL_PREVIEW::ShowMsf (Rml::Element* pBody, SNEEZE::FILE* pFile)
{
   std::vector<uint8_t> aData;
   pFile->ReadData (aData);

   if (aData.empty ())
   {
      pBody->SetInnerRML ("No content available");
      return;
   }

   std::string sJws (aData.begin (), aData.end ());

   SNEEZE::MSF msf;

   if (!msf.Parse (sJws, pFile->Url ()))
   {
      pBody->SetInnerRML ("<div class=\"detail-kv\"><div class=\"detail-value\">Not a valid MSF / JWS file</div></div>");
      return;
   }

   // Signature against the leaf certificate's public key. Chain validation is
   // best-effort (no OS trust store wired here) -- it only drives the expired
   // flag, mirroring the standalone MsfViewer which does not validate to root.
   bool bSigValid = msf.Signature_Verify ();
   msf.Chain_Verify ();
   bool bExpired  = msf.IsChainExpired ();

   std::string sRml;

   // --- Signature status banner ---
   std::string sColor;
   std::string sStatus;

   if (bSigValid && !bExpired)
   {
      sColor  = "#3fb950";
      sStatus = "&#x2713; Signature verified (" + UTILS::Escape (msf.Algorithm (), false) + ")";
   }
   else if (bSigValid && bExpired)
   {
      sColor  = "#d29922";
      sStatus = "&#x26A0; Signature valid (" + UTILS::Escape (msf.Algorithm (), false) + "), certificate expired";
   }
   else
   {
      sColor  = "#f85149";
      sStatus = "&#x2717; Signature verification failed";
      std::string sErr = msf.SignatureError ();
      if (!sErr.empty ())
         sStatus += " - " + UTILS::Escape (sErr, false);
   }

   sRml += "<div style=\"display: block; font-weight: bold; color: " + sColor + ";\">" + sStatus + "</div>";

   // --- Identity ---
   sRml += MsfSubHeader ("Identity");
   sRml += MsfKv ("Algorithm:", msf.Algorithm ());
   sRml += MsfKv ("Organization:", msf.DisplayOrganization ());
   sRml += MsfKv ("Fingerprint:", msf.Fingerprint ());

   // --- Certificate chain ---
   const std::vector<SNEEZE::MSF::CERT>& aCerts = msf.Certs ();
   sRml += MsfSubHeader ("Certificate Chain (" + std::to_string (aCerts.size ()) + ")");

   if (aCerts.empty ())
   {
      sRml += "<div class=\"detail-kv\"><div class=\"detail-value\">No certificates</div></div>";
   }
   else
   {
      for (size_t i = 0; i < aCerts.size (); i++)
      {
         const SNEEZE::MSF::CERT& cert = aCerts[i];

         std::string sLabel = (i == 0) ? "Leaf (signer)" : (cert.bIsCA ? "CA / Intermediate" : "Intermediate");
         sRml += MsfSubHeader (sLabel);
         sRml += MsfKv ("Subject:", cert.sSubject);
         sRml += MsfKv ("Issuer:", cert.sIssuer);
         if (!cert.sOrganization.empty ())
            sRml += MsfKv ("Organization:", cert.sOrganization);
         sRml += MsfKv ("Serial:", cert.sSerial);
         sRml += MsfKv ("Not Before:", cert.sNotBefore);
         sRml += MsfKv ("Not After:", cert.sNotAfter);
         sRml += MsfKv ("Key:", cert.sKeyType + " (" + std::to_string (cert.nKeyBits) + " bits)");
      }
   }

   // --- Payload (structured view + raw JSON) ---
   sRml += MsfPayloadRml (msf);

   pBody->SetInnerRML (sRml);
}

void IW_NETWORK_DETAIL_PREVIEW::ShowMsfJSON (Rml::Element* pBody, SNEEZE::FILE* pFile)
{
   std::vector<uint8_t> aData;
   pFile->ReadData (aData);

   if (aData.empty ())
   {
      pBody->SetInnerRML ("No content available");
      return;
   }

   std::string sJws (aData.begin (), aData.end ());

   SNEEZE::MSF msf;

   if (!msf.Parse (sJws, pFile->Url ()))
   {
      pBody->SetInnerRML ("<div class=\"detail-kv\"><div class=\"detail-value\">Not a valid JOSE / JWS file</div></div>");
      return;
   }

   pBody->SetInnerRML (MsfPayloadRml (msf));
}

// Renders a plain JSON file (application/json and friends) as pretty-printed,
// indented JSON. If the bytes don't parse as JSON they're shown verbatim so the
// user still sees the raw content instead of an error.
void IW_NETWORK_DETAIL_PREVIEW::ShowJSON (Rml::Element* pBody, SNEEZE::FILE* pFile)
{
   std::vector<uint8_t> aData;
   pFile->ReadData (aData);

   if (aData.empty ())
   {
      pBody->SetInnerRML ("No content available");
   }
   else
   {
      std::string sText (aData.begin (), aData.end ());

      nlohmann::json jDoc = nlohmann::json::parse (sText, nullptr, false);

      std::string sPretty = jDoc.is_discarded () ? sText : jDoc.dump (3);

      pBody->SetInnerRML ("<div class=\"detail-preview-body detail-json\" style=\"white-space: pre-wrap;\">" +
                          UTILS::Escape (sPretty, false) + "</div>");
   }
}

Rml::Element* IW_NETWORK_DETAIL_PREVIEW::PreviewBody () const
{
   return m_pContainer ? m_pContainer->GetElementById ("preview-body") : nullptr;
}

Rml::Element* IW_NETWORK_DETAIL_PREVIEW::PreviewHost () const
{
   Rml::Element* pResult = nullptr;

   if (Rml::Element* pBody = PreviewBody ())
      pResult = pBody->GetElementById ("preview-3d-host");

   return pResult;
}

void IW_NETWORK_DETAIL_PREVIEW::ShowFile (SNEEZE::FILE* pFile)
{
   if (!m_pContainer || !pFile)
      return;

   Rml::Element* pBody = m_pContainer->GetElementById ("preview-body");
   if (!pBody)
      return;

   // Clear any prior glb selection up front; the glb branch below re-arms it.
   // INSPECTOR_RML polls IsGlb ()/GlbData () each frame to drive the live
   // PREVIEW3D window, so a non-glb selection must reset this state.
   m_bIsGlb = false;
   m_aGlbData.clear ();

   std::string sContentType = pFile->ContentType ();
   std::string sUrl         = pFile->Url ();

   if (IsImageType (sContentType, sUrl))
   {
      std::string sPath = pFile->DiskPath ();

      for (char& c : sPath)
      {
         if (c == '\\')
            c = '/';
      }

      if (sPath.empty ())
      {
         pBody->SetInnerRML ("No content available");
      }
      else
      {
         // RmlUi's SystemInterface::JoinPath strips one leading '/' from absolute
         // POSIX paths (it assumes root-relative web paths). DiskPath () is a real
         // absolute filesystem path, so prepend an extra '/' to survive the strip
         // and reach fopen () intact. Windows drive paths (C:/...) are passed
         // through untouched by JoinPath, so leave those alone.
         if (sPath[0] == '/')
            sPath = "/" + sPath;

         pBody->SetInnerRML ("<img class=\"preview-image\" src=\"" + sPath + "\"/>");
      }
   }
   else if (IsMsfType (sContentType, sUrl))
   {
      ShowMsf (pBody, pFile);
   }
   else if (sContentType.compare ("application/jose+json") == 0)
   {
      ShowMsfJSON (pBody, pFile);
   }
   else if (sContentType.compare ("application/json") == 0)
   {
      ShowJSON (pBody, pFile);
   }
   else
   {
      std::vector<uint8_t> aData;
      pFile->ReadData (aData);

      if (aData.empty ())
      {
         pBody->SetInnerRML ("No content available");
      }
      else if (IsGltfType (sContentType, sUrl, aData))
      {
         // Hand the bytes to INSPECTOR_RML, which docks a live PREVIEW3D window
         // over #preview-body and renders the model through Halogen's native
         // surface. The text under the window is a fallback if that fails.
         m_bIsGlb = true;
         m_aGlbData.swap (aData);
         m_nGlbGen++;

         pBody->SetInnerRML ("<div id=\"preview-3d-host\" class=\"detail-preview-3d\">Rendering 3D preview&#8230;</div>");
      }
      else
      {
         size_t nLimit = (aData.size () > 4096) ? 4096 : aData.size ();
         std::string sText (aData.begin (), aData.begin () + nLimit);

         std::string sEscaped = UTILS::Escape (sText, false);

         if (aData.size () > 4096)
            sEscaped += "\n\n... truncated at 4 KB ...";

         pBody->SetInnerRML (sEscaped);
      }
   }
}

void IW_NETWORK_DETAIL_PREVIEW::ProcessEvent (Rml::Event& Event)
{
   COLLAPSE::Toggle (Event.GetCurrentElement ());
}

IW_NETWORK_DETAIL_PREVIEW::~IW_NETWORK_DETAIL_PREVIEW ()
{
   COLLAPSE::Detach (this, m_apSections);
}

} // namespace RUBIDIUM
