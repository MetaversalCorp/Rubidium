// Copyright 2026 Metaversal Corporation. All rights reserved.

#ifndef RUBIDIUM_INSPECTOR_INSPECTORRML_H
#define RUBIDIUM_INSPECTOR_INSPECTORRML_H

namespace Rml { class Element; }

namespace RUBIDIUM
{
   class INSPECTOR_WIDGET;
   typedef INSPECTOR_WIDGET* (*FN_CREATEWIDGET) ();
}

namespace RUBIDIUM
{
   class INSPECTOR_WIDGET;

   class INSPECTOR_WIDGET_DATA
   {
   public:
      virtual ~INSPECTOR_WIDGET_DATA () {};

      virtual INSPECTOR_WIDGET* CreateWidget () = 0;
   };

   class INSPECTOR_WIDGET
   {
   public:
      virtual ~INSPECTOR_WIDGET ();

      virtual const char* Id         ()                         = 0;
      virtual bool        Initialize (Rml::Element* pContainer) = 0;

      Rml::Element*     Container ()            const { return m_pContainer; }
      Rml::Element*     Container (int nWidget) const { return m_apChildren[nWidget]->Container (); }
      INSPECTOR_WIDGET* Widget    (int nIndex)  const { return m_apChildren[nIndex]; }

      static INSPECTOR_WIDGET* Create () { return nullptr; }

   protected:
      Rml::Element*                  m_pContainer = nullptr;
      std::vector<INSPECTOR_WIDGET*> m_apChildren;

      bool CreateWidgets (const FN_CREATEWIDGET* afnCreateWidget, int nCount);

      bool AddWidget (INSPECTOR_WIDGET* pWidget);

      template <typename T>
      bool CreateWidgetsEx (T* apWidget, int nCount)
      {
         bool bResult = true;

         for (int nWidget = 0; nWidget < nCount  &&  bResult; nWidget++)
            bResult = AddWidget (apWidget[nWidget].CreateWidget ());

         return bResult;
      }
   };

   class INSPECTOR_RML
   {
   public:
      INSPECTOR_RML ();
      ~INSPECTOR_RML ();

      bool Initialize ();

      void Render ();
      void Toggle ();

      bool IsVisible () const;

      void SetContext (SNEEZE::CONTEXT* pContext);
      void SetScene   (SNEEZE::SCENE* pScene);
      void SetEngine  (SNEEZE::ENGINE* pEngine);
      void Reset ();

      void OnNetworkCacheCreated (SNEEZE::CACHE* pCache);
      void OnNetworkCacheDeleted (SNEEZE::CACHE* pCache);

      bool OnNetworkFileCreated (SNEEZE::FILE* pFile);
      void OnNetworkFileChanged (SNEEZE::FILE* pFile);
      void OnNetworkFileDeleted (SNEEZE::FILE* pFile);

      void OnConsoleEntryCreated (std::shared_ptr<const SNEEZE::ENTRY> pEntryPtr);
      void OnConsoleEntryDeleted (std::shared_ptr<const SNEEZE::ENTRY> pEntryPtr);

      void OnStorageSiloCreated (SNEEZE::SILO* pSilo);
      void OnStorageSiloDeleted (SNEEZE::SILO* pSilo);
      void OnStorageUnitChanged (SNEEZE::SILO* pSilo, SNEEZE::eSILO_SCOPE eScope);

      void ProcessPendingFiles ();

   private:
      class Impl;
      Impl* m_pImpl;
   };
} // namespace RUBIDIUM

#endif // RUBIDIUM_INSPECTOR_INSPECTORRML_H
