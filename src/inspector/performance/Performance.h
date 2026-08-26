// Copyright 2026 Metaversal Corporation. All rights reserved.

#ifndef RUBIDIUM_INSPECTOR_PERFORMANCE_H
#define RUBIDIUM_INSPECTOR_PERFORMANCE_H

#include "inspector/InspectorRml.h"

namespace RUBIDIUM
{

/*******************************************************************************************************************************
**                                                       Toolbar                                                              **
*******************************************************************************************************************************/

class IW_BUTTON;

class IW_PERFORMANCE_TOOLBAR : public INSPECTOR_WIDGET
{
public:
   IW_PERFORMANCE_TOOLBAR ();
   ~IW_PERFORMANCE_TOOLBAR () override;

   static INSPECTOR_WIDGET* Create () { return new IW_PERFORMANCE_TOOLBAR (); }
   const char* Id () override { return "perf-toolbar"; }
   bool Initialize (Rml::Element* pContainer) override;

   enum eBUTTON
   {
      kBUTTON_RECORD,
      kBUTTON_CLEAR,

      kBUTTON_COUNT
   };

   IW_BUTTON* Button (int nIndex) const;
};

/*******************************************************************************************************************************
**                                                    Metrics Summary                                                         **
*******************************************************************************************************************************/

class IW_PERFORMANCE_METRICS : public INSPECTOR_WIDGET
{
public:
   IW_PERFORMANCE_METRICS ();
   ~IW_PERFORMANCE_METRICS () override;

   static INSPECTOR_WIDGET* Create () { return new IW_PERFORMANCE_METRICS (); }
   const char* Id () override { return "perf-metrics"; }
   bool Initialize (Rml::Element* pContainer) override;

   void Update (double dFps, double dFrame, double dSubmit, double dRender);

private:
   Rml::Element* m_pFps;
   Rml::Element* m_pFrame;
   Rml::Element* m_pSubmit;
   Rml::Element* m_pRender;
};

/*******************************************************************************************************************************
**                                                      Chart Area                                                            **
*******************************************************************************************************************************/

class IW_PERFORMANCE_CHART : public INSPECTOR_WIDGET
{
public:
   IW_PERFORMANCE_CHART ();
   ~IW_PERFORMANCE_CHART () override;

   static INSPECTOR_WIDGET* Create () { return new IW_PERFORMANCE_CHART (); }
   const char* Id () override { return "perf-chart"; }
   bool Initialize (Rml::Element* pContainer) override;
};

/*******************************************************************************************************************************
**                                                       Log Area                                                             **
*******************************************************************************************************************************/

class IW_PERFORMANCE_LOG : public INSPECTOR_WIDGET
{
public:
   IW_PERFORMANCE_LOG ();
   ~IW_PERFORMANCE_LOG () override;

   static INSPECTOR_WIDGET* Create () { return new IW_PERFORMANCE_LOG (); }
   const char* Id () override { return "perf-log"; }
   bool Initialize (Rml::Element* pContainer) override;

   void AppendLine (const char* sLine);
   void Clear ();

private:
   Rml::Element* m_pLines;
};

/*******************************************************************************************************************************
**                                                  Performance Frame                                                         **
*******************************************************************************************************************************/

class IW_PERFORMANCE : public INSPECTOR_WIDGET
{
public:
   IW_PERFORMANCE ();
   ~IW_PERFORMANCE () override;

   static INSPECTOR_WIDGET* Create () { return new IW_PERFORMANCE (); }
   const char* Id () override { return "performance"; }
   bool Initialize (Rml::Element* pContainer) override;

   IW_PERFORMANCE_TOOLBAR* Toolbar () const;
   IW_PERFORMANCE_METRICS* Metrics () const;
   IW_PERFORMANCE_CHART*   Chart ()   const;
   IW_PERFORMANCE_LOG*     Log ()     const;

   enum eWIDGET
   {
      kWIDGET_TOOLBAR = 0,
      kWIDGET_METRICS = 1,
      kWIDGET_CHART   = 2,
      kWIDGET_LOG     = 3,
      kWIDGET_COUNT   = 4
   };

private:
   static const char*           s_sRml;
   static const FN_CREATEWIDGET s_afnCreateWidget[kWIDGET_COUNT];
};

} // namespace RUBIDIUM

#endif // RUBIDIUM_INSPECTOR_PERFORMANCE_H
