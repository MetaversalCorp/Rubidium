// Copyright 2026 Metaversal Corporation. All rights reserved.

#ifndef RUBIDIUM_INSPECTOR_MEMORY_H
#define RUBIDIUM_INSPECTOR_MEMORY_H

#include "inspector/InspectorRml.h"

namespace RUBIDIUM
{

class IW_MEMORY : public INSPECTOR_WIDGET
{
public:
   IW_MEMORY () {}

   static INSPECTOR_WIDGET* Create () { return new IW_MEMORY (); }
   const char* Id () override { return "memory"; }
   bool Initialize (Rml::Element* pContainer) override;
};

} // namespace RUBIDIUM

#endif // RUBIDIUM_INSPECTOR_MEMORY_H
