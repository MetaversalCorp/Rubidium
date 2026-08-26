// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "inspector/elements/Elements.h"
#include "inspector/common/Collapse.h"

using namespace RUBIDIUM;

// One collapsible "detail-section" per MAP_OBJECT sub-struct. Reserved padding
// arrays (Type.abReserved, Bound.abReserved, Properties.abReserved) are intentionally
// omitted -- they carry no field meaning.
static const char* s_sRml =
R"rml(
<div class="detail-section"><span class="section-arrow">&#xE5C5;</span> Identity</div>
<div class="detail-section-body">
   <div class="detail-kv"><div class="detail-key">Class:</div><div class="detail-value" id="eld-class"></div></div>
   <div class="detail-kv"><div class="detail-key">Type:</div><div class="detail-value" id="eld-type"></div></div>
   <div class="detail-kv"><div class="detail-key">Subtype:</div><div class="detail-value" id="eld-subtype"></div></div>
</div>
<div class="detail-section"><span class="section-arrow">&#xE5C5;</span> Head</div>
<div class="detail-section-body">
   <div class="detail-kv"><div class="detail-key">Parent:</div><div class="detail-value" id="eld-head-parent"></div></div>
   <div class="detail-kv"><div class="detail-key">Self:</div><div class="detail-value" id="eld-head-self"></div></div>
</div>
<div class="detail-section"><span class="section-arrow">&#xE5C5;</span> Name</div>
<div class="detail-section-body">
   <div class="detail-kv"><div class="detail-key">Name:</div><div class="detail-value" id="eld-name"></div></div>
</div>
<div class="detail-section"><span class="section-arrow">&#xE5C5;</span> Type</div>
<div class="detail-section-body">
   <div class="detail-kv"><div class="detail-key">bType:</div><div class="detail-value" id="eld-type-btype"></div></div>
   <div class="detail-kv"><div class="detail-key">bSubtype:</div><div class="detail-value" id="eld-type-bsubtype"></div></div>
   <div class="detail-kv"><div class="detail-key">bFiction:</div><div class="detail-value" id="eld-type-bfiction"></div></div>
</div>
<div class="detail-section"><span class="section-arrow">&#xE5C5;</span> Owner</div>
<div class="detail-section-body">
   <div class="detail-kv"><div class="detail-key">twOwner:</div><div class="detail-value" id="eld-owner"></div></div>
</div>
<div class="detail-section"><span class="section-arrow">&#xE5C5;</span> Resource</div>
<div class="detail-section-body">
   <div class="detail-kv"><div class="detail-key">qwResource:</div><div class="detail-value" id="eld-res-resource"></div></div>
   <div class="detail-kv"><div class="detail-key">Name:</div><div class="detail-value" id="eld-res-name"></div></div>
   <div class="detail-kv"><div class="detail-key">Reference:</div><div class="detail-value" id="eld-res-reference"></div></div>
</div>
<div class="detail-section"><span class="section-arrow">&#xE5C5;</span> Transform</div>
<div class="detail-section-body">
   <div class="detail-kv"><div class="detail-key">Position (x,y,z):</div><div class="detail-value" id="eld-xf-position"></div></div>
   <div class="detail-kv"><div class="detail-key">Rotation (x,y,z,w):</div><div class="detail-value" id="eld-xf-rotation"></div></div>
   <div class="detail-kv"><div class="detail-key">Scale (x,y,z):</div><div class="detail-value" id="eld-xf-scale"></div></div>
</div>
<div class="detail-section"><span class="section-arrow">&#xE5C5;</span> Orbit</div>
<div class="detail-section-body">
   <div class="detail-kv"><div class="detail-key">tmPeriod:</div><div class="detail-value" id="eld-orbit-period"></div></div>
   <div class="detail-kv"><div class="detail-key">tmOrigin:</div><div class="detail-value" id="eld-orbit-origin"></div></div>
   <div class="detail-kv"><div class="detail-key">dA:</div><div class="detail-value" id="eld-orbit-a"></div></div>
   <div class="detail-kv"><div class="detail-key">dB:</div><div class="detail-value" id="eld-orbit-b"></div></div>
</div>
<div class="detail-section"><span class="section-arrow">&#xE5C5;</span> Bound</div>
<div class="detail-section-body">
   <div class="detail-kv"><div class="detail-key">Max (x,y,z):</div><div class="detail-value" id="eld-bound-max"></div></div>
</div>
<div class="detail-section"><span class="section-arrow">&#xE5C5;</span> Properties</div>
<div class="detail-section-body">
   <div class="detail-kv"><div class="detail-key">fMass:</div><div class="detail-value" id="eld-prop-mass"></div></div>
   <div class="detail-kv"><div class="detail-key">fGravity:</div><div class="detail-value" id="eld-prop-gravity"></div></div>
   <div class="detail-kv"><div class="detail-key">fColor:</div><div class="detail-value" id="eld-prop-color"></div></div>
   <div class="detail-kv"><div class="detail-key">fBrightness:</div><div class="detail-value" id="eld-prop-brightness"></div></div>
   <div class="detail-kv"><div class="detail-key">fReflectivity:</div><div class="detail-value" id="eld-prop-reflectivity"></div></div>
</div>
)rml";

static std::string FormatDouble (double dValue)
{
   char acBuffer[64];

   std::snprintf (acBuffer, sizeof (acBuffer), "%g", dValue);

   return std::string (acBuffer);
}

static std::string FormatFloat (float fValue)
{
   char acBuffer[64];

   std::snprintf (acBuffer, sizeof (acBuffer), "%g", static_cast<double> (fValue));

   return std::string (acBuffer);
}

static std::string FormatDouble3 (const double* adValue)
{
   return FormatDouble (adValue[0]) + ", " + FormatDouble (adValue[1]) + ", " + FormatDouble (adValue[2]);
}

static std::string FormatDouble4 (const double* adValue)
{
   return FormatDouble (adValue[0]) + ", " + FormatDouble (adValue[1]) + ", " + FormatDouble (adValue[2]) + ", " + FormatDouble (adValue[3]);
}

// A fixed-width char field is not guaranteed null-terminated when full.
static std::string FromFixedChars (const char* sField, size_t nMax)
{
   return std::string (sField, ::strnlen (sField, nMax));
}

// MAP_OBJECT_NAME stores the name as UTF-16; convert to UTF-8 for display.
static std::string DecodeName (const uint16_t* pwsName, size_t nMax)
{
   std::string sResult;

   for (size_t n = 0; n < nMax; n++)
   {
      uint16_t wc = pwsName[n];

      if (wc == 0)
         break;

      if (wc < 0x80)
      {
         sResult += static_cast<char> (wc);
      }
      else if (wc < 0x800)
      {
         sResult += static_cast<char> (0xC0 | (wc >> 6));
         sResult += static_cast<char> (0x80 | (wc & 0x3F));
      }
      else
      {
         sResult += static_cast<char> (0xE0 | (wc >> 12));
         sResult += static_cast<char> (0x80 | ((wc >> 6) & 0x3F));
         sResult += static_cast<char> (0x80 | (wc & 0x3F));
      }
   }

   return sResult;
}

IW_ELEMENTS_DETAIL_DETAILS::IW_ELEMENTS_DETAIL_DETAILS ()
{
}

IW_ELEMENTS_DETAIL_DETAILS::~IW_ELEMENTS_DETAIL_DETAILS ()
{
   COLLAPSE::Detach (this, m_apSections);
}

bool IW_ELEMENTS_DETAIL_DETAILS::Initialize (Rml::Element* pContainer)
{
   m_pContainer = pContainer;

   m_pContainer->SetInnerRML (s_sRml);

   COLLAPSE::Attach (m_pContainer, this, m_apSections);

   return true;
}

void IW_ELEMENTS_DETAIL_DETAILS::ShowNode (SNEEZE::NODE* pNode)
{
   if (m_pContainer)
   {
      const std::string sEmpty = "&#8212;";   // em dash

      auto Set = [this, &sEmpty] (const char* sId, const std::string& sValue)
      {
         if (Rml::Element* pEl = m_pContainer->GetElementById (sId))
            pEl->SetInnerRML (sValue.empty () ? sEmpty : UTILS::Escape (sValue));
      };

      // Identity always comes from the NODE -- valid even when no MAP_OBJECT is attached.
      Set ("eld-class",   pNode ? pNode->ClassName () : std::string ());
      Set ("eld-type",    pNode ? pNode->TypeName ()  : std::string ());
      Set ("eld-subtype", pNode ? std::to_string (pNode->Subtype ()) : std::string ());

      RMAP::MAP::MAP_OBJECT* pMap = pNode ? pNode->Map_Object () : nullptr;

      if (pMap)
      {
         RMAP::MAP::MAP_OBJECT_POD Pod;

         pMap->GetPOD (Pod);

         SNEEZE::NODE* pNode_Parent = pNode->Parent ();
         Set ("eld-head-parent", pNode_Parent ? std::to_string (pNode_Parent->ObjectIx ()) : std::string ());
         Set ("eld-head-self",   std::to_string (pNode->ObjectIx ()));

         Set ("eld-name", DecodeName (Pod.Name.wsName, 48));

         Set ("eld-type-btype",    std::to_string (Pod.Type.bType));
         Set ("eld-type-bsubtype", std::to_string (Pod.Type.bSubtype));
         Set ("eld-type-bfiction", std::to_string (Pod.Type.bFiction));

         Set ("eld-owner", std::to_string (Pod.Owner.twOwner));

         Set ("eld-res-resource",  std::to_string (Pod.Resource.qwResource));
         Set ("eld-res-name",      FromFixedChars (Pod.Resource.sName,      sizeof (Pod.Resource.sName)));
         Set ("eld-res-reference", FromFixedChars (Pod.Resource.sReference, sizeof (Pod.Resource.sReference)));

         Set ("eld-xf-position", FormatDouble3 (Pod.Transform.d3Position));
         Set ("eld-xf-rotation", FormatDouble4 (Pod.Transform.d4Rotation));
         Set ("eld-xf-scale",    FormatDouble3 (Pod.Transform.d3Scale));

         Set ("eld-orbit-period", std::to_string (Pod.Orbit.Celestial.tmPeriod));
         Set ("eld-orbit-origin", std::to_string (Pod.Orbit.Celestial.tmOrigin));
         Set ("eld-orbit-a",      FormatDouble (Pod.Orbit.Celestial.dA));
         Set ("eld-orbit-b",      FormatDouble (Pod.Orbit.Celestial.dB));

         Set ("eld-bound-max", FormatDouble3 (Pod.Bound.d3Max));

         Set ("eld-prop-mass",         FormatFloat (Pod.Properties.Celestial.fMass));
         Set ("eld-prop-gravity",      FormatFloat (Pod.Properties.Celestial.fGravity));
         Set ("eld-prop-color",        FormatFloat (Pod.Properties.Celestial.fColor));
         Set ("eld-prop-brightness",   FormatFloat (Pod.Properties.Celestial.fBrightness));
         Set ("eld-prop-reflectivity", FormatFloat (Pod.Properties.Celestial.fReflectivity));
      }
      else
      {
         // No map object attached -- blank every struct field.
         static const char* s_asEmptyIds[] =
         {
            "eld-head-parent", "eld-head-self", "eld-name",
            "eld-type-btype", "eld-type-bsubtype", "eld-type-bfiction", "eld-owner",
            "eld-res-resource", "eld-res-name", "eld-res-reference",
            "eld-xf-position", "eld-xf-rotation", "eld-xf-scale",
            "eld-orbit-period", "eld-orbit-origin", "eld-orbit-a", "eld-orbit-b",
            "eld-bound-max",
            "eld-prop-mass", "eld-prop-gravity", "eld-prop-color", "eld-prop-brightness", "eld-prop-reflectivity",
         };

         for (const char* sId : s_asEmptyIds)
            Set (sId, std::string ());
      }
   }
}

void IW_ELEMENTS_DETAIL_DETAILS::ProcessEvent (Rml::Event& Event)
{
   Rml::Element* pHeader = Event.GetCurrentElement ();

   if (COLLAPSE::Toggle (pHeader))
   {
      Rml::Element* pArrow = pHeader->GetFirstChild ();

      if (pArrow)
         pArrow->SetInnerRML (pHeader->IsClassSet ("collapsed") ? "&#xE5C6;" : "&#xE5C5;");
   }
}
