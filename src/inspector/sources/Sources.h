// Copyright 2026 Metaversal Corporation. All rights reserved.

#ifndef RUBIDIUM_INSPECTOR_SOURCES_H
#define RUBIDIUM_INSPECTOR_SOURCES_H

#include "inspector/InspectorRml.h"

namespace RUBIDIUM
{

class IW_SOURCES : public INSPECTOR_WIDGET
{
public:
   IW_SOURCES () {}

   static INSPECTOR_WIDGET* Create () { return new IW_SOURCES (); }
   const char* Id () override { return "sources"; }
   bool Initialize (Rml::Element* pContainer) override;
};

} // namespace RUBIDIUM

#endif // RUBIDIUM_INSPECTOR_SOURCES_H
