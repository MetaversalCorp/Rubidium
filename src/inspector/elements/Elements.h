// Copyright 2026 Metaversal Corporation. All rights reserved.

#ifndef RUBIDIUM_INSPECTOR_ELEMENTS_H
#define RUBIDIUM_INSPECTOR_ELEMENTS_H

#include "inspector/InspectorRml.h"

#include <set>

namespace SNEEZE
{
   class SCENE;
   class FABRIC;
   class NODE;
}

namespace RUBIDIUM
{

/*******************************************************************************************************************************
**                                               Detail Sub-Tab Components                                                    **
*******************************************************************************************************************************/

class IW_ELEMENTS_DETAIL_COMPUTED : public INSPECTOR_WIDGET, public Rml::EventListener
{
public:
   IW_ELEMENTS_DETAIL_COMPUTED ();
   ~IW_ELEMENTS_DETAIL_COMPUTED () override;

   static INSPECTOR_WIDGET* Create () { return new IW_ELEMENTS_DETAIL_COMPUTED (); }
   const char* Id () override { return "elements-detail-computed"; }
   bool Initialize (Rml::Element* pContainer) override;

   // Show the selected node's position resolved in its parent's frame.
   void ShowNode (SNEEZE::NODE* pNode);

   void ProcessEvent (Rml::Event& Event) override;

private:
   std::vector<Rml::Element*> m_apSections;
};

class IW_ELEMENTS_DETAIL_DETAILS : public INSPECTOR_WIDGET, public Rml::EventListener
{
public:
   IW_ELEMENTS_DETAIL_DETAILS ();
   ~IW_ELEMENTS_DETAIL_DETAILS () override;

   static INSPECTOR_WIDGET* Create () { return new IW_ELEMENTS_DETAIL_DETAILS (); }
   const char* Id () override { return "elements-detail-details"; }
   bool Initialize (Rml::Element* pContainer) override;

   // Populate every MAP_OBJECT field for the selected node (clears when null).
   void ShowNode (SNEEZE::NODE* pNode);

   void ProcessEvent (Rml::Event& Event) override;

private:
   std::vector<Rml::Element*> m_apSections;
};

class IW_ELEMENTS_DETAIL_EVENTS : public INSPECTOR_WIDGET
{
public:
   IW_ELEMENTS_DETAIL_EVENTS ();
   ~IW_ELEMENTS_DETAIL_EVENTS () override;

   static INSPECTOR_WIDGET* Create () { return new IW_ELEMENTS_DETAIL_EVENTS (); }
   const char* Id () override { return "elements-detail-events"; }
   bool Initialize (Rml::Element* pContainer) override;
};

/*******************************************************************************************************************************
**                                                     Details Pane                                                           **
*******************************************************************************************************************************/

class IW_ELEMENTS_DETAILS : public INSPECTOR_WIDGET, public Rml::EventListener
{
public:
   IW_ELEMENTS_DETAILS ();
   ~IW_ELEMENTS_DETAILS () override;

   static INSPECTOR_WIDGET* Create () { return new IW_ELEMENTS_DETAILS (); }
   const char* Id () override { return "elements-details"; }
   bool Initialize (Rml::Element* pContainer) override;

   // Forwards the selected node to the Details sub-tab.
   void ShowNode (SNEEZE::NODE* pNode);

   void ProcessEvent (Rml::Event& Event) override;

   enum eWIDGET
   {
      kWIDGET_COMPUTED = 0,
      kWIDGET_DETAILS  = 1,
      kWIDGET_EVENTS   = 2,
      kWIDGET_COUNT    = 3
   };

private:
   static const FN_CREATEWIDGET s_afnCreateWidget[kWIDGET_COUNT];

   Rml::Element* m_pTabbar;
   Rml::Element* m_apTabs[kWIDGET_COUNT];
   Rml::Element* m_apPanels[kWIDGET_COUNT];
   int           m_nActiveTab;
};

/*******************************************************************************************************************************
**                                                   Container Sidebar                                                        **
*******************************************************************************************************************************/

class IW_ELEMENTS_TREE;

class IW_ELEMENTS_CONTAINERS : public INSPECTOR_WIDGET, public Rml::EventListener
{
public:
   IW_ELEMENTS_CONTAINERS ();
   ~IW_ELEMENTS_CONTAINERS () override;

   static INSPECTOR_WIDGET* Create () { return new IW_ELEMENTS_CONTAINERS (); }
   const char* Id () override { return "elements-containers"; }
   bool Initialize (Rml::Element* pContainer) override;

   void SetTree (IW_ELEMENTS_TREE* pTree);

   void AddContainer (const std::string& sName);
   void Clear ();

   const std::string& SelectedContainer () const;

   void ProcessEvent (Rml::Event& Event) override;

private:
   void UpdateSelection (Rml::Element* pItem);

   Rml::Element*            m_pItems;
   Rml::Element*            m_pItemAll;
   IW_ELEMENTS_TREE*        m_pTree;
   std::vector<std::string> m_asContainers;
   std::string              m_sSelected;
};

/*******************************************************************************************************************************
**                                                     Element Tree                                                           **
*******************************************************************************************************************************/

class IW_ELEMENTS_TREE : public INSPECTOR_WIDGET, public Rml::EventListener
{
public:
   IW_ELEMENTS_TREE ();
   ~IW_ELEMENTS_TREE () override;

   static INSPECTOR_WIDGET* Create () { return new IW_ELEMENTS_TREE (); }
   const char* Id () override { return "elements-tree"; }
   bool Initialize (Rml::Element* pContainer) override;

   void ShowSearch ();
   void HideSearch ();
   bool IsSearchVisible () const;

   // The Details pane receives the node selected by clicking a tree row.
   void SetDetails (IW_ELEMENTS_DETAILS* pDetails);

   // Fabric/Node tree. Rebuild () returns true when the tree markup changed.
   void SetScene (SNEEZE::SCENE* pScene);
   bool Rebuild  ();

   // Origin filter. Empty string ("(all)") shows every fabric and its nodes.
   // A non-empty origin shows that fabric's nodes; all other fabrics render as
   // dimmed (pink) rows with their nodes hidden.
   void SetOriginFilter (const std::string& sOrigin);

   void ProcessEvent (Rml::Event& Event) override;

private:
   void BuildFabric (std::string& sRml, SNEEZE::FABRIC* pFabric, int nDepth);
   void BuildNode   (std::string& sRml, SNEEZE::NODE*   pNode,   int nDepth, bool bShowNodes);

   // True when the fabric belongs to the active origin filter (always true when
   // the filter is empty -- the "(all)" case).
   bool FabricMatchesFilter (SNEEZE::FABRIC* pFabric) const;

   // Disclosure arrow for a fabric/node row. Expandable rows register pKey in
   // m_apToggleKey and carry id "eltree-tog-N"; leaves emit an empty spacer so
   // labels stay aligned. bCollapsed rotates the arrow to the closed state.
   std::string BuildToggle (const void* pKey, bool bExpandable, bool bCollapsed);

   bool        IsCollapsed (const void* pKey) const;

   // Re-applies the "selected" highlight to the row of m_pSelectedNode after a
   // rebuild replaces the row DOM. Sets m_pSelectedRow to the resolved element.
   void ApplySelectionHighlight ();

   Rml::Element*        m_pContent;
   Rml::Element*        m_pRows;
   Rml::Element*        m_pPathBar;
   Rml::Element*        m_pSearchBar;
   Rml::Element*        m_pSearchInput;
   Rml::Element*        m_pSearchClose;
   bool                 m_bSearchVisible;
   SNEEZE::SCENE*       m_pScene;
   std::string          m_sLastRml;
   std::string          m_sOriginFilter;   // empty == "(all)"

   IW_ELEMENTS_DETAILS*       m_pDetails;
   std::vector<SNEEZE::NODE*> m_apRowNode;       // index N <-> row id "eltree-row-N"
   SNEEZE::NODE*              m_pSelectedNode;
   Rml::Element*             m_pSelectedRow;

   std::set<const void*>      m_setCollapsed;    // collapsed fabric/node pointers
   std::vector<const void*>   m_apToggleKey;     // index N <-> toggle id "eltree-tog-N"
};

/*******************************************************************************************************************************
**                                                    Elements Frame                                                          **
*******************************************************************************************************************************/

class IW_ELEMENTS : public INSPECTOR_WIDGET
{
public:
   IW_ELEMENTS ();
   ~IW_ELEMENTS () override;

   static INSPECTOR_WIDGET* Create () { return new IW_ELEMENTS (); }
   const char* Id () override { return "elements"; }
   bool Initialize (Rml::Element* pContainer) override;

   IW_ELEMENTS_CONTAINERS* Containers () const;
   IW_ELEMENTS_TREE*       Tree ()       const;
   IW_ELEMENTS_DETAILS*    Details ()    const;

   enum eWIDGET
   {
      kWIDGET_CONTAINERS = 0,
      kWIDGET_TREE       = 1,
      kWIDGET_DETAILS    = 2,
      kWIDGET_COUNT      = 3
   };

private:
   static const char*           s_sRml;
   static const FN_CREATEWIDGET s_afnCreateWidget[kWIDGET_COUNT];
};

} // namespace RUBIDIUM

#endif // RUBIDIUM_INSPECTOR_ELEMENTS_H
