// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/elements/Elements.h"
#include "inspector/common/Collapse.h"

using namespace RUBIDIUM;

static const char* s_sRml =
R"rml(
<div class="detail-section"><span class="section-arrow">&#xE5C5;</span> Position (relative to parent)</div>
<div class="detail-section-body">
   <div class="detail-kv"><div class="detail-key">Parent:</div><div class="detail-value" id="eldc-parent"></div></div>
   <div class="detail-kv"><div class="detail-key">X:</div><div class="detail-value" id="eldc-x"></div></div>
   <div class="detail-kv"><div class="detail-key">Y:</div><div class="detail-value" id="eldc-y"></div></div>
   <div class="detail-kv"><div class="detail-key">Z:</div><div class="detail-value" id="eldc-z"></div></div>
</div>
<div class="detail-section"><span class="section-arrow">&#xE5C5;</span> Rotation (relative to parent)</div>
<div class="detail-section-body">
   <div class="detail-kv"><div class="detail-key">Quaternion X:</div><div class="detail-value" id="eldc-qx"></div></div>
   <div class="detail-kv"><div class="detail-key">Quaternion Y:</div><div class="detail-value" id="eldc-qy"></div></div>
   <div class="detail-kv"><div class="detail-key">Quaternion Z:</div><div class="detail-value" id="eldc-qz"></div></div>
   <div class="detail-kv"><div class="detail-key">Quaternion W:</div><div class="detail-value" id="eldc-qw"></div></div>
   <div class="detail-kv"><div class="detail-key">Euler Roll (X):</div><div class="detail-value" id="eldc-roll"></div></div>
   <div class="detail-kv"><div class="detail-key">Euler Pitch (Y):</div><div class="detail-value" id="eldc-pitch"></div></div>
   <div class="detail-kv"><div class="detail-key">Euler Yaw (Z):</div><div class="detail-value" id="eldc-yaw"></div></div>
</div>
)rml";

static const double kPI = 3.14159265358979323846;

static std::string FormatDouble (double dValue)
{
   char acBuffer[64];

   std::snprintf (acBuffer, sizeof (acBuffer), "%g", dValue);

   return std::string (acBuffer);
}

static std::string FormatDegrees (double dRadians)
{
   char acBuffer[64];

   std::snprintf (acBuffer, sizeof (acBuffer), "%g\xC2\xB0", dRadians * 180.0 / kPI);   // UTF-8 degree sign

   return std::string (acBuffer);
}

IW_ELEMENTS_DETAIL_COMPUTED::IW_ELEMENTS_DETAIL_COMPUTED ()
{
}

IW_ELEMENTS_DETAIL_COMPUTED::~IW_ELEMENTS_DETAIL_COMPUTED ()
{
   COLLAPSE::Detach (this, m_apSections);
}

bool IW_ELEMENTS_DETAIL_COMPUTED::Initialize (Rml::Element* pContainer)
{
   m_pContainer = pContainer;

   m_pContainer->SetInnerRML (s_sRml);

   COLLAPSE::Attach (m_pContainer, this, m_apSections);

   return true;
}

void IW_ELEMENTS_DETAIL_COMPUTED::ShowNode (SNEEZE::NODE* pNode)
{
   if (m_pContainer)
   {
      const std::string sEmpty = "&#8212;";   // em dash

      auto Set = [this, &sEmpty] (const char* sId, const std::string& sValue)
      {
         if (Rml::Element* pEl = m_pContainer->GetElementById (sId))
            pEl->SetInnerRML (sValue.empty () ? sEmpty : UTILS::Escape (sValue));
      };

      SNEEZE::NODE* pParent = pNode ? pNode->Parent () : nullptr;

      if (pParent)
      {
         std::string sName = pParent->Name ();
         Set ("eldc-parent", sName.empty () ? std::string ("(unnamed)") : sName);
      }
      else
      {
         Set ("eldc-parent", std::string ("(root)"));
      }

      RMAP::MAP::MAP_OBJECT* pMap = pNode ? pNode->Map_Object () : nullptr;

      if (pMap)
      {
         // Position() resolves the object in its parent's frame: the live orbital
         // position for celestial bodies, the static transform for everything else.
         // tmNow comes from the owning viewport's current simulation tick.
         int64_t tmNow = 0;

         if (SNEEZE::FABRIC* pFabric = pNode->Fabric ())
         {
            if (SNEEZE::SCENE* pScene = pFabric->Scene ())
            {
               if (SNEEZE::CONTEXT* pContext = pScene->Context ())
               {
                  if (SNEEZE::VIEWPORT* pViewport = pContext->Viewport ())
                     tmNow = pViewport->m_tmNow;
               }
            }
         }

         double dX = 0.0;
         double dY = 0.0;
         double dZ = 0.0;

         pMap->Position (tmNow, dX, dY, dZ);

         Set ("eldc-x", FormatDouble (dX));
         Set ("eldc-y", FormatDouble (dY));
         Set ("eldc-z", FormatDouble (dZ));

         double dQx = 0.0;
         double dQy = 0.0;
         double dQz = 0.0;
         double dQw = 1.0;

         pMap->Rotation (tmNow, dQx, dQy, dQz, dQw);

         Set ("eldc-qx", FormatDouble (dQx));
         Set ("eldc-qy", FormatDouble (dQy));
         Set ("eldc-qz", FormatDouble (dQz));
         Set ("eldc-qw", FormatDouble (dQw));

         // Tait-Bryan ZYX (roll=X, pitch=Y, yaw=Z), output in degrees.
         double dSinRoll  = 2.0 * (dQw * dQx + dQy * dQz);
         double dCosRoll  = 1.0 - 2.0 * (dQx * dQx + dQy * dQy);
         double dRoll     = std::atan2 (dSinRoll, dCosRoll);

         double dSinPitch = 2.0 * (dQw * dQy - dQz * dQx);
         double dPitch    = (std::fabs (dSinPitch) >= 1.0) ? std::copysign (kPI / 2.0, dSinPitch) : std::asin (dSinPitch);

         double dSinYaw   = 2.0 * (dQw * dQz + dQx * dQy);
         double dCosYaw   = 1.0 - 2.0 * (dQy * dQy + dQz * dQz);
         double dYaw      = std::atan2 (dSinYaw, dCosYaw);

         Set ("eldc-roll",  FormatDegrees (dRoll));
         Set ("eldc-pitch", FormatDegrees (dPitch));
         Set ("eldc-yaw",   FormatDegrees (dYaw));
      }
      else
      {
         Set ("eldc-x", std::string ());
         Set ("eldc-y", std::string ());
         Set ("eldc-z", std::string ());

         Set ("eldc-qx", std::string ());
         Set ("eldc-qy", std::string ());
         Set ("eldc-qz", std::string ());
         Set ("eldc-qw", std::string ());

         Set ("eldc-roll",  std::string ());
         Set ("eldc-pitch", std::string ());
         Set ("eldc-yaw",   std::string ());
      }
   }
}

void IW_ELEMENTS_DETAIL_COMPUTED::ProcessEvent (Rml::Event& Event)
{
   Rml::Element* pHeader = Event.GetCurrentElement ();

   if (COLLAPSE::Toggle (pHeader))
   {
      Rml::Element* pArrow = pHeader->GetFirstChild ();

      if (pArrow)
         pArrow->SetInnerRML (pHeader->IsClassSet ("collapsed") ? "&#xE5C6;" : "&#xE5C5;");
   }
}
