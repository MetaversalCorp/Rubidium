// Copyright 2026 Metaversal Corporation. All rights reserved.
// Inspector RCSS stylesheet — included by InspectorRml.cpp

R"css(
body
{
   font-family: [{FONT-FAMILY}];
   font-size: 12dp;
   background: #ffffff;
   color: #555555;
   margin: 0;
   padding: 0;
   width: 100%;
   height: 100%;
   position: relative;
}

div.tabbar
{
   display: block;
   width: 100%;
   height: 26dp;
   background: #EDF2FA;
   border-bottom-width: 1px;
   border-bottom-color: #D4E3FD;
   padding-left: 3dp;
   box-sizing: border-box;
}

div.tab
{
   display: inline-block;
   height: 26dp;
   box-sizing: border-box;
   padding: 5dp 10dp 8dp 10dp;
   vertical-align: top;
   font-weight: bold;
   cursor: pointer;
}

div.tab:hover
{
   background: #E1E6ED;
}

div.tab.active
{
   color: #0B57D0;
   border-bottom-width: 2dp;
   border-bottom-color: #0B57D0;
}

div.panel
{
   display: none;
   position: absolute;
   top: 26dp;
   left: 0;
   right: 0;
   bottom: 0;
   box-sizing: border-box;
}

div.panel.active
{
   display: flex;
   flex-direction: column;
}

div.placeholder
{
   display: block;
   width: 100%;
   padding-top: 80dp;
   text-align: center;
   color: #999999;
   font-size: 14dp;
}

div.placeholder-icon
{
   display: block;
   width: 100%;
   text-align: center;
   font-family: "Material Symbols Outlined";
   font-size: 48dp;
   color: #D4E3FD;
   padding-bottom: 8dp;
}

div.panel-toolbar
{
   display: block;
   flex-shrink: 0;
   background: #ffffff;
   border-bottom-width: 1px;
   border-bottom-color: #D4E3FD;
   box-sizing: border-box;
}

div.toolbar-row
{
   display: flex;
   align-items: center;
   min-height: 26dp;
   padding: 5dp 4dp;
   box-sizing: border-box;
}

div.toolbar-row.toolbar-settings-2
{
   min-height: 52dp;
}

div.toolbar-row.toolbar-settings-3
{
   min-height: 78dp;
}

div.toolbar-btn
{
   display: inline-block;
   width: 20dp;
   height: 20dp;
   text-align: center;
   padding-top: 2dp;
   cursor: pointer;
   border-radius: 2dp;
   font-family: "Material Symbols Outlined";
   font-size: 16dp;
   box-sizing: border-box;
   vertical-align: middle;
}

div.toolbar-btn:hover
{
   background: #d3e3fd;
   color: #202124;
}

div.toolbar-btn.active
{
   color: #ea4335;
}

div.toolbar-sep
{
   display: inline-block;
   width: 1px;
   height: 13dp;
   background: #D4E3FD;
   margin: 0 4dp;
   vertical-align: middle;
}

div.toolbar-chk
{
   display: inline-block;
   padding: 1dp 5dp;
   cursor: pointer;
   vertical-align: middle;
}

div.toolbar-chk:hover
{
   color: #202124;
}

div.toolbar-chk.checked
{
   color: #0B57D0;
}

div.toolbar-lbl
{
   display: inline-block;
   padding: 1dp 5dp;
   color: #3c4043;
   vertical-align: middle;
}

/* Console "Levels" filter dropdown. The wrap is the positioned anchor; the menu
   floats below the label (z-index lifts it over the entries list). */
div.levels-wrap
{
   position: relative;
   display: inline-block;
   vertical-align: middle;
}

div.toolbar-lbl.toolbar-lbl-click
{
   cursor: pointer;
}

div.toolbar-lbl.toolbar-lbl-click:hover
{
   color: #3c4043;
   background: #F2F2F2
}

div.levels-menu
{
   display: none;
   position: absolute;
   top: 100%;
   left: 0;
   min-width: 150dp;
   background: #ffffff;
   border-width: 1px;
   border-color: #D4E3FD;
   border-radius: 4dp;
   padding: 4dp 0;
   z-index: 50;
   box-sizing: border-box;
}

div.levels-menu.visible
{
   display: block;
}

div.levels-item
{
   display: block;
   padding: 3dp 10dp;
   cursor: pointer;
   color: #3c4043;
   white-space: nowrap;
}

div.levels-item:hover
{
   background: #d3e3fd;
   color: #202124;
}

span.levels-check
{
   display: inline-block;
   width: 18dp;
   font-family: "Material Symbols Outlined";
   font-size: 14dp;
   color: #00000000;
   vertical-align: middle;
}

div.levels-item.checked span.levels-check
{
   color: #0B57D0;
}

div.toolbar-collapsible
{
   display: none;
}

div.toolbar-collapsible.visible
{
   display: flex;
   border-top-width: 1px;
   border-top-color: #D4E3FD;
}

input.filter-input
{
   height: 22dp;
   padding: 1dp 4dp;
   border-width: 1px;
   border-color: #D4E3FD;
   box-sizing: border-box;
}

div.filter-type-btn
{
   display: inline-block;
   padding: 3dp 7dp;
   cursor: pointer;
   border-radius: 2dp;
}

div.filter-type-btn:hover
{
   background: #d3e3fd;
   color: #202124;
}

div.filter-type-btn.active
{
   color: #0B57D0;
   font-weight: bold;
}

div.perf-metrics
{
   display: block;
   flex-shrink: 0;
   height: 52dp;
   padding: 8dp 12dp;
   background: #F8FAFD;
   border-bottom-width: 1px;
   border-bottom-color: #D4E3FD;
   box-sizing: border-box;
}

div.metric-card
{
   display: inline-block;
   width: 100dp;
   height: 36dp;
   margin-right: 12dp;
   padding: 4dp 8dp;
   background: #ffffff;
   border-width: 1px;
   border-color: #D4E3FD;
   border-radius: 4dp;
   box-sizing: border-box;
   vertical-align: top;
}

div.metric-label
{
   display: block;
   font-size: 9dp;
   color: #808689;
}

div.metric-value
{
   display: block;
   font-size: 14dp;
   font-weight: bold;
   color: #202124;
}

div.perf-chart
{
   display: block;
   flex-shrink: 0;
   height: 80dp;
   background: #F8FAFD;
   border-bottom-width: 1px;
   border-bottom-color: #D4E3FD;
   box-sizing: border-box;
   overflow: hidden;
}

div.perf-log
{
   display: block;
   flex-grow: 1;
   min-height: 0;
   overflow: auto;
   padding: 4dp 8dp;
   font-family: "JetBrains Mono";
   font-size: 11dp;
   color: #555555;
}

div.log-line
{
   display: block;
   padding: 1dp 0;
}

div.panel-tabbar
{
   display: block;
   height: 26dp;
   flex-shrink: 0;
   background: #F1F3F4;
   border-bottom-width: 1px;
   border-bottom-color: #D4E3FD;
   padding-left: 3dp;
   box-sizing: border-box;
}

div.panel-content
{
   display: block;
   flex-grow: 1;
   min-height: 0;
   overflow: auto;
}

div.split-left
{
   display: flex;
   flex-direction: column;
   position: absolute;
   top: 0;
   /* Leaves room for the 180dp Origins sidebar (div.panel-sidebar) on the left. */
   left: 180dp;
   right: 40%;
   bottom: 0;
   overflow: hidden;
   box-sizing: border-box;
}

div.split-right
{
   display: block;
   position: absolute;
   top: 0;
   right: 0;
   bottom: 0;
   width: 40%;
   background: #ffffff;
   border-left-width: 1px;
   border-left-color: #D4E3FD;
   overflow: hidden;
   box-sizing: border-box;
}

div.panel-bottombar
{
   display: block;
   flex-shrink: 0;
   height: 26dp;
   background: #F8FAFD;
   border-top-width: 1px;
   border-top-color: #D4E3FD;
   padding: 5dp 5dp;
   box-sizing: border-box;
   overflow: hidden;
}

div.panel-searchbar
{
   display: none;
   flex-shrink: 0;
   height: 26dp;
   background: #ffffff;
   border-top-width: 1px;
   border-top-color: #D4E3FD;
   padding: 3dp 5dp;
   box-sizing: border-box;
}

div.panel-searchbar.visible
{
   display: block;
}

div.panel-searchbar-close
{
   display: inline-block;
   width: 20dp;
   height: 20dp;
   text-align: center;
   padding-top: 2dp;
   cursor: pointer;
   border-radius: 2dp;
   font-family: "Material Symbols Outlined";
   font-size: 16dp;
   box-sizing: border-box;
   vertical-align: middle;
}

div.panel-searchbar-close:hover
{
   background: #d3e3fd;
   color: #202124;
}

div.network-waterfall
{
   display: block;
   flex-shrink: 0;
   height: 80dp;
   background: #ffffff;
   border-bottom-width: 1px;
   border-bottom-color: #D4E3FD;
   box-sizing: border-box;
   overflow: hidden;
   position: relative;
}

div.waterfall-header
{
   display: block;
   height: 26dp;
   background: #ffffff;
   color: #808689;
   padding: 5dp 5dp;
   box-sizing: border-box;
}

div.waterfall-bars
{
   display: block;
   position: absolute;
   top: 26dp;
   left: 0;
   right: 0;
   bottom: 0;
   overflow: hidden;
}

div.panel-body
{
   display: block;
   flex-grow: 1;
   min-height: 0;
   position: relative;
   background: #ffffff;
   overflow: hidden;
   box-sizing: border-box;
}

div.panel-sidebar
{
   display: block;
   position: absolute;
   top: 0;
   left: 0;
   width: 180dp;
   bottom: 0;
   background: #ffffff;
   border-right-width: 1px;
   border-right-color: #D4E3FD;
   overflow: hidden;
   box-sizing: border-box;
}

div.container-item
{
   display: block;
   padding: 3dp 5dp;
   cursor: pointer;
}

div.container-item:hover
{
   background: #d3e3fd;
}

div.container-item.selected
{
   background: #D3E3FD;
   color: #0B57D0;
}

div.panel-main
{
   display: block;
   position: absolute;
   top: 0;
   left: 180dp;
   right: 0;
   bottom: 0;
   background: #ffffff;
   overflow: hidden;
   box-sizing: border-box;
}

/* Network tab: wider Origins sidebar (14% wider than the shared 180dp), with
   origin names clipped instead of word-wrapped. Scoped by element id so the
   other tabs' sidebars keep the shared 180dp width. */
#network-containers.panel-sidebar
{
   width: 205dp;
}

#network-files.panel-main
{
   left: 205dp;
}

#network-containers div.container-item
{
   white-space: nowrap;
   overflow: hidden;
}

/* Console tab: Origins sidebar 10% wider than the shared 180dp, origin names clipped. */
#console-containers.panel-sidebar
{
   width: 198dp;
}

#console-entries.panel-main
{
   left: 198dp;
}

#console-containers div.container-item
{
   white-space: nowrap;
   overflow: hidden;
}

/* Elements tab: Origins sidebar 10% wider (180dp -> 198dp); Details section
   10% smaller (40% -> 36%). The tree (split-left) seam moves to match so it
   fills the space the Details pane gives up. */
#elements-containers.panel-sidebar
{
   width: 198dp;
}

#elements-tree.split-left
{
   left: 198dp;
   right: 36%;
}

#elements-details.split-right
{
   width: 36%;
}

/* --------------------------------------------------------------------------
   Storage tab -- Overview blocks + key/value scope tables.
   -------------------------------------------------------------------------- */

div.kv-section
{
   display: block;
   margin: 0 0 12dp 0;
}

div.kv-section-title
{
   display: block;
   padding: 6dp 10dp;
   font-weight: bold;
   color: #202124;
   background: #F1F3F4;
   border-bottom-width: 1px;
   border-bottom-color: #D4E3FD;
}

div.ov-row
{
   display: flex;
   padding: 4dp 10dp;
   border-bottom-width: 1px;
   border-bottom-color: #F1F3F4;
}

div.ov-key
{
   flex: 0 0 130dp;
   color: #5F6368;
}

div.ov-val
{
   flex: 1 1 auto;
   min-width: 0;
   color: #202124;
   word-break: break-all;
}

div.ov-scope
{
   display: block;
   padding: 6dp 10dp;
   border-bottom-width: 1px;
   border-bottom-color: #F1F3F4;
}

div.ov-scope-name
{
   color: #202124;
   font-weight: bold;
}

div.ov-scope-meta
{
   color: #0B57D0;
}

div.ov-scope-path
{
   color: #9AA0A6;
   font-size: 11dp;
   word-break: break-all;
}

/* Storage scope tabs -- collapsible JSON tree (DevTools-style). */
div.jtree
{
   display: block;
   padding: 4dp 6dp;
   font-size: 12dp;
}

div.jtn
{
   display: block;
}

div.jtn-children
{
   display: block;
   margin-left: 15dp;
}

div.jtn.collapsed div.jtn-children
{
   display: none;
}

div.jtn-row
{
   display: block;
   white-space: nowrap;
   padding: 1dp 2dp;
   cursor: pointer;
}

div.jtn-row:hover
{
   background: #F1F3F4;
}

div.jtn-row.selected
{
   background: #D3E3FD;
}

span.jtn-toggle
{
   display: inline-block;
   width: 11dp;
   height: 11dp;
   margin-right: 3dp;
   line-height: 10dp;
   text-align: center;
   font-size: 11dp;
   color: #5F6368;
   border-width: 1px;
   border-color: #BDC1C6;
   vertical-align: middle;
}

span.jtn-spacer
{
   border-width: 0;
}

span.tg-c
{
   display: none;
}

div.jtn.collapsed span.jtn-toggle span.tg-o
{
   display: none;
}

div.jtn.collapsed span.jtn-toggle span.tg-c
{
   display: inline;
}

span.jtn-icon
{
   display: inline-block;
   margin-right: 4dp;
   vertical-align: middle;
}

span.jtn-obj,
span.jtn-arr
{
   padding: 0 1dp;
   font-size: 9dp;
   color: #80868B;
   border-width: 1px;
   border-color: #DADCE0;
}

span.jtn-icon-leaf
{
   width: 7dp;
   height: 7dp;
   background: #4A86C7;
}

span.jtn-key
{
   color: #202124;
   vertical-align: middle;
}

span.jtn-colon
{
   color: #5F6368;
   vertical-align: middle;
}

span.jtn-val
{
   vertical-align: middle;
}

span.jtn-str  { color: #202124; }
span.jtn-num  { color: #1A73E8; }
span.jtn-bool { color: #C5221F; }
span.jtn-null { color: #9AA0A6; }

div.files-header
{
   display: flex;
   height: 26dp;
   background: #F8FAFD;
   border-bottom-width: 1px;
   border-bottom-color: #D4E3FD;
   /* Right padding reserves the same gutter as the list's scrollbar so the
      header cells line up with the scrolled row cells. */
   padding: 0 12dp 0 0;
   box-sizing: border-box;
   overflow: hidden;
}

div.files-header-cell
{
   flex-grow: 0;
   flex-shrink: 0;
   min-width: 0;
   height: 100%;
   padding: 5dp 5dp;
   overflow: hidden;
   box-sizing: border-box;
}

div.list-body
{
   display: block;
   position: absolute;
   top: 26dp;
   left: 0;
   right: 0;
   bottom: 0;
   overflow: auto;
}

/* The network file list always reserves its scrollbar gutter so the row
   content width is constant whether or not the list is scrolling. The header
   and column separators reserve a matching 12dp gutter so all three stay
   aligned. Scoped to the files list so other (shared) list-body lists keep
   their default auto scrollbar behaviour. */
div.files-body
{
   overflow-x: hidden;
   overflow-y: scroll;
}

/* Scrollbars must have an explicit size. Without it RmlUi lays the scrollbar
   element out at auto width, which fills the whole container and collapses the
   content (client) width to 0 -- squishing every column whenever a list has
   enough rows to scroll. */
scrollbarvertical
{
   width: 12dp;
}

scrollbarhorizontal
{
   height: 12dp;
}
)css"
// Split to stay under MSVC's per-string-literal limit (C2026). Adjacent string
// literals are concatenated by the compiler into one stylesheet.
R"css(
slidertrack
{
   background: #F1F3F4;
}

sliderbar
{
   background: #C4C7C5;
   border-radius: 6dp;
   margin: 2dp;
   min-height: 30dp;
}

sliderbar:hover
{
   background: #A8ABAA;
}

sliderbar:active
{
   background: #909392;
}

sliderarrowdec,
sliderarrowinc
{
   width: 0;
   height: 0;
}

div.files-row
{
   display: flex;
   height: 20dp;
   cursor: pointer;
   padding: 3dp 0;
   box-sizing: border-box;
}

div.files-row:hover,
div.files-row.alt:hover
{
   background: #d3e3fd;
}

div.files-row.alt
{
   background: #F8FAFD;
}

div.files-row.selected
{
   background: #D3E3FD;
   color: #0B57D0;
}

div.files-cell
{
   flex-grow: 0;
   flex-shrink: 0;
   min-width: 0;
   padding: 0 5dp;
   overflow: hidden;
   white-space: nowrap;
   box-sizing: border-box;
}

div.entries-row
{
   display: block;
   height: 20dp;
   cursor: pointer;
   padding: 3dp 0;
   box-sizing: border-box;
}

div.entries-row:hover,
div.entries-row.alt:hover
{
   background: #d3e3fd;
}

div.entries-row.alt
{
   background: #F8FAFD;
}

/* Console severity tints (Chrome DevTools style): error rows get a pink
   background with red text, warning rows a yellow background with dark text.
   Placed before .selected/:hover so selection and hover still take precedence. */
div.entries-row.level-error
{
   background: #fce8e6;
   color: #c5221f;
}

div.entries-row.level-warn
{
   background: #fef7e0;
   color: #5c3c00;
}

div.entries-row.selected
{
   background: #D3E3FD;
   color: #0B57D0;
}

div.entries-cell
{
   display: inline-block;
   padding: 0 5dp;
   overflow: hidden;
   white-space: nowrap;
   box-sizing: border-box;
}

div.entries-col-sep
{
   position: absolute;
   top: 0;
   bottom: 0;
   width: 1px;
   background: #D4E3FD;
}

div.container-header
{
   display: block;
   height: 26dp;
   background: #F8FAFD;
   border-bottom-width: 1px;
   border-bottom-color: #D4E3FD;
   padding: 5dp 5dp;
   box-sizing: border-box;
}

div.files-col-seps
{
   position: absolute;
   top: 0;
   left: 0;
   /* Match the list's reserved scrollbar gutter so the separators line up with
      the columns. The separators inside are positioned as a percentage of this
      container's (gutter-reduced) width. */
   right: 12dp;
   bottom: 0;
   /* Decorative overlay only — must not intercept clicks meant for the file
      rows beneath it (it spans the whole list and is drawn on top). */
   pointer-events: none;
}

div.files-col-sep
{
   position: absolute;
   top: 0;
   bottom: 0;
   width: 1px;
   background: #D4E3FD;
}

div.network-details
{
   display: none;
   position: absolute;
   top: 0;
   right: 0;
   bottom: 0;
   width: 65%;
   background: #ffffff;
   border-left-width: 1px;
   border-left-color: #D4E3FD;
   overflow: hidden;
   box-sizing: border-box;
}

div.network-details.visible
{
   display: block;
}

div.detail-tabbar
{
   display: block;
   position: absolute;
   top: 0;
   left: 0;
   right: 0;
   height: 26dp;
   background: #EDF2FA;
   border-bottom-width: 1px;
   border-bottom-color: #D4E3FD;
   padding-left: 0;
   box-sizing: border-box;
}

div.detail-close
{
   display: inline-block;
   width: 26dp;
   height: 26dp;
   text-align: center;
   padding-top: 5dp;
   cursor: pointer;
   box-sizing: border-box;
}

div.detail-close:hover
{
   color: #202124;
   background: #d3e3fd;
}


div.detail-content
{
   display: block;
   position: absolute;
   top: 26dp;
   left: 0;
   right: 0;
   bottom: 0;
   padding: 5dp 8dp;
   overflow: auto;
}

div.detail-section
{
   display: block;
   font-weight: bold;
   color: #202124;
   padding: 4dp 8dp;
   margin: 4dp -8dp 2dp -8dp;
   background: #F1F3F4;
   border-bottom-width: 1px;
   border-bottom-color: #E0E0E0;
   border-top-width: 1px;
   border-top-color: #E0E0E0;
   cursor: pointer;
}

div.detail-section-body
{
   display: block;
}

span.section-arrow
{
   display: inline-block;
   font-family: "Material Symbols Outlined";
   font-size: 16dp;
   vertical-align: middle;
}

div.detail-section.collapsed span.section-arrow
{
   transform: rotate(-90deg);
}

div.detail-kv
{
   display: block;
   padding: 1dp 0;
}

div.detail-key
{
   display: inline-block;
   width: 190dp;
}

div.detail-value
{
   display: inline-block;
   color: #202124;
}

div.detail-url
{
   display: inline-block;
   color: #1a73e8;
}

div.detail-preview-body
{
   display: block;
   padding: 3dp 0;
}

/* Live glb preview. INSPECTOR_RML docks a native render window over this block,
   so it only needs to reserve height; the centred text is a fallback shown
   underneath if the render window fails to come up. */
div.detail-preview-3d
{
   display: block;
   height: 360dp;
   background-color: #ffffff;
   color: #9aa0a6;
   text-align: center;
   padding-top: 16dp;
}

/* Pretty-printed JSON preview -- monospace so indentation and columns align. */
div.detail-json
{
   font-family: "JetBrains Mono";
   font-size: 11dp;
}

img.preview-image
{
   display: block;
   max-width: 100%;
   margin: 4dp 0;
}

div.detail-response-body
{
   display: none;
}

div.hex-toolbar
{
   display: block;
   position: absolute;
   top: 0;
   left: 0;
   right: 0;
   height: 80dp;
   padding: 4dp 8dp;
   background: #F1F3F4;
   border-bottom-width: 1px;
   border-bottom-color: #E0E0E0;
   box-sizing: border-box;
   text-align: center;
}

div.hex-tb-btn
{
   display: inline-block;
   width: 20dp;
   height: 18dp;
   text-align: center;
   cursor: pointer;
   color: #5F6368;
}

div.hex-tb-btn:hover
{
   color: #202124;
}

div.hex-tb-addr
{
   display: inline-block;
   font-family: JetBrains Mono;
   font-size: 11dp;
   padding: 1dp 8dp;
   color: #202124;
}

div.hex-area
{
   display: flex;
   flex-direction: row;
   position: absolute;
   top: 80dp;
   left: 0;
   right: 0;
   bottom: 0;
   font-family: JetBrains Mono;
   font-size: 11dp;
   overflow: auto;
   white-space: nowrap;
}

div.hex-col-addr
{
   display: block;
   flex-grow: 0;
   flex-shrink: 0;
   flex-basis: 20%;
   min-width: 0;
   box-sizing: border-box;
   overflow: hidden;
   text-align: right;
   background: #EDF2FA;
   color: #5F6368;
   padding: 2dp 4dp;
   padding-top: 5dp;
   border-left-width: 1px;
   border-left-color: transparent;
}

div.hex-col-bytes
{
   display: block;
   flex-grow: 0;
   flex-shrink: 0;
   flex-basis: 50%;
   min-width: 0;
   box-sizing: border-box;
   overflow: hidden;
   color: #202124;
   padding: 2dp 0;
   border-left-width: 1px;
   border-left-color: #E0E0E0;
   padding-top: 5dp;
}

div.hex-col-chars
{
   display: block;
   flex-grow: 0;
   flex-shrink: 0;
   flex-basis: 30%;
   min-width: 0;
   box-sizing: border-box;
   overflow: hidden;
   color: #5F6368;
   padding: 2dp 4dp;
   padding-top: 5dp;
   border-left-width: 1px;
   border-left-color: #E0E0E0;
}

div.hex-line
{
   display: block;
   height: 16dp;
   padding: 0 4dp;
}

div.hex-line.selected
{
   font-weight: bold;
   color: #0B57D0;
}

div.hex-byte
{
   display: inline-block;
   width: 22dp;
   text-align: center;
   cursor: pointer;
}

div.hex-byte:hover
{
   background: #E8F0FE;
}

div.hex-byte.selected
{
   background: #D3E3FD;
   color: #0B57D0;
   font-weight: bold;
}

div.hex-byte.hex-empty
{
   color: transparent;
   cursor: default;
}

div.hex-char
{
   display: inline-block;
   width: 11dp;
   text-align: center;
   cursor: pointer;
}

div.hex-char:hover
{
   background: #E8F0FE;
}

div.hex-char.selected
{
   background: #D3E3FD;
   color: #0B57D0;
   font-weight: bold;
}

div.detail-timing-body
{
   display: block;
   padding: 3dp 0;
}

div.panel-statusbar
{
   display: block;
   flex-shrink: 0;
   height: 26dp;
   background: #ffffff;
   border-top-width: 1px;
   border-top-color: #D4E3FD;
   padding: 5dp 5dp 5dp 9dp;
   box-sizing: border-box;
}

/* ---------------------------------------------------------------------------
   Elements fabric/node tree
   --------------------------------------------------------------------------- */

div.tree-body
{
   display: block;
   overflow-y: auto;
   overflow-x: hidden;
}

div.tree-row
{
   display: block;
   height: 18dp;
   line-height: 18dp;
   padding-top: 1dp;
   padding-bottom: 1dp;
   padding-right: 6dp;
   white-space: nowrap;
   overflow: hidden;
   cursor: pointer;
   font-size: 12dp;
}

div.tree-row:hover
{
   background: #EDF2FA;
}

div.tree-row.selected
{
   background: #D3E3FD;
}

div.tree-fabric
{
   color: #EE3333;
   font-weight: bold;
}

/* Fabrics outside the selected origin: dimmed to pink, nodes suppressed. */
div.tree-row.tree-fabric-other
{
   color: #EECCCC;
}

div.tree-node
{
   color: #202124;
}

span.tree-toggle
{
   display: inline-block;
   width: 14dp;
   font-family: "Material Symbols Outlined";
   font-size: 16dp;
   vertical-align: middle;
   color: #5f6368;
}

span.tree-toggle.collapsed
{
   transform: rotate(-90deg);
}

/* Leaf rows: reserve the arrow's width but draw nothing, so labels stay aligned. */
span.tree-toggle.leaf
{
   color: transparent;
}
)css"
