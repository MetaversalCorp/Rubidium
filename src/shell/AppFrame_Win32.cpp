// Copyright 2026 Metaversal Corporation. All rights reserved.

#include "resource.h"
#include "Brand.h"
#include "logger/ILogger.h"
#include "inspector/InspectorRml.h"
#include "shell/WinUtils.h"
#include "AppFrameTab.h"

using namespace RUBIDIUM;

static constexpr char* g_szLogLevels[LOGGER::kLOGLEVEL_COUNT] = { "trace", "info", "warning", "error", "off" };

static constexpr int IDC_BTN_ELLIPSIS     = 0x0103;
static constexpr int IDC_URL_EDIT         = 0x0104;

static const WCHAR   wszUrlPopupClass[]   = PRODUCT_URL_POPUP_CLASS_W;

// Maximum history rows shown at once in the URL dropdown.
static constexpr int URL_POPUP_MAX_ROWS   = 10;

static constexpr int IDM_LOG_TRACE        = 0x0210;
static constexpr int IDM_LOG_INFO         = 0x0211;
static constexpr int IDM_LOG_WARNING      = 0x0212;
static constexpr int IDM_LOG_ERROR        = 0x0213;
static constexpr int IDM_LOG_OFF          = 0x0214;

// Set this to 0 to remove the fake shadow painting
#define WIN32_FAKE_SHADOW_HEIGHT 1
// The offset of the 2 rectangles of the maximized window button
#define WIN32_MAXIMIZED_RECTANGLE_OFFSET 2

#define TAB_MAX_COUNT                                      24
#define BAR_HEIGHT_DPI96                                   36
#define BAR_MARGIN_X_DPI96                                  8
#define BAR_MARGIN_Y_DPI96                                  6
#define BAR_BTN_SIZE_DPI96                                 24
#define BAR_GAP_DPI96                                       4
#define EDIT_BORDER_DPI96                                  1

static WCHAR wszEllipsis[2]  = { 0x22EE, 0 };

#define WM_URL_SUBMIT                                      (WM_USER + 100)

typedef struct
{
   RECT                                rcClose;
   RECT                                rcMaximize;
   RECT                                rcMinimize;
}
TITLEBAR_RECTS;

static constexpr COLORREF crShadow                          = RGB (100, 100, 100);

static constexpr COLORREF crEditBorder                      = RGB (180, 180, 185);
static constexpr COLORREF crEditBknd                        = RGB (237, 242, 250);
static constexpr COLORREF crToolbarBknd                     = RGB (255, 255, 255);
static constexpr COLORREF crTitlebarBknd                    = RGB (211, 227, 253);
static constexpr COLORREF crTitlebarTabHover                = RGB (170, 199, 255);
static constexpr COLORREF crCanvasBknd                      = RGB (252, 252, 252);
static constexpr COLORREF crTitlebarBtnHover                = RGB (210, 212, 215);
static constexpr COLORREF crTitlebarBtnCloseHover           = RGB (204,   0,   0);
static constexpr COLORREF rcTitlebarItem_On                 = RGB ( 33,  33,  33);
static constexpr COLORREF rcTitlebarItem_Off                = RGB (127, 127, 127);
static constexpr COLORREF rcTitlebarBtnClose_On             = RGB (255, 255, 255);

/*******************************************************************************************************************************
**                                                      Class: APPFRAME_NATIVE::Impl                                          **
***************************************************************************************************************************** */

class APPFRAME_NATIVE::Impl
{
public:
   enum eHOVERBTN
   {
      kHOVERBTN_None,
      kHOVERBTN_Minimize,
      kHOVERBTN_Maximize,
      kHOVERBTN_Close
   };

   enum eHOVERTAB
   {
      kHOVERTAB_None,
      kHOVERTAB_BtnAdd,
      kHOVERTAB_BtnClose,
      kHOVERTAB_Body
   };

   enum eGDIPEN
   {
      kGDIPEN_EditBorder,
      kGDIPEN_TitlebarBorder_Off,
      kGDIPEN_TitlebarBorder_On,
      kGDIPEN_TitlebarItem_On,
      kGDIPEN_TitlebarItem_Off,
      kGDIPEN_TitlebarClose_On,

      kGDIPEN_COUNT
   };

   enum eGDIBRUSH
   {
      kGDIBRUSH_EditBknd,
      kGDIBRUSH_ToolbarBknd,
      kGDIBRUSH_TitlebarBknd,
      kGDIBRUSH_CanvasBknd,
      kGDIBRUSH_TitlebarBtnHover,
      kGDIBRUSH_TitlebarBtnCloseHover,
      kGDIBRUSH_TitlebarTabHover,
      kGDIBRUSH_TitlebarItem_On,
      kGDIBRUSH_TitlebarItem_Off,

      kGDIBRUSH_COUNT
   };

   HPEN        m_ahPens[kGDIPEN_COUNT];
   HBRUSH      m_ahBrush[kGDIBRUSH_COUNT];

public:
   Impl (APPFRAME_NATIVE* pAppFrame, HINSTANCE hInst, LOGGER* pLogger, SNEEZE::ENGINE* pSneeze) :
      m_pAppFrame (pAppFrame),
      m_hInst (hInst),
      m_pLogger (pLogger),
      m_pSneeze (pSneeze),
      m_hWnd (HWND_DESKTOP),
      m_hEdit (NULL),
      m_hUrlPopup (NULL),
      m_bUrlPopupOpen (false),
      m_nUrlPopupHover (-1),
      m_nUrlRowHeight (0),
      m_hFontEdit (NULL),
      m_hwndSetting (NULL),
      m_bBtnEllipsisHover (false),
      m_eHoverBtn (kHOVERBTN_None),
      m_eHoverTab (kHOVERTAB_None),
      m_nTabIx_Hover (-1),
      m_nTabIx_Active (-1),
      m_nTabCount (0),
      m_hPopupMenu (NULL),
      m_hLogLevelMenu (NULL),
      m_hTabIcon (NULL),
      m_eSession (SNEEZE::CONTEXT::kSESSION_PERSISTENT)
   {
      COLORREF acrGDIPen[kGDIPEN_COUNT] =
      {
         crEditBorder,
         crTitlebarBknd,
         crToolbarBknd,
         rcTitlebarItem_On,
         rcTitlebarItem_Off,
         rcTitlebarBtnClose_On
      };

      COLORREF acrGDIBrush[kGDIBRUSH_COUNT] =
      {
         crEditBknd,
         crToolbarBknd,
         crTitlebarBknd,
         crCanvasBknd,
         crTitlebarBtnHover,
         crTitlebarBtnCloseHover,
         crTitlebarTabHover,
         rcTitlebarItem_On,
         rcTitlebarItem_Off,
      };

      for (int n = 0; n < TAB_MAX_COUNT; n++)
         m_pAppFrameTab[n] = nullptr;

      for (int n = 0; n < kGDIPEN_COUNT; n++)
         m_ahPens[n] = (HPEN)CreatePen (PS_SOLID, 1, acrGDIPen[n]);

      for (int n = 0; n < kGDIBRUSH_COUNT; n++)
         m_ahBrush[n] = (HBRUSH)CreateSolidBrush (acrGDIBrush[n]);
   }

   ~Impl ()
   {
      if (m_hwndSetting)
      {
         RemoveWindowSubclass (m_hwndSetting, BtnEllipsisSubclassProc, 0);
         DestroyWindow (m_hwndSetting);
         m_hwndSetting = NULL;
      }

      if (m_hUrlPopup)
      {
         DestroyWindow (m_hUrlPopup);
         m_hUrlPopup = NULL;
      }

      if (m_hEdit)
      {
         RemoveWindowSubclass (m_hEdit, EditSubclassProc, 0);

         DestroyWindow (m_hEdit);
         m_hEdit = NULL;
      }

      if (m_hFontEdit)
      {
         DeleteObject (m_hFontEdit);
         m_hFontEdit = NULL;
      }

      for (int n = 0; n < m_nTabCount; n++)
         delete m_pAppFrameTab[n];

      if (m_hLogLevelMenu != NULL)
         DestroyMenu (m_hLogLevelMenu);

      if (m_hPopupMenu != NULL)
         DestroyMenu (m_hPopupMenu);

      if (m_hTabIcon != NULL)
      {
         DestroyIcon (m_hTabIcon);
         m_hTabIcon = NULL;
      }

      for (int n = 0; n < kGDIPEN_COUNT; n++)
         DeleteObject (m_ahPens[n]);

      for (int n = 0; n < kGDIBRUSH_COUNT; n++)
         DeleteObject (m_ahBrush[n]);
   }

   bool Init (HWND hWnd)
   {
      nlohmann::json& jSettings = APPNATIVE::GetInstance ()->SettingToJSON ();

      m_hWnd = hWnd;

      RECT rcEdit = GetEditWindowRect ();

      // Plain single-line EDIT styled as a Chrome-like rounded pill. The history
      // dropdown is a separate owner-drawn popup window (see RegisterUrlPopupClass
      // / ShowUrlPopup) rather than a native combobox arrow.
      m_hEdit = CreateWindowExA (0, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
         rcEdit.left, rcEdit.top, rcEdit.right - rcEdit.left, rcEdit.bottom - rcEdit.top,
         m_hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_URL_EDIT)), m_hInst, NULL);

      if (m_hEdit)
      {
         UINT     uDpi = GetDpiForWindow (m_hWnd);
         LOGFONTW lf   = { 0 };

         if (SystemParametersInfoForDpi (SPI_GETICONTITLELOGFONT, sizeof (lf), &lf, false, uDpi) != FALSE)
         {
            if ((m_hFontEdit = CreateFontIndirectW (&lf)) != NULL)
               SendMessage (m_hEdit, WM_SETFONT, reinterpret_cast<WPARAM>(m_hFontEdit), TRUE);
         }

         int nMargin = WINUTILS::ScaleDpiEx (4, uDpi);
         SendMessage (m_hEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM (nMargin, nMargin));

         SetWindowSubclass (m_hEdit, EditSubclassProc, 0, reinterpret_cast<DWORD_PTR>(this));

         LayoutEdit ();
      }

      RegisterUrlPopupClass ();

      m_hUrlPopup = CreateWindowExW (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, wszUrlPopupClass, L"",
         WS_POPUP, 0, 0, 10, 10, m_hWnd, NULL, m_hInst, this);

      RECT rcBtn = GetBtnEllipsisRect ();
      m_hwndSetting = CreateWindowExA (0, "BUTTON", "", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, rcBtn.left, rcBtn.top, rcBtn.right - rcBtn.left, rcBtn.bottom - rcBtn.top, m_hWnd, (HMENU)(INT_PTR)IDC_BTN_ELLIPSIS, m_hInst, NULL);

      if (m_hwndSetting)
         SetWindowSubclass (m_hwndSetting, BtnEllipsisSubclassProc, 0, reinterpret_cast<DWORD_PTR>(this));

      FrameTabAdd ();

      int nIconSize = WINUTILS::ScaleDpi (m_hWnd, 16);
      m_hTabIcon = (HICON)LoadImageW (m_hInst, MAKEINTRESOURCEW (IDI_TAB_ICON), IMAGE_ICON, nIconSize, nIconSize, LR_DEFAULTCOLOR);

      m_hLogLevelMenu = CreatePopupMenu ();
      AppendMenuA (m_hLogLevelMenu, MF_STRING, IDM_LOG_TRACE, "Trace");
      AppendMenuA (m_hLogLevelMenu, MF_STRING, IDM_LOG_INFO, "Info");
      AppendMenuA (m_hLogLevelMenu, MF_STRING, IDM_LOG_WARNING, "Warning");
      AppendMenuA (m_hLogLevelMenu, MF_STRING, IDM_LOG_ERROR, "Error");
      AppendMenuA (m_hLogLevelMenu, MF_SEPARATOR, 0, nullptr);
      AppendMenuA (m_hLogLevelMenu, MF_STRING, IDM_LOG_OFF, "Off");

      m_hPopupMenu = CreatePopupMenu ();
      AppendMenuA (m_hPopupMenu, MF_STRING, IDM_WINDOW_PERSISTENT, "New Window");
      AppendMenuA (m_hPopupMenu, MF_STRING, IDM_WINDOW_TRANSITORY, "New Incognito Window");
      AppendMenuA (m_hPopupMenu, MF_SEPARATOR, 0, nullptr);
      AppendMenuA (m_hPopupMenu, MF_STRING, IDM_SETTINGS, "Settings");
      AppendMenuA (m_hPopupMenu, MF_POPUP, (UINT_PTR)m_hLogLevelMenu, "Log Level");
      AppendMenuA (m_hPopupMenu, MF_SEPARATOR, 0, nullptr);
      AppendMenuA (m_hPopupMenu, MF_STRING, IDM_INSPECTOR_RML, "Inspector\tF12");
      AppendMenuA (m_hPopupMenu, MF_SEPARATOR, 0, nullptr);
      AppendMenuA (m_hPopupMenu, MF_STRING, IDM_DEVELOPER_WINDOW,       "Show Developer Console");
      AppendMenuA (m_hPopupMenu, MF_STRING, IDM_DEVELOPER_BOUNDINGBOX,  "Show Developer BoundingBox");
      AppendMenuA (m_hPopupMenu, MF_SEPARATOR, 0, nullptr);
      AppendMenuA (m_hPopupMenu, MF_STRING, IDM_RELEASE_NOTES, "Release Notes");
      AppendMenuA (m_hPopupMenu, MF_STRING, IDM_CHECK_UPDATE, "Check for Updates");
      AppendMenuA (m_hPopupMenu, MF_SEPARATOR, 0, nullptr);
      AppendMenuA (m_hPopupMenu, MF_STRING, IDM_EXIT, "Exit");

      CheckMenuItem (m_hPopupMenu, IDM_DEVELOPER_WINDOW,      jSettings["developer"].value ("console", false) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem (m_hPopupMenu, IDM_DEVELOPER_BOUNDINGBOX, jSettings["developer"].value ("boundingbox", false) ? MF_CHECKED : MF_UNCHECKED);

      UpdateLogLevelMenu ();

      return true;
   }

   /*******************************************************************************************************************************
   **                                                   Impl                                                                     **
   *******************************************************************************************************************************/

   void ProcessInput ()
   {
      if (m_nTabIx_Active >= 0 && m_nTabCount > 0)
         m_pAppFrameTab[m_nTabIx_Active]->ProcessInput ();
   }

   bool IsWindowMax (HWND hWnd)
   {
      bool bResult = false;
      WINDOWPLACEMENT WindowPlacement = { 0 };

      WindowPlacement.length = sizeof (WINDOWPLACEMENT);
      if (GetWindowPlacement (hWnd, &WindowPlacement))
      {
         bResult = (WindowPlacement.showCmd == SW_SHOWMAXIMIZED);
      }

      return bResult;
   }

   void SetMenuItemState (HMENU menu, MENUITEMINFO* menuItemInfo, UINT uItem, bool bEnabled)
   {
      menuItemInfo->fState = bEnabled ? MF_ENABLED : MF_DISABLED;
      SetMenuItemInfo (menu, uItem, false, menuItemInfo);
   }

   RECT GetTitleBarRect (HWND hWnd)
   {
      RECT rcClient;
      SIZE szTitlebar = { 0 };
      const int nVerticalBorders = 2 + 6;
      HTHEME hTheme = OpenThemeData (hWnd, L"WINDOW");

      GetThemePartSize (hTheme, NULL, WP_CAPTION, CS_ACTIVE, NULL, TS_TRUE, &szTitlebar);
      CloseThemeData (hTheme);

      UINT uDpi = GetDpiForWindow (hWnd);

      int nTBHeight = WINUTILS::ScaleDpiEx (szTitlebar.cy, uDpi) + nVerticalBorders;

      GetClientRect (hWnd, &rcClient);
      rcClient.bottom = rcClient.top + nTBHeight;

      if (IsWindowMax (hWnd))
      {
         int nCYFrame = GetSystemMetricsForDpi (SM_CYFRAME, uDpi);

         rcClient.top    -= nCYFrame;
         rcClient.bottom -= nCYFrame;
      }

      return rcClient;
   }

   RECT GetFakeShadowRect (HWND hWnd)
   {
      RECT rcClient;

      GetClientRect (hWnd, &rcClient);
      rcClient.bottom = rcClient.top + WIN32_FAKE_SHADOW_HEIGHT;

      return rcClient;
   }

   TITLEBAR_RECTS TBGetRects (HWND hWnd, const RECT* prcTB)
   {
      TITLEBAR_RECTS TBRects;

      int nBtnWidth = WINUTILS::ScaleDpi (hWnd, 47);

      TBRects.rcClose = *prcTB;
      TBRects.rcClose.top += WIN32_FAKE_SHADOW_HEIGHT;

      TBRects.rcClose.left        = TBRects.rcClose.right - nBtnWidth;
      TBRects.rcMaximize          = TBRects.rcClose;
      TBRects.rcMaximize.left    -= nBtnWidth;
      TBRects.rcMaximize.right   -= nBtnWidth;
      TBRects.rcMinimize          = TBRects.rcMaximize;
      TBRects.rcMinimize.left    -= nBtnWidth;
      TBRects.rcMinimize.right   -= nBtnWidth;

      return TBRects;
   }

   void DrawTabIcon (HDC hdc, RECT& label_rect, UINT uDpi)
   {
      if (m_hTabIcon)
      {
         int nIconSize = WINUTILS::ScaleDpiEx (16, uDpi);
         int nGap      = WINUTILS::ScaleDpiEx (6, uDpi);
         int nIconY    = label_rect.top + (label_rect.bottom - label_rect.top - nIconSize) / 2;

         DrawIconEx (hdc, label_rect.left, nIconY, m_hTabIcon, nIconSize, nIconSize, 0, NULL, DI_NORMAL);

         label_rect.left += nIconSize + nGap;
      }
   }

   void DrawTabCloseGlyph (HDC hdc, const RECT& rcBtn)
   {
      UINT  uDpi       = GetDpiForWindow (m_hWnd);
      int   nGlyphHalf = WINUTILS::ScaleDpiEx (4, uDpi);
      int   nThick     = WINUTILS::ScaleDpiEx (1, uDpi); // + 1;
      POINT ptCenter   = { (rcBtn.left + rcBtn.right) / 2, (rcBtn.top + rcBtn.bottom) / 2 };

      HPEN hPen    = CreatePen (PS_SOLID, nThick, RGB (0, 0, 0));

      HPEN hPenOld = (HPEN)SelectObject (hdc, hPen);
      {
         MoveToEx (hdc, ptCenter.x - nGlyphHalf,     ptCenter.y - nGlyphHalf, NULL);
         LineTo   (hdc, ptCenter.x + nGlyphHalf + 1, ptCenter.y + nGlyphHalf + 1);

         MoveToEx (hdc, ptCenter.x + nGlyphHalf,     ptCenter.y - nGlyphHalf, NULL);
         LineTo   (hdc, ptCenter.x - nGlyphHalf - 1, ptCenter.y + nGlyphHalf + 1);
      }
      SelectObject (hdc, hPenOld);

      DeleteObject (hPen);
   }

   void TabComputeLayout (RECT* aprcTab_Body, RECT* aprcTab_BtnClose, RECT &rcTab_BtnAdd)
   {
      UINT uDpi = GetDpiForWindow (m_hWnd);

      RECT rcTitlebar = GetTitleBarRect (m_hWnd);
      TITLEBAR_RECTS TBRects = TBGetRects (m_hWnd, &rcTitlebar);

      int nLeftPad            = WINUTILS::ScaleDpiEx (  6, uDpi);   // Padding for Left Margin
      int nWidth_BtnAdd       = WINUTILS::ScaleDpiEx ( 36, uDpi);   // Width for Add Button
      int nWidth_TabPad       = WINUTILS::ScaleDpiEx (  4, uDpi);   // Pad After Tab
      int nTopMargin          = WINUTILS::ScaleDpiEx (  4, uDpi) + WIN32_FAKE_SHADOW_HEIGHT;  // Tab Top Margin
      int strip_bottom_inset  = WINUTILS::ScaleDpiEx (  2, uDpi);
      int min_tab_width       = WINUTILS::ScaleDpiEx ( 72, uDpi);
      int max_tab_width       = WINUTILS::ScaleDpiEx (196, uDpi);
      int close_slot          = WINUTILS::ScaleDpiEx ( 22, uDpi);
      int tab_overlap         = WINUTILS::ScaleDpiEx (  1, uDpi);

      int strip_top = rcTitlebar.top + nTopMargin;
      int strip_bottom = rcTitlebar.bottom - strip_bottom_inset;

      int usable_left = rcTitlebar.left + nLeftPad;
      int usable_right = TBRects.rcMinimize.left - nWidth_TabPad - nWidth_BtnAdd - nLeftPad;
      int usable_width = usable_right - usable_left;
      if (usable_width < min_tab_width)
      {
         usable_width = min_tab_width;
      }

      int tab_width = min_tab_width;
      if (m_nTabCount > 0)
      {
         tab_width = usable_width / m_nTabCount;
         if (tab_width > max_tab_width)
         {
            tab_width = max_tab_width;
         }
         if (tab_width < min_tab_width)
         {
            tab_width = min_tab_width;
         }
      }

      int cursor_x = usable_left;
      int tab_iz;
      for (tab_iz = 0; tab_iz < m_nTabCount; tab_iz++)
      {
         aprcTab_Body[tab_iz].left = cursor_x;
         aprcTab_Body[tab_iz].top = strip_top;
         aprcTab_Body[tab_iz].right = cursor_x + tab_width;
         aprcTab_Body[tab_iz].bottom = strip_bottom;

         aprcTab_BtnClose[tab_iz] = aprcTab_Body[tab_iz];
         aprcTab_BtnClose[tab_iz].left = aprcTab_Body[tab_iz].right - close_slot;

         cursor_x += tab_width - tab_overlap;
      }

      rcTab_BtnAdd.left     = cursor_x + nWidth_TabPad;
      rcTab_BtnAdd.top      = strip_top;
      rcTab_BtnAdd.right    = rcTab_BtnAdd.left + nWidth_BtnAdd;
      rcTab_BtnAdd.bottom   = strip_bottom;

      for (; tab_iz < TAB_MAX_COUNT; tab_iz++)
      {
         SetRectEmpty (&aprcTab_Body[tab_iz]);
         SetRectEmpty (&aprcTab_BtnClose[tab_iz]);
      }
   }

   int GetTBHeight ()
   {
      RECT rcClient;
      HTHEME hTheme;
      SIZE sz = { 0 };
      const int nVerticalBorders = 2 + 6;

      if ((hTheme = OpenThemeData (m_hWnd, L"WINDOW")) != NULL)
      {
         GetThemePartSize (hTheme, NULL, WP_CAPTION, CS_ACTIVE, NULL, TS_TRUE, &sz);

         CloseThemeData (hTheme);
      }

      return WINUTILS::ScaleDpi (m_hWnd, sz.cy) + nVerticalBorders;
   }

   int GetBarHeight ()
   {
      return WINUTILS::ScaleDpi (m_hWnd, BAR_HEIGHT_DPI96);
   }

   int GetContentTop ()
   {
      return GetTBHeight () + GetBarHeight ();
   }

   RECT GetBarRect ()
   {
      RECT rcClient;

      GetClientRect (m_hWnd, &rcClient);
      rcClient.top    = GetTBHeight ();
      rcClient.bottom = GetContentTop ();

      return rcClient;
   }

   RECT GetBtnEllipsisRect ()
   {
      UINT uDpi      = GetDpiForWindow (m_hWnd);
      int  nMarginX  = WINUTILS::ScaleDpiEx (BAR_MARGIN_X_DPI96, uDpi);
      int  nBtnSize  = WINUTILS::ScaleDpiEx (BAR_BTN_SIZE_DPI96, uDpi);
      RECT rcBar     = GetBarRect ();
      RECT rcBtn;

      rcBtn.right    = rcBar.right - nMarginX;
      rcBtn.left     = rcBtn.right - nBtnSize;
      rcBtn.top      = rcBar.top + ((rcBar.bottom - rcBar.top) - nBtnSize) / 2;
      rcBtn.bottom   = rcBtn.top + nBtnSize;

      return rcBtn;
   }

   RECT GetEditRect ()
   {
      UINT uDpi      = GetDpiForWindow (m_hWnd);
      int  nMarginX  = WINUTILS::ScaleDpiEx (BAR_MARGIN_X_DPI96, uDpi);
      int  nMarginY  = WINUTILS::ScaleDpiEx (BAR_MARGIN_Y_DPI96, uDpi);
      int  nGap      = WINUTILS::ScaleDpiEx (BAR_GAP_DPI96, uDpi);
      RECT rcBar     = GetBarRect ();
      RECT rcBtn     = GetBtnEllipsisRect ();
      RECT rcEdit;

      rcEdit.left    = rcBar.left + nMarginX;
      rcEdit.right   = rcBtn.left - nGap;
      rcEdit.top     = rcBar.top + nMarginY;
      rcEdit.bottom  = rcBar.bottom - nMarginY;

      return rcEdit;
   }

   RECT GetEditInnerRect ()
   {
      UINT uDpi     = GetDpiForWindow (m_hWnd);
      int  nBorder  = WINUTILS::ScaleDpiEx (EDIT_BORDER_DPI96, uDpi);
      RECT rcEdit   = GetEditRect ();

      if (nBorder < 1)
         nBorder = 1;

      rcEdit.left   += nBorder;
      rcEdit.top    += nBorder;
      rcEdit.right  -= nBorder;
      rcEdit.bottom -= nBorder;

      return rcEdit;
   }

   // Corner radius of the rounded URL pill -- half the pill height yields fully
   // rounded (stadium) ends, matching Chrome's omnibox.
   int UrlPillRadius ()
   {
      RECT rcPill = GetEditRect ();

      return (rcPill.bottom - rcPill.top) / 2;
   }

   RECT GetEditWindowRect ()
   {
      RECT rcWnd    = GetEditInnerRect ();
      int  nRadius  = UrlPillRadius ();

      // Inset the edit horizontally past the rounded ends so the EDIT rectangle
      // never paints over the pill's rounded corners (its background matches, but
      // the corners must show the toolbar colour to read as rounded).
      rcWnd.left  += nRadius;
      rcWnd.right -= nRadius;

      if (rcWnd.right < rcWnd.left)
         rcWnd.right = rcWnd.left;

      if (m_hFontEdit)
      {
         UINT         uDpi       = GetDpiForWindow (m_hWnd);
         TEXTMETRIC   TextMetric = { 0 };
         HDC          hdc        = GetDC (m_hWnd);

         if (hdc)
         {
            HFONT hfntOld = (HFONT)SelectObject (hdc, m_hFontEdit);
            GetTextMetrics (hdc, &TextMetric);
            SelectObject (hdc, hfntOld);
            ReleaseDC (m_hWnd, hdc);

            int nLineHeight = TextMetric.tmAscent + TextMetric.tmDescent;
            int nWndHeight  = nLineHeight + WINUTILS::ScaleDpiEx (2, uDpi);
            int nInnerHeight = rcWnd.bottom - rcWnd.top;

            if (nWndHeight > nInnerHeight)
               nWndHeight = nInnerHeight;

            int nOffsetY = (nInnerHeight - nWndHeight) / 2;
            rcWnd.top     += nOffsetY;
            rcWnd.bottom   = rcWnd.top + nWndHeight;
         }
      }

      return rcWnd;
   }

   void LayoutEdit ()
   {
      if (m_hEdit)
      {
         UINT uDpi    = GetDpiForWindow (m_hWnd);
         RECT rcWnd   = GetEditWindowRect ();
         RECT rcInner = GetEditInnerRect ();

         // History-popup row height: a full font line plus vertical padding.
         int nLineHeight = rcWnd.bottom - rcWnd.top;
         HDC hdc         = GetDC (m_hWnd);

         if (hdc)
         {
            TEXTMETRIC TextMetric = { 0 };
            HFONT      hfntOld     = m_hFontEdit ? (HFONT)SelectObject (hdc, m_hFontEdit) : NULL;

            if (GetTextMetrics (hdc, &TextMetric))
               nLineHeight = TextMetric.tmAscent + TextMetric.tmDescent;

            if (hfntOld)
               SelectObject (hdc, hfntOld);

            ReleaseDC (m_hWnd, hdc);
         }

         m_nUrlRowHeight = nLineHeight + WINUTILS::ScaleDpiEx (10, uDpi);

         MoveWindow (m_hEdit, rcWnd.left, rcWnd.top, rcWnd.right - rcWnd.left, rcWnd.bottom - rcWnd.top, TRUE);
         InvalidateRect (m_hWnd, &rcInner, FALSE);
      }
   }

   void FrameTabActive (int nTabIx_Active, bool bHideOld)
   {
      if (nTabIx_Active != m_nTabIx_Active || !bHideOld)
      {
         if (bHideOld && m_nTabIx_Active >= 0)
         {
            m_pAppFrameTab[m_nTabIx_Active]->ViewportDetach ();
            m_pAppFrameTab[m_nTabIx_Active]->Show (false);
         }

         m_nTabIx_Active = nTabIx_Active;
         m_pAppFrameTab[m_nTabIx_Active]->Show (true);
         m_pAppFrameTab[m_nTabIx_Active]->NotifyChildReady (m_hWnd, true);
         m_pAppFrameTab[m_nTabIx_Active]->ViewportAttach ();
         SetWindowTextW (m_hWnd, m_pAppFrameTab[m_nTabIx_Active]->Title ().c_str ());
         SetFocus (m_hWnd);
         m_pAppFrameTab[m_nTabIx_Active]->UpdateUrl (m_hEdit);
      }
   }

   void FrameTabAdd ()
   {
      RECT rcClient, rcTitlebar;

      if (m_nTabCount < TAB_MAX_COUNT)
      {
         m_pAppFrameTab[m_nTabCount] = new APPFRAMETAB_NATIVE (m_hInst, m_pLogger, m_pSneeze, std::wstring (L"New Tab"));

         rcTitlebar = GetTitleBarRect (m_hWnd);
         GetClientRect (m_hWnd, &rcClient);

         m_pAppFrameTab[m_nTabCount]->Init (m_hWnd, 0, GetContentTop (), (rcClient.right - rcClient.left), (rcClient.bottom - rcClient.top) - GetContentTop (), m_eSession);

         FrameTabActive (m_nTabCount++, true);
      }
   }

   void FrameTabClose (int nTabIx)
   {
      bool bUpdateTab = (m_nTabIx_Active == nTabIx);

      if (m_nTabCount > 0)
      {
         delete m_pAppFrameTab[nTabIx];
         m_nTabCount--;

         for (int n = nTabIx; n < m_nTabCount; n++)
         {
            m_pAppFrameTab[n] = m_pAppFrameTab[n + 1];
         }

         if (m_nTabIx_Active > nTabIx)
         {
            m_nTabIx_Active--;
         }
         else if (m_nTabIx_Active >= m_nTabCount)
         {
            m_nTabIx_Active = m_nTabCount - 1;
         }

         if (m_nTabCount > 0)
         {
            if (bUpdateTab)
               FrameTabActive (m_nTabIx_Active, false);
         }
         else
            PostMessageW (m_hWnd, WM_CLOSE, 0, 0);
      }
   }

   bool TabPointHitTest (POINT point, eHOVERTAB& eHoverTabNew, int& nTabIx_Hover)
   {
      RECT rcTitlebar = GetTitleBarRect (m_hWnd);
      bool hit_strip = false;
      eHoverTabNew = kHOVERTAB_None;
      nTabIx_Hover = -1;

      if (point.y >= rcTitlebar.top && point.y < rcTitlebar.bottom)
      {
         TITLEBAR_RECTS TBRects = TBGetRects (m_hWnd, &rcTitlebar);
         if (!PtInRect (&TBRects.rcMinimize, point) && !PtInRect (&TBRects.rcMaximize, point) && !PtInRect (&TBRects.rcClose, point))
         {
            RECT aprcTab_Body[TAB_MAX_COUNT];
            RECT aprcTab_BtnClose[TAB_MAX_COUNT];
            RECT add_button_rect = { 0 };
            TabComputeLayout (aprcTab_Body, aprcTab_BtnClose, add_button_rect);

            if (PtInRect (&add_button_rect, point))
            {
               eHoverTabNew = kHOVERTAB_BtnAdd;
               hit_strip = true;
            }
            else
            {
               int tab_iz;
               for (tab_iz = m_nTabCount - 1; tab_iz >= 0; tab_iz--)
               {
                  if (!PtInRect (&aprcTab_Body[tab_iz], point))
                  {
                     continue;
                  }

                  hit_strip      = true;
                  nTabIx_Hover   = tab_iz;
                  if (PtInRect (&aprcTab_BtnClose[tab_iz], point))
                  {
                     eHoverTabNew = kHOVERTAB_BtnClose;
                  }
                  else
                  {
                     eHoverTabNew = kHOVERTAB_Body;
                  }
                  break;
               }
            }
         }
      }

      return hit_strip;
   }

   void UpdateHoverState (POINT point)
   {
      eHOVERBTN eHoverBtn     = kHOVERBTN_None;
      eHOVERTAB eHoverTabNew  = kHOVERTAB_None;
      int       nTabIx_Hover  = -1;
      RECT rcTitlebar = GetTitleBarRect (m_hWnd);
      TITLEBAR_RECTS TBRects = TBGetRects (m_hWnd, &rcTitlebar);

      if (PtInRect (&TBRects.rcClose, point))
      {
         eHoverBtn = kHOVERBTN_Close;
      }
      else if (PtInRect (&TBRects.rcMinimize, point))
      {
         eHoverBtn = kHOVERBTN_Minimize;
      }
      else if (PtInRect (&TBRects.rcMaximize, point))
      {
         eHoverBtn = kHOVERBTN_Maximize;
      }

      TabPointHitTest (point, eHoverTabNew, nTabIx_Hover);

      if (eHoverBtn != m_eHoverBtn  ||  eHoverTabNew != m_eHoverTab  ||  nTabIx_Hover != m_nTabIx_Hover)
      {
         m_eHoverBtn    = eHoverBtn;
         m_eHoverTab    = eHoverTabNew;
         m_nTabIx_Hover = nTabIx_Hover;

         InvalidateRect (m_hWnd, &rcTitlebar, FALSE);
      }
   }

   void ShowEllipsisMenu ()
   {
      RECT rcBtn;

      UpdateLogLevelMenu ();

      GetWindowRect (m_hwndSetting, &rcBtn);

      TrackPopupMenu (m_hPopupMenu, TPM_RIGHTALIGN | TPM_TOPALIGN, rcBtn.right, rcBtn.bottom, 0, m_hWnd, nullptr);
   }

   void UpdateLogLevelMenu ()
   {
      LOGGER::eLOGLEVEL Level = m_pAppFrame->m_pLogger->LogLevel ();

      CheckMenuItem (m_hLogLevelMenu, IDM_LOG_TRACE,     (Level == LOGGER::kLOGLEVEL_Trace)   ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem (m_hLogLevelMenu, IDM_LOG_INFO,      (Level == LOGGER::kLOGLEVEL_Info)    ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem (m_hLogLevelMenu, IDM_LOG_WARNING,   (Level == LOGGER::kLOGLEVEL_Warning) ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem (m_hLogLevelMenu, IDM_LOG_ERROR,     (Level == LOGGER::kLOGLEVEL_Error)   ? MF_CHECKED : MF_UNCHECKED);
      CheckMenuItem (m_hLogLevelMenu, IDM_LOG_OFF,       (Level == LOGGER::kLOGLEVEL_Off)     ? MF_CHECKED : MF_UNCHECKED);
   }

   void SetLogLevel (LOGGER::eLOGLEVEL Level)
   {
      m_pAppFrame->m_pLogger->LogLevel (Level);

      nlohmann::json& jSettings = APPNATIVE::GetInstance ()->SettingToJSON ();

      jSettings["logger"]["level"] = g_szLogLevels[Level];
   }

   void ToggleInspector ()
   {
      if (m_nTabIx_Active >= 0 && m_nTabIx_Active < m_nTabCount)
         m_pAppFrameTab[m_nTabIx_Active]->ToggleInspector ();
   }

   void ShowSettings ()
   {
      if (m_nTabIx_Active >= 0 && m_nTabIx_Active < m_nTabCount)
         m_pAppFrameTab[m_nTabIx_Active]->ShowSettings ();
   }

   void ShowReleaseNotes ()
   {
      APPNATIVE::GetInstance ()->ShowReleaseNotes ((void*) m_hWnd);
   }

   void Reload (bool bReset)
   {
      if (m_nTabIx_Active >= 0 && m_nTabIx_Active < m_nTabCount)
         m_pAppFrameTab[m_nTabIx_Active]->Reload (bReset);
   }

   void DeveloperWindow ()
   {
      nlohmann::json& jSettings = APPNATIVE::GetInstance ()->SettingToJSON ();
      bool bConsole = jSettings["developer"].value ("console", false);

      CheckMenuItem (m_hPopupMenu, IDM_DEVELOPER_WINDOW, !bConsole ? MF_CHECKED : MF_UNCHECKED);

      jSettings["developer"]["console"] = !bConsole;
   }

   void DeveloperBoundingBox ()
   {
      SNEEZE::ENGINE::CONFIG Config;

      m_pSneeze->GetConfig (Config);

      nlohmann::json& jSettings = APPNATIVE::GetInstance ()->SettingToJSON ();
      Config.bBoundingBox = jSettings["developer"].value ("boundingbox", false);

      Config.bBoundingBox = !Config.bBoundingBox;

      CheckMenuItem (m_hPopupMenu, IDM_DEVELOPER_BOUNDINGBOX, Config.bBoundingBox ? MF_CHECKED : MF_UNCHECKED);

      jSettings["developer"]["boundingbox"] = Config.bBoundingBox;

      m_pSneeze->SetConfig (Config);
   }

   void UpdateCheck ()
   {
      APPNATIVE::GetInstance ()->CheckForUpdate ();
   }

   /*******************************************************************************************************************************
   **                                       WIN32 Messages                                                                       **
   ***************************************************************************************************************************** */

   LRESULT onNCCalcSize (bool& bDefWndProc, WPARAM wParam, LPARAM lParam)
   {
      LRESULT lResult = 0;

      if (wParam != 0)
      {
         bDefWndProc = false;

         UINT uDpi = GetDpiForWindow (m_hWnd);

         int frame_x = GetSystemMetricsForDpi (SM_CXFRAME, uDpi);
         int frame_y = GetSystemMetricsForDpi (SM_CYFRAME, uDpi);
         int padding = GetSystemMetricsForDpi (SM_CXPADDEDBORDER, uDpi);

         NCCALCSIZE_PARAMS* params = (NCCALCSIZE_PARAMS*)lParam;
         RECT* rcRequested = params->rgrc;

         rcRequested->right -= frame_x + padding;
         rcRequested->left += frame_x + padding;
         rcRequested->bottom -= frame_y + padding;

         if (IsWindowMax (m_hWnd))
         {
            rcRequested->top += frame_y + padding;
         }
      }

      return lResult;
   }

   LRESULT onActivate (bool& bDefWndProc, WPARAM wParam, LPARAM lParam)
   {
      LRESULT lResult = 0;

      RECT rc = GetTitleBarRect (m_hWnd);
      InvalidateRect (m_hWnd, &rc, FALSE);

      if (LOWORD (wParam) != WA_INACTIVE)
      {
         m_pAppFrame->m_pController->Window_OnFocus (m_pAppFrame);
      }
      else
      {
         HideUrlPopup ();
         m_pAppFrame->m_pController->Window_OnBlur (m_pAppFrame);
      }

      return lResult;
   }

   LRESULT onNCHitTest (bool& bDefWndProc, WPARAM wParam, LPARAM lParam)
   {
      LRESULT lResult = 0;
      LRESULT lHit = DefWindowProcW (m_hWnd, WM_NCHITTEST, wParam, lParam);
      UINT uDpi;
      int nFrameY, nPadding;
      POINT cursor_point = { 0 };
      eHOVERTAB eHoverTab = kHOVERTAB_None;
      int       nTabIx_Hover = -1;

      bDefWndProc = false;

      switch (lHit)
      {
      case HTNOWHERE:
      case HTRIGHT:
      case HTLEFT:
      case HTTOPLEFT:
      case HTTOP:
      case HTTOPRIGHT:
      case HTBOTTOMRIGHT:
      case HTBOTTOM:
      case HTBOTTOMLEFT:
         lResult = lHit;
         break;

      default:
         uDpi = GetDpiForWindow (m_hWnd);

         int nFrameY = GetSystemMetricsForDpi (SM_CYFRAME, uDpi);
         int nPadding = GetSystemMetricsForDpi (SM_CXPADDEDBORDER, uDpi);

         cursor_point.x = GET_X_LPARAM (lParam);
         cursor_point.y = GET_Y_LPARAM (lParam);
         ScreenToClient (m_hWnd, &cursor_point);

         RECT rcTitleBar = GetTitleBarRect (m_hWnd);
         TITLEBAR_RECTS TBRects = TBGetRects (m_hWnd, &rcTitleBar);

         if (PtInRect (&TBRects.rcMaximize, cursor_point))
         {
            lResult = HTMAXBUTTON;
         }
         else if (!IsWindowMax (m_hWnd) && cursor_point.y > 0 && cursor_point.y < nFrameY + nPadding)
         {
            lResult = HTTOP;
         }
         else if (cursor_point.y < rcTitleBar.bottom)
         {
            lResult = HTCAPTION;

            if (m_nTabCount > 0)
            {
               if (TabPointHitTest (cursor_point, eHoverTab, nTabIx_Hover))
               {
                  lResult = HTCLIENT;
               }
            }
         }
         else lResult = HTCLIENT;
         break;
      }

      return lResult;
   }

   LRESULT onPaint (bool& bDefWndProc, WPARAM wParam, LPARAM lParam)
   {
      LRESULT lResult = 0;
      RECT rcIcon;
      PAINTSTRUCT ps;
      HPEN     hpenOld, hpenOld2;
      HBRUSH   hbrOld;
      RECT rcClient, rcTmp;
      bool bIsMaxHover;

      HWND hForeground = GetForegroundWindow ();
      bool bFocus = (hForeground == m_hWnd) || (hForeground != NULL && IsChild (m_hWnd, hForeground));

      UINT uDpi = GetDpiForWindow (m_hWnd);

      HDC hdc = BeginPaint (m_hWnd, &ps);

      GetClientRect (m_hWnd, &rcClient);
      RECT rcTitleBar = GetTitleBarRect (m_hWnd);

      rcTmp = rcClient;
      rcTmp.top = rcTitleBar.bottom;
      FillRect (hdc, &rcTmp, m_ahBrush[kGDIBRUSH_CanvasBknd]);

      rcTmp = GetBarRect ();
      FillRect (hdc, &rcTmp, m_ahBrush[kGDIBRUSH_ToolbarBknd]);

      // The URL bar is a Chrome-style rounded pill: a single RoundRect fills the
      // interior with the edit background and strokes the border. Fully rounded
      // (stadium) ends -- ellipse diameter equals the pill height.
      rcTmp = GetEditRect ();
      {
         int nEllipse = rcTmp.bottom - rcTmp.top;

         hpenOld = (HPEN)SelectObject (hdc, m_ahPens[kGDIPEN_EditBorder]);
         hbrOld  = (HBRUSH)SelectObject (hdc, m_ahBrush[kGDIBRUSH_EditBknd]);

         RoundRect (hdc, rcTmp.left, rcTmp.top, rcTmp.right, rcTmp.bottom, nEllipse, nEllipse);

         SelectObject (hdc, hbrOld);
         SelectObject (hdc, hpenOld);
      }

      FillRect (hdc, &rcTitleBar, m_ahBrush[kGDIBRUSH_TitlebarBknd]);

      HTHEME hTheme = OpenThemeData (m_hWnd, L"WINDOW");

      TITLEBAR_RECTS TBRects = TBGetRects (m_hWnd, &rcTitleBar);

      int icon_dimension = WINUTILS::ScaleDpiEx (10, uDpi);
      int corner_ellipse = WINUTILS::ScaleDpiEx (6, uDpi);

      // Caption buttons
      if (m_eHoverBtn == kHOVERBTN_Minimize)
      {
         FillRect (hdc, &TBRects.rcMinimize, m_ahBrush[kGDIBRUSH_TitlebarBtnHover]);
      }
      SetRect (&rcIcon, 0, 0, icon_dimension, 1);
      WINUTILS::RectCenterBox (rcIcon, TBRects.rcMinimize);
      FillRect (hdc, &rcIcon, m_ahBrush[bFocus ? kGDIBRUSH_TitlebarItem_On :kGDIBRUSH_TitlebarItem_Off]);

      bIsMaxHover = (m_eHoverBtn == kHOVERBTN_Maximize);
      if (bIsMaxHover)
      {
         FillRect (hdc, &TBRects.rcMaximize, m_ahBrush[kGDIBRUSH_TitlebarBtnHover]);
      }

      SetRect (&rcIcon, 0, 0, icon_dimension, icon_dimension);
      WINUTILS::RectCenterBox (rcIcon, TBRects.rcMaximize);

      hpenOld = (HPEN)SelectObject (hdc, m_ahPens[bFocus ? kGDIPEN_TitlebarItem_On : kGDIPEN_TitlebarItem_Off]);
      {
         hbrOld = (HBRUSH)SelectObject (hdc, GetStockObject (HOLLOW_BRUSH));
         {
            if (IsWindowMax (m_hWnd))
            {
               Rectangle (hdc, rcIcon.left + WIN32_MAXIMIZED_RECTANGLE_OFFSET, rcIcon.top - WIN32_MAXIMIZED_RECTANGLE_OFFSET, rcIcon.right + WIN32_MAXIMIZED_RECTANGLE_OFFSET, rcIcon.bottom - WIN32_MAXIMIZED_RECTANGLE_OFFSET);
               FillRect (hdc, &rcIcon, bIsMaxHover ? m_ahBrush[kGDIBRUSH_TitlebarBtnHover] : m_ahBrush[kGDIBRUSH_TitlebarBknd]);
            }
            Rectangle (hdc, rcIcon.left, rcIcon.top, rcIcon.right, rcIcon.bottom);

            if (m_eHoverBtn == kHOVERBTN_Close)
            {
               FillRect (hdc, &TBRects.rcClose, m_ahBrush[kGDIBRUSH_TitlebarBtnCloseHover]);
               hpenOld2 = (HPEN)SelectObject (hdc, m_ahPens[kGDIPEN_TitlebarClose_On]);
            }
            else hpenOld2 = NULL;

            SetRect (&rcIcon, 0, 0, icon_dimension, icon_dimension);
            WINUTILS::RectCenterBox (rcIcon, TBRects.rcClose);
            MoveToEx (hdc, rcIcon.left, rcIcon.top, NULL);
            LineTo (hdc, rcIcon.right + 1, rcIcon.bottom + 1);
            MoveToEx (hdc, rcIcon.left, rcIcon.bottom, NULL);
            LineTo (hdc, rcIcon.right + 1, rcIcon.top - 1);

            if (hpenOld2 != NULL)
            {
               SelectObject (hdc, hpenOld2);
            }
         }
         SelectObject (hdc, hbrOld);
      }
      SelectObject (hdc, hpenOld);

      LOGFONTW lf;
      HFONT hfntOld, hfntTheme;
      if (SystemParametersInfoForDpi (SPI_GETICONTITLELOGFONT, sizeof (lf), &lf, false, uDpi) != FALSE)
      {
         if ((hfntTheme = CreateFontIndirectW (&lf)) != NULL)
         {
            hfntOld = (HFONT)SelectObject (hdc, hfntTheme);
            {
               DTTOPTS draw_theme_options = { sizeof (draw_theme_options) };
               draw_theme_options.dwFlags = DTT_TEXTCOLOR;
               draw_theme_options.crText = bFocus ? rcTitlebarItem_On : rcTitlebarItem_Off;

               if (m_nTabCount > 0)
               {
                  RECT aprcTab_Body[TAB_MAX_COUNT];
                  RECT aprcTab_BtnClose[TAB_MAX_COUNT];
                  RECT rcBtnAdd = { 0 };
                  TabComputeLayout (aprcTab_Body, aprcTab_BtnClose, rcBtnAdd);

                  int tab_iz;
                  for (tab_iz = 0; tab_iz < m_nTabCount; tab_iz++)
                  {
                     if (tab_iz == m_nTabIx_Active)
                     {
                        continue;
                     }

                     bool tab_hot = (m_nTabIx_Hover == tab_iz);

                     hpenOld = (HPEN)SelectObject (hdc, m_ahPens[kGDIPEN_TitlebarBorder_Off]);
                     {
                        hbrOld = (HBRUSH)SelectObject (hdc, m_ahBrush[tab_hot ? kGDIBRUSH_TitlebarTabHover : kGDIBRUSH_TitlebarBknd]);
                        {
                           RoundRect (hdc, aprcTab_Body[tab_iz].left, aprcTab_Body[tab_iz].top, aprcTab_Body[tab_iz].right, aprcTab_Body[tab_iz].bottom, corner_ellipse, corner_ellipse);
                        }
                        SelectObject (hdc, hbrOld);
                     }
                     SelectObject (hdc, hpenOld);

                     RECT label_rect = aprcTab_Body[tab_iz];
                     label_rect.right = aprcTab_BtnClose[tab_iz].left;
                     label_rect.left += WINUTILS::ScaleDpiEx (8, uDpi);
                     DrawTabIcon (hdc, label_rect, uDpi);
                     DrawThemeTextEx (hTheme, hdc, 0, 0, m_pAppFrameTab[tab_iz]->Title ().c_str (), -1, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, &label_rect, &draw_theme_options);

                     bool close_hot = (m_nTabIx_Hover == tab_iz && m_eHoverTab == kHOVERTAB_BtnClose);
                     if (close_hot)
                     {
                        FillRect (hdc, &aprcTab_BtnClose[tab_iz], m_ahBrush[kGDIBRUSH_TitlebarBtnHover]);
                     }

                     DrawTabCloseGlyph (hdc, aprcTab_BtnClose[tab_iz]);
                  }

                  if (m_nTabIx_Active >= 0 && m_nTabIx_Active < m_nTabCount)
                  {
                     hpenOld = (HPEN)SelectObject (hdc, m_ahPens[kGDIPEN_TitlebarBorder_On]);
                     {
                        hbrOld = (HBRUSH)SelectObject (hdc, m_ahBrush[kGDIBRUSH_ToolbarBknd]);
                        {
                           RoundRect (hdc, aprcTab_Body[m_nTabIx_Active].left, aprcTab_Body[m_nTabIx_Active].top, aprcTab_Body[m_nTabIx_Active].right, rcTitleBar.bottom/* + WINUTILS::ScaleDpiEx (2, uDpi)*/, corner_ellipse, corner_ellipse);
                        }
                        SelectObject (hdc, hbrOld);
                     }
                     SelectObject (hdc, hpenOld);

                     rcTmp = aprcTab_Body[m_nTabIx_Active];
                     rcTmp.right = aprcTab_BtnClose[m_nTabIx_Active].left;
                     rcTmp.left += WINUTILS::ScaleDpiEx (8, uDpi);
                     rcTmp.bottom = aprcTab_Body[m_nTabIx_Active].bottom;
                     DrawTabIcon (hdc, rcTmp, uDpi);
                     DrawThemeTextEx (hTheme, hdc, 0, 0, m_pAppFrameTab[m_nTabIx_Active]->Title ().c_str (), -1, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, &rcTmp, &draw_theme_options);

                     if (m_nTabIx_Hover == m_nTabIx_Active && m_eHoverTab == kHOVERTAB_BtnClose)
                     {
                        FillRect (hdc, &aprcTab_BtnClose[m_nTabIx_Active], m_ahBrush[kGDIBRUSH_TitlebarBtnHover]);
                     }

                     DrawTabCloseGlyph (hdc, aprcTab_BtnClose[m_nTabIx_Active]);
                  }

                  if (m_eHoverTab == kHOVERTAB_BtnAdd)
                  {
                     FillRect (hdc, &rcBtnAdd, m_ahBrush[kGDIBRUSH_TitlebarBtnHover]);
                  }

                  int half = icon_dimension / 2;
                  int cx   = (rcBtnAdd.left + rcBtnAdd.right) / 2;
                  int cy   = (rcBtnAdd.top  + rcBtnAdd.bottom) / 2;

                  rcIcon.left   = cx - half;
                  rcIcon.top    = cy;
                  rcIcon.right  = cx + half + 1;
                  rcIcon.bottom = cy + 1;
                  FillRect (hdc, &rcIcon, m_ahBrush[bFocus ? kGDIBRUSH_TitlebarItem_On : kGDIBRUSH_TitlebarItem_Off]);

                  rcIcon.left   = cx;
                  rcIcon.top    = cy - half;
                  rcIcon.right  = cx + 1;
                  rcIcon.bottom = cy + half + 1;
                  FillRect (hdc, &rcIcon, m_ahBrush[bFocus ? kGDIBRUSH_TitlebarItem_On : kGDIBRUSH_TitlebarItem_Off]);
               }
            }
            SelectObject (hdc, hfntOld);

            DeleteObject (hfntTheme);
         }
      }

      CloseThemeData (hTheme);

      COLORREF crFakeTopShadow = bFocus ? crShadow : RGB ((GetRValue (crTitlebarBknd) + GetRValue (crShadow)) / 2, (GetGValue (crTitlebarBknd) + GetGValue (crShadow)) / 2, (GetBValue (crTitlebarBknd) + GetBValue (crShadow)) / 2);
      HBRUSH hbrFakeTopShadow = CreateSolidBrush (crFakeTopShadow);
      rcTmp = GetFakeShadowRect (m_hWnd);
      FillRect (hdc, &rcTmp, hbrFakeTopShadow);
      DeleteObject (hbrFakeTopShadow);

      EndPaint (m_hWnd, &ps);

      return lResult;
   }

   LRESULT onMouseMove (bool& bDefWndProc, WPARAM wParam, LPARAM lParam)
   {
      LRESULT lResult = 0;
      POINT   pt;

      if (m_nTabCount > 0)
      {
         pt.x = GET_X_LPARAM (lParam);
         pt.y = GET_Y_LPARAM (lParam);
         UpdateHoverState (pt);
      }

      return lResult;
   }

   LRESULT onLButtonDown (bool& bDefWndProc, WPARAM wParam, LPARAM lParam)
   {
      LRESULT lResult = 0;
      POINT point;
      eHOVERTAB eHoverTab = kHOVERTAB_None;
      int nTabIx_Hover = -1;

      if (m_nTabCount > 0)
      {
         point.x = GET_X_LPARAM (lParam);
         point.y = GET_Y_LPARAM (lParam);

         if (TabPointHitTest (point, eHoverTab, nTabIx_Hover))
         {
            if (eHoverTab == kHOVERTAB_BtnAdd)
            {
               FrameTabAdd ();
               InvalidateRect (m_hWnd, NULL, FALSE);
            }
            else if (eHoverTab == kHOVERTAB_BtnClose && nTabIx_Hover >= 0)
            {
               FrameTabClose (nTabIx_Hover);
               InvalidateRect (m_hWnd, NULL, FALSE);
            }
            else if (eHoverTab == kHOVERTAB_Body && nTabIx_Hover >= 0)
            {
               FrameTabActive (nTabIx_Hover, true);
               InvalidateRect (m_hWnd, NULL, FALSE);
            }

            bDefWndProc = false;
         }
      }

      return lResult;
   }

   // The canvas SDL child is WS_EX_NOACTIVATE, so clicks there never take
   // keyboard focus off the URL EDIT. PARENTNOTIFY fires for child button
   // downs -- pull focus back to the frame (unless the click was on the EDIT
   // itself) so WASD / other viewport keys work again.
   LRESULT onParentNotify (bool& bDefWndProc, WPARAM wParam, LPARAM lParam)
   {
      LRESULT lResult = 0;
      UINT uEvent = LOWORD (wParam);

      if ((uEvent == WM_LBUTTONDOWN  ||  uEvent == WM_RBUTTONDOWN  ||  uEvent == WM_MBUTTONDOWN)
          &&  m_hEdit
          &&  GetFocus () == m_hEdit
          &&  reinterpret_cast<HWND>(lParam) != m_hEdit)
      {
         SetFocus (m_hWnd);
      }

      return lResult;
   }

   LRESULT onNCMouseMove (bool& bDefWndProc, WPARAM wParam, LPARAM lParam)
   {
      LRESULT lResult = 0;
      POINT point;

      if (m_nTabCount > 0)
      {
         GetCursorPos (&point);
         ScreenToClient (m_hWnd, &point);
         UpdateHoverState (point);
      }

      return lResult;
   }

   LRESULT onNCLButtonDown (bool& bDefWndProc, WPARAM wParam, LPARAM lParam)
   {
      LRESULT lResult = 0;

      if (m_eHoverBtn != kHOVERBTN_None)
      {
         bDefWndProc = false;
      }

      return lResult;
   }

   LRESULT onNCLButtonUp (bool& bDefWndProc, WPARAM wParam, LPARAM lParam)
   {
      LRESULT lResult = 0;

      bDefWndProc = false;
      switch (m_eHoverBtn)
      {
      case kHOVERBTN_Close:           PostMessageW (m_hWnd, WM_CLOSE, 0, 0);                                break;
      case kHOVERBTN_Minimize:        ShowWindow (m_hWnd, SW_MINIMIZE);                                     break;
      case kHOVERBTN_Maximize:        ShowWindow (m_hWnd, IsWindowMax (m_hWnd) ? SW_NORMAL : SW_MAXIMIZE);  break;
      case kHOVERBTN_None:            bDefWndProc = true;                                                   break;
      }

      return lResult;
   }

   LRESULT onNCRButtonUp (bool& bDefWndProc, WPARAM wParam, LPARAM lParam)
   {
      LRESULT lResult = 0;

      if (wParam == HTCAPTION)
      {
         BOOL const isMaximized = IsZoomed (m_hWnd);
         MENUITEMINFO menu_item_info = { 0 };

         menu_item_info.cbSize = sizeof (menu_item_info);
         menu_item_info.fMask = MIIM_STATE;

         HMENU hMenu = GetSystemMenu (m_hWnd, false);
         SetMenuItemState (hMenu, &menu_item_info, SC_RESTORE, isMaximized);
         SetMenuItemState (hMenu, &menu_item_info, SC_MOVE, !isMaximized);
         SetMenuItemState (hMenu, &menu_item_info, SC_SIZE, !isMaximized);
         SetMenuItemState (hMenu, &menu_item_info, SC_MINIMIZE, true);
         SetMenuItemState (hMenu, &menu_item_info, SC_MAXIMIZE, !isMaximized);
         SetMenuItemState (hMenu, &menu_item_info, SC_CLOSE, true);

         WPARAM nMenuItemId = TrackPopupMenu (hMenu, TPM_RETURNCMD, GET_X_LPARAM (lParam), GET_Y_LPARAM (lParam), 0, m_hWnd, NULL);
         if (nMenuItemId != 0)
         {
            PostMessage (m_hWnd, WM_SYSCOMMAND, nMenuItemId, 0);
         }
      }

      return lResult;
   }

   LRESULT onDrawItem (bool& bDefWndProc, WPARAM wParam, LPARAM lParam)
   {
      LRESULT lResult = 0;
      DRAWITEMSTRUCT* pDIS = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);

      if (pDIS  &&  pDIS->hwndItem == m_hwndSetting)
      {
         HDC      hdc        = pDIS->hDC;
         RECT     rc         = pDIS->rcItem;
         UINT     uDpi       = GetDpiForWindow (m_hWnd);
         COLORREF bg         = m_bBtnEllipsisHover ? RGB (210, 212, 215) : RGB (228, 230, 232);
         HBRUSH   brush      = CreateSolidBrush (bg);
         LOGFONTW lf         = { 0 };
         HFONT    hfntOld    = NULL;
         HFONT    hfntTheme  = NULL;

         FillRect (hdc, &rc, brush);
         DeleteObject (brush);

         if (SystemParametersInfoForDpi (SPI_GETICONTITLELOGFONT, sizeof (lf), &lf, false, uDpi) != FALSE)
         {
            if ((hfntTheme = CreateFontIndirectW (&lf)) != NULL)
               hfntOld = (HFONT)SelectObject (hdc, hfntTheme);
         }

         SetBkMode    (hdc, TRANSPARENT);
         SetTextColor (hdc, RGB (33, 33, 33));
         DrawTextW (hdc, wszEllipsis, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

         if (hfntOld)
         {
            SelectObject (hdc, hfntOld);
            DeleteObject (hfntTheme);
         }

         lResult     = TRUE;
         bDefWndProc = false;
      }

      return lResult;
   }

   LRESULT onSize (bool& bDefWndProc, WPARAM wParam, LPARAM lParam)
   {
      LRESULT lResult = 0;

      HideUrlPopup ();

      if (m_hEdit)
         LayoutEdit ();

      if (m_hwndSetting)
      {
         RECT rcBtn = GetBtnEllipsisRect ();
         MoveWindow (m_hwndSetting, rcBtn.left, rcBtn.top, rcBtn.right - rcBtn.left, rcBtn.bottom - rcBtn.top, TRUE);
      }

      // Ideally, we'd notify every tab that the size changed, and the tab itself would figure out 
      // what was best for it based on the bFinal status. But since the tab currently has no knowledge 
      // of its active status, it can't make that decision on its own. Therefore, we'll only send a 
      // resize notification to the active tab. We absolutely should be telling the tab when it's active.
      if (m_nTabIx_Active >= 0)
         m_pAppFrameTab[m_nTabIx_Active]->Resize (m_hWnd, false);

      return lResult;
   }

   LRESULT onExitSizeMove (bool& bDefWndProc, WPARAM wParam, LPARAM lParam)
   {
      LRESULT lResult = 0;

      for (int n = 0; n < m_nTabCount; n++)
         m_pAppFrameTab[n]->Resize (m_hWnd, true);

      return lResult;
   }

   LRESULT onGetMinMaxInfo (bool& bDefWndProc, WPARAM wParam, LPARAM lParam)
   {
      LRESULT lResult = 0;
      MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lParam);

      mmi->ptMinTrackSize.x = 600;
      mmi->ptMinTrackSize.y = 250;

      return lResult;
   }

   LRESULT onCtlColorEdit (bool& bDefWndProc, WPARAM wParam, LPARAM lParam)
   {
      LRESULT lResult = 0;

      // Paint the URL edit's text on the pill background colour.
      if (reinterpret_cast<HWND>(lParam) == m_hEdit)
      {
         HDC hdc = reinterpret_cast<HDC>(wParam);
         SetBkColor   (hdc, crEditBknd);
         SetTextColor (hdc, RGB (33, 33, 33));
         lResult     = reinterpret_cast<LRESULT>(m_ahBrush[kGDIBRUSH_EditBknd]);
         bDefWndProc = false;
      }

      return lResult;
   }

   LRESULT onEraseBkgnd (bool& bDefWndProc, WPARAM wParam, LPARAM lParam)
   {
      LRESULT lResult = 0;

      return lResult;
   }

   void GetEditUrl (std::string& sUrl)
   {
      int nLength = GetWindowTextLength (m_hEdit);
      char* pszBuffer = new char[nLength + 1];

      GetWindowText (m_hEdit, pszBuffer, nLength + 1);

      sUrl = pszBuffer;

      delete[] pszBuffer;
   }

   void Navigate (const std::string& sUrl)
   {
      if (m_nTabIx_Active >= 0)
      {
         std::string sLocal = sUrl;

         APPNATIVE::GetInstance ()->UrlHistory_Add (sLocal);
         m_pAppFrameTab[m_nTabIx_Active]->Url (sLocal);
      }
   }

   LRESULT onUrlSubmit (bool& bDefWndProc, WPARAM wParam, LPARAM lParam)
   {
      LRESULT     lResult = 0;
      std::string sUrl;

      // A highlighted history row takes precedence over the typed text.
      if (m_bUrlPopupOpen  &&  m_nUrlPopupHover >= 0  &&  m_nUrlPopupHover < static_cast<int>(m_asUrlPopup.size ()))
      {
         sUrl = m_asUrlPopup[m_nUrlPopupHover];
         SetWindowTextA (m_hEdit, sUrl.c_str ());
      }
      else
      {
         GetEditUrl (sUrl);
      }

      HideUrlPopup ();
      Navigate (sUrl);
      SetFocus (m_hWnd);

      return lResult;
   }

   // === URL history dropdown (custom owner-drawn popup window) ===========

   // Number of rows the popup will display for the current history snapshot.
   int UrlPopupVisibleRows () const
   {
      int nRows = static_cast<int>(m_asUrlPopup.size ());

      if (nRows > URL_POPUP_MAX_ROWS)
         nRows = URL_POPUP_MAX_ROWS;

      return nRows;
   }

   void ShowUrlPopup ()
   {
      m_asUrlPopup = APPNATIVE::GetInstance ()->UrlHistory ();

      if (m_hUrlPopup  &&  !m_asUrlPopup.empty ()  &&  m_nUrlRowHeight > 0)
      {
         RECT  rcPill = GetEditRect ();
         POINT ptTopLeft = { rcPill.left, rcPill.bottom };

         ClientToScreen (m_hWnd, &ptTopLeft);

         UINT uDpi    = GetDpiForWindow (m_hWnd);
         int  nGap    = WINUTILS::ScaleDpiEx (3, uDpi);
         int  nWidth  = rcPill.right - rcPill.left;
         int  nHeight = UrlPopupVisibleRows () * m_nUrlRowHeight + 2;

         m_nUrlPopupHover = -1;
         m_bUrlPopupOpen  = true;

         SetWindowPos (m_hUrlPopup, HWND_TOPMOST, ptTopLeft.x, ptTopLeft.y + nGap, nWidth, nHeight,
            SWP_NOACTIVATE | SWP_SHOWWINDOW);
         InvalidateRect (m_hUrlPopup, NULL, TRUE);
      }
   }

   void HideUrlPopup ()
   {
      if (m_bUrlPopupOpen)
      {
         m_bUrlPopupOpen  = false;
         m_nUrlPopupHover = -1;

         if (m_hUrlPopup)
            ShowWindow (m_hUrlPopup, SW_HIDE);
      }
   }

   void UrlPopupCommit (int nRow)
   {
      if (nRow >= 0  &&  nRow < static_cast<int>(m_asUrlPopup.size ()))
      {
         std::string sUrl = m_asUrlPopup[nRow];

         SetWindowTextA (m_hEdit, sUrl.c_str ());
         HideUrlPopup ();
         Navigate (sUrl);
         SetFocus (m_hWnd);   // return keyboard focus to the frame so WASD navigation resumes (the Edit keeps focus otherwise and the canvas is WS_EX_NOACTIVATE)
      }
   }

   // Keyboard arrow navigation in the popup. nDirection is +1 (down) or -1 (up).
   void UrlPopupKey (int nDirection)
   {
      if (!m_bUrlPopupOpen)
      {
         if (nDirection > 0)
         {
            ShowUrlPopup ();

            if (m_bUrlPopupOpen  &&  UrlPopupVisibleRows () > 0)
            {
               m_nUrlPopupHover = 0;
               InvalidateRect (m_hUrlPopup, NULL, TRUE);
            }
         }
      }
      else
      {
         int nRows = UrlPopupVisibleRows ();

         if (nRows > 0)
         {
            int nHover = m_nUrlPopupHover + nDirection;

            if (nHover < 0)
               nHover = 0;
            else if (nHover >= nRows)
               nHover = nRows - 1;

            m_nUrlPopupHover = nHover;
            InvalidateRect (m_hUrlPopup, NULL, TRUE);
         }
      }
   }

   void PaintUrlPopup (HWND hWnd)
   {
      PAINTSTRUCT ps;
      HDC         hdc = BeginPaint (hWnd, &ps);

      if (hdc)
      {
         RECT rcClient = { 0 };
         GetClientRect (hWnd, &rcClient);

         FillRect (hdc, &rcClient, m_ahBrush[kGDIBRUSH_ToolbarBknd]);

         HFONT hfntOld = m_hFontEdit ? (HFONT)SelectObject (hdc, m_hFontEdit) : NULL;
         int   nPad    = UrlPillRadius ();
         int   nRows   = UrlPopupVisibleRows ();

         SetBkMode (hdc, TRANSPARENT);

         for (int n = 0; n < nRows; n++)
         {
            RECT rcRow = { rcClient.left, n * m_nUrlRowHeight, rcClient.right, (n + 1) * m_nUrlRowHeight };

            if (n == m_nUrlPopupHover)
               FillRect (hdc, &rcRow, m_ahBrush[kGDIBRUSH_TitlebarBknd]);

            SetTextColor (hdc, RGB (33, 33, 33));

            RECT rcText = rcRow;
            rcText.left  += nPad;
            rcText.right -= nPad;

            DrawTextA (hdc, m_asUrlPopup[n].c_str (), -1, &rcText,
               DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
         }

         if (hfntOld)
            SelectObject (hdc, hfntOld);

         HPEN   hpenOld = (HPEN)SelectObject (hdc, m_ahPens[kGDIPEN_EditBorder]);
         HBRUSH hbrOld  = (HBRUSH)SelectObject (hdc, GetStockObject (HOLLOW_BRUSH));

         Rectangle (hdc, rcClient.left, rcClient.top, rcClient.right, rcClient.bottom);

         SelectObject (hdc, hbrOld);
         SelectObject (hdc, hpenOld);

         EndPaint (hWnd, &ps);
      }
   }

   void OnUrlPopupMouseMove (HWND hWnd, int nY)
   {
      int nRows = UrlPopupVisibleRows ();
      int nRow  = m_nUrlRowHeight > 0 ? nY / m_nUrlRowHeight : -1;

      if (nRow >= nRows)
         nRow = -1;

      if (nRow != m_nUrlPopupHover)
      {
         m_nUrlPopupHover = nRow;
         InvalidateRect (hWnd, NULL, TRUE);

         TRACKMOUSEEVENT TrackMouseEvent_ = { sizeof (TRACKMOUSEEVENT), TME_LEAVE, hWnd, 0 };
         TrackMouseEvent (&TrackMouseEvent_);
      }
   }

   static void RegisterUrlPopupClass ()
   {
      static bool s_bRegistered = false;

      if (!s_bRegistered)
      {
         WNDCLASSEXW WndClassExW = { 0 };

         WndClassExW.cbSize        = sizeof (WNDCLASSEXW);
         WndClassExW.lpfnWndProc   = UrlPopupProc;
         WndClassExW.hInstance     = GetModuleHandleW (NULL);
         WndClassExW.hCursor       = LoadCursor (NULL, IDC_ARROW);
         WndClassExW.lpszClassName = wszUrlPopupClass;
         WndClassExW.style         = CS_DROPSHADOW;

         RegisterClassExW (&WndClassExW);
         s_bRegistered = true;
      }
   }

   static LRESULT CALLBACK UrlPopupProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
   {
      LRESULT lResult = 0;
      Impl*   pImpl   = reinterpret_cast<Impl*>(GetWindowLongPtr (hWnd, GWLP_USERDATA));

      if (message == WM_CREATE)
      {
         CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
         SetWindowLongPtr (hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreate->lpCreateParams));
      }
      else if (pImpl  &&  message == WM_PAINT)
      {
         pImpl->PaintUrlPopup (hWnd);
      }
      else if (message == WM_ERASEBKGND)
      {
         lResult = 1;
      }
      else if (pImpl  &&  message == WM_MOUSEMOVE)
      {
         pImpl->OnUrlPopupMouseMove (hWnd, static_cast<int>(static_cast<short>(HIWORD (lParam))));
      }
      else if (pImpl  &&  message == WM_MOUSELEAVE)
      {
         pImpl->m_nUrlPopupHover = -1;
         InvalidateRect (hWnd, NULL, TRUE);
      }
      else if (pImpl  &&  message == WM_LBUTTONDOWN)
      {
         int nY   = static_cast<int>(static_cast<short>(HIWORD (lParam)));
         int nRow = pImpl->m_nUrlRowHeight > 0 ? nY / pImpl->m_nUrlRowHeight : -1;
         pImpl->UrlPopupCommit (nRow);
      }
      else
      {
         lResult = DefWindowProc (hWnd, message, wParam, lParam);
      }

      return lResult;
   }

   
   static LRESULT CALLBACK EditSubclassProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR /*uIdSubclass*/, DWORD_PTR dwRefData)
   {
      LRESULT lResult  = 0;
      bool    bHandled = false;
      Impl*   pImpl    = reinterpret_cast<Impl*>(dwRefData);

      if (pImpl && message == WM_ERASEBKGND)
      {
         RECT rcClient = { 0 };

         GetClientRect (hWnd, &rcClient);
         FillRect (reinterpret_cast<HDC>(wParam), &rcClient, pImpl->m_ahBrush[kGDIBRUSH_EditBknd]);
         bHandled = true;
      }
      else if (message == WM_GETDLGCODE)
      {
         return DLGC_WANTALLKEYS; // <-- THIS prevents the beep
      }
      else if (pImpl  &&  message == WM_KEYDOWN)
      {
         if (wParam == VK_RETURN)
         {
            // Post the submit to the frame window the subclass was bound to.
            PostMessage (pImpl->m_hWnd, WM_URL_SUBMIT, 0, 0);
            return 0;
         }
         else if (wParam == VK_DOWN)
         {
            pImpl->UrlPopupKey (+1);
            return 0;
         }
         else if (wParam == VK_UP)
         {
            pImpl->UrlPopupKey (-1);
            return 0;
         }
         else if (wParam == VK_ESCAPE)
         {
            if (pImpl->m_bUrlPopupOpen)
            {
               pImpl->HideUrlPopup ();
               return 0;
            }
         }
      }
      else if (pImpl  &&  message == WM_LBUTTONDOWN)
      {
         // Re-open the dropdown when the URL bar is clicked while it already
         // has focus -- e.g. after Escape dismissed it. When the click is what
         // gives the edit focus, EN_SETFOCUS opens it instead, so skip here to
         // avoid a double-open. Falls through to DefSubclassProc so the caret
         // still positions normally.
         if (GetFocus () == hWnd  &&  !pImpl->m_bUrlPopupOpen)
            pImpl->ShowUrlPopup ();
      }
      else if (message == WM_CHAR)
      {
         // TranslateMessage still synthesizes a WM_CHAR (0x0D / 0x0A) for Enter
         // even though WM_KEYDOWN was handled above. A single-line EDIT calls
         // MessageBeep on a carriage-return char it can't insert, so swallow it.
         if (wParam == VK_RETURN  ||  wParam == '\n')
            return 0;
      }

      if (!bHandled)
         lResult = DefSubclassProc (hWnd, message, wParam, lParam);
      else
         lResult = 1;

      return lResult;
   }

   static LRESULT CALLBACK BtnEllipsisSubclassProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR /*uIdSubclass*/, DWORD_PTR dwRefData)
   {
      Impl* pImpl = reinterpret_cast<Impl*>(dwRefData);

      if (pImpl)
      {
         if (message == WM_MOUSEMOVE  &&  !pImpl->m_bBtnEllipsisHover)
         {
            pImpl->m_bBtnEllipsisHover = true;

            TRACKMOUSEEVENT tme = { sizeof (tme) };
            tme.dwFlags   = TME_LEAVE;
            tme.hwndTrack = hWnd;
            TrackMouseEvent (&tme);

            InvalidateRect (hWnd, NULL, FALSE);
         }
         else if (message == WM_MOUSELEAVE  &&  pImpl->m_bBtnEllipsisHover)
         {
            pImpl->m_bBtnEllipsisHover = false;
            InvalidateRect (hWnd, NULL, FALSE);
         }
      }

      return DefSubclassProc (hWnd, message, wParam, lParam);
   }

public:
   APPFRAME_NATIVE*              m_pAppFrame;
   HINSTANCE                     m_hInst;
   HWND                          m_hWnd;
   HWND                          m_hEdit;          // URL edit field (Chrome-style pill)
   HWND                          m_hUrlPopup;      // owner-drawn history dropdown window
   bool                          m_bUrlPopupOpen;
   int                           m_nUrlPopupHover; // highlighted history row (-1 = none)
   int                           m_nUrlRowHeight;  // pixel height of one history row
   std::vector<std::string>      m_asUrlPopup;     // history snapshot shown while open
   HFONT                         m_hFontEdit;
   HWND                          m_hwndSetting;
   bool                          m_bBtnEllipsisHover;
   LOGGER*                       m_pLogger;
   SNEEZE::ENGINE*               m_pSneeze;

   HMENU                         m_hPopupMenu;
   HMENU                         m_hLogLevelMenu;
   HICON                         m_hTabIcon;

   APPFRAMETAB_NATIVE*           m_pAppFrameTab[TAB_MAX_COUNT];
   int                           m_nTabCount;
   eHOVERBTN                     m_eHoverBtn;
   eHOVERTAB                     m_eHoverTab;
   int                           m_nTabIx_Hover;
   int                           m_nTabIx_Active;

   SNEEZE::CONTEXT::eSESSION     m_eSession;
};

/*******************************************************************************************************************************
**                                                      Class: APPFRAME_NATIVE                                                **
***************************************************************************************************************************** */

APPFRAME_NATIVE::APPFRAME_NATIVE (IAPPWINDOW* pController, SNEEZE::ENGINE* pSneeze, LOGGER* pLogger) :
   APPFRAME (pController, pLogger),
   m_pImpl (new Impl (this, GetModuleHandleA (nullptr), pLogger, pSneeze))
{
}

APPFRAME_NATIVE::~APPFRAME_NATIVE ()
{
   delete m_pImpl;
}

void* APPFRAME_NATIVE::Init (APPFRAME* pAppFrame_From, SNEEZE::CONTEXT::eSESSION eSession)
{
   int nX, nY, nWidth, nHeight;
   bool bMaximized;

   m_pImpl->m_eSession = eSession;

   m_pController->Window_OnCreate (this, pAppFrame_From, nX, nY, nWidth, nHeight, bMaximized);
   if (CreateWindowExA (WS_EX_APPWINDOW, PRODUCT_WINDOW_CLASS, PRODUCT_NAME, WS_THICKFRAME | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_CLIPCHILDREN | WS_VISIBLE, nX, nY, nWidth, nHeight, nullptr, nullptr, m_pImpl->m_hInst, m_pImpl) != NULL)
   {
      if (bMaximized)
         ShowWindow (m_pImpl->m_hWnd, SW_MAXIMIZE);
   }

   return m_pImpl->m_hWnd;
}

void APPFRAME_NATIVE::ProcessInput ()
{
   m_pImpl->ProcessInput ();
}

void* APPFRAME_NATIVE::NativeWindow () const
{
   return m_pImpl->m_hWnd;
}

LRESULT CALLBACK APPFRAME_NATIVE::WndProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
   bool bDefWndProc = true;
   LRESULT lResult = 0;
   APPFRAME_NATIVE::Impl* pImpl;

   if (message == WM_CREATE)
   {
      // This message is invoked during the call to CreateWindowExA, before it returns

      CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);

      pImpl = reinterpret_cast<APPFRAME_NATIVE::Impl*>(pCreate->lpCreateParams);

      SetWindowLongPtr (hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pImpl));

      if (pImpl->Init (hWnd))
      {
         SetWindowPos (hWnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
      }
      else lResult = -1;

      bDefWndProc = false;
   }
   else
   {
      pImpl = reinterpret_cast<APPFRAME_NATIVE::Impl*>(GetWindowLongPtr (hWnd, GWLP_USERDATA));
   }

   if (pImpl != nullptr)
   {
      switch (message)
      {
      case WM_DESTROY:
         SetWindowLongPtr (hWnd, GWLP_USERDATA, 0);
         pImpl->m_pAppFrame->m_pController->Window_OnDestroy (pImpl->m_pAppFrame);
         bDefWndProc = false;
         break;

      case WM_CLOSE:
         DestroyWindow (hWnd);
         bDefWndProc = false;
         break;

      case WM_NCCALCSIZE:           lResult = pImpl->onNCCalcSize      (bDefWndProc, wParam, lParam);         break;
      case WM_ACTIVATE:             lResult = pImpl->onActivate        (bDefWndProc, wParam, lParam);         break;
      case WM_NCHITTEST:            lResult = pImpl->onNCHitTest       (bDefWndProc, wParam, lParam);         break;
      case WM_PAINT:                lResult = pImpl->onPaint           (bDefWndProc, wParam, lParam);         break;
      case WM_MOUSEMOVE:            lResult = pImpl->onMouseMove       (bDefWndProc, wParam, lParam);         break;
      case WM_LBUTTONDOWN:          lResult = pImpl->onLButtonDown     (bDefWndProc, wParam, lParam);         break;
      case WM_PARENTNOTIFY:         lResult = pImpl->onParentNotify    (bDefWndProc, wParam, lParam);         break;
      case WM_NCMOUSEMOVE:          lResult = pImpl->onNCMouseMove     (bDefWndProc, wParam, lParam);         break;
      case WM_NCLBUTTONDOWN:        lResult = pImpl->onNCLButtonDown   (bDefWndProc, wParam, lParam);         break;
      case WM_NCLBUTTONUP:          lResult = pImpl->onNCLButtonUp     (bDefWndProc, wParam, lParam);         break;
      case WM_NCRBUTTONUP:          lResult = pImpl->onNCRButtonUp     (bDefWndProc, wParam, lParam);         break;
      case WM_DRAWITEM:             lResult = pImpl->onDrawItem        (bDefWndProc, wParam, lParam);         break;
      case WM_SIZE:                 lResult = pImpl->onSize            (bDefWndProc, wParam, lParam);         break;
      case WM_EXITSIZEMOVE:         lResult = pImpl->onExitSizeMove    (bDefWndProc, wParam, lParam);         break;
      case WM_GETMINMAXINFO:        lResult = pImpl->onGetMinMaxInfo   (bDefWndProc, wParam, lParam);         break;
      case WM_CTLCOLOREDIT:         lResult = pImpl->onCtlColorEdit    (bDefWndProc, wParam, lParam);         break;
      case WM_ERASEBKGND:           lResult = pImpl->onEraseBkgnd      (bDefWndProc, wParam, lParam);         break;

      case WM_URL_SUBMIT:           lResult = pImpl->onUrlSubmit (bDefWndProc, wParam, lParam);               break;

      case WM_SETCURSOR:
         SetCursor (LoadCursor (NULL, IDC_ARROW));
         break;

      case WM_COMMAND:
         {
            int wmId = LOWORD (wParam);
            int nNotifyCode = HIWORD (wParam);

            switch (wmId)
            {
            case IDC_BTN_ELLIPSIS:
               if (nNotifyCode == BN_CLICKED)
               {
                  pImpl->ShowEllipsisMenu ();
               }
               break;

            case IDC_URL_EDIT:
               if (nNotifyCode == EN_SETFOCUS)
                  pImpl->ShowUrlPopup ();
               else if (nNotifyCode == EN_KILLFOCUS)
                  pImpl->HideUrlPopup ();
               break;

            case IDM_WINDOW_PERSISTENT:
               pImpl->m_pAppFrame->m_pController->Window_OnNew (pImpl->m_pAppFrame, SNEEZE::CONTEXT::kSESSION_PERSISTENT);
               break;

            case IDM_WINDOW_TRANSITORY:
               pImpl->m_pAppFrame->m_pController->Window_OnNew (pImpl->m_pAppFrame, SNEEZE::CONTEXT::kSESSION_TRANSITORY);
               break;

            case IDM_LOG_TRACE:              pImpl->SetLogLevel (LOGGER::kLOGLEVEL_Trace);   break;
            case IDM_LOG_INFO:               pImpl->SetLogLevel (LOGGER::kLOGLEVEL_Info);    break;
            case IDM_LOG_WARNING:            pImpl->SetLogLevel (LOGGER::kLOGLEVEL_Warning); break;
            case IDM_LOG_ERROR:              pImpl->SetLogLevel (LOGGER::kLOGLEVEL_Error);   break;
            case IDM_LOG_OFF:                pImpl->SetLogLevel (LOGGER::kLOGLEVEL_Off);     break;

            case IDM_RELOAD:                 pImpl->Reload (false);                          break;
            case IDM_RELOAD_RESET:           pImpl->Reload (true);                           break;

            case IDM_INSPECTOR_RML:          pImpl->ToggleInspector ();                      break;
            case IDM_RELEASE_NOTES:          pImpl->ShowReleaseNotes ();                     break;
            case IDM_SETTINGS:               pImpl->ShowSettings ();                         break;
            case IDM_DEVELOPER_WINDOW:       pImpl->DeveloperWindow ();                      break;
            case IDM_DEVELOPER_BOUNDINGBOX:  pImpl->DeveloperBoundingBox ();                 break;
            case IDM_CHECK_UPDATE:           pImpl->UpdateCheck ();                          break;

            case IDM_EXIT:
               pImpl->m_pAppFrame->m_pController->Window_OnExit ();
               break;
            }
         }
         break;
      }
   }

   if (bDefWndProc)
      lResult = DefWindowProcW (hWnd, message, wParam, lParam);

   return lResult;
}
