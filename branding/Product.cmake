# Copyright 2026 Metaversal Corporation. All rights reserved.
#
# Product identity for this tree. Artemis overlays this file (and Brand.props)
# and leaves every other source file identical so `git merge` from Rubidium
# does not collide with branding. Keep Brand.props in sync with these values.
# Internal names stay Rubidium: namespace RUBIDIUM, CMake target Rubidium,
# include guards, file layout.

set (PRODUCT_NAME              "Rubidium")
set (PRODUCT_NAME_SETUP        "RubidiumSetup")
set (PRODUCT_DESCRIPTION       "Rubidium Metaverse Browser")
set (PRODUCT_DESCRIPTION_SETUP "Rubidium Setup")
set (PRODUCT_COMPANY           "Metaversal Corporation")
set (PRODUCT_APPDATA_DIR       "Metaversal/Rubidium")
set (PRODUCT_HOME_URL          "https://cdn.rp1.com/fabric/rubidium.msf")
set (PRODUCT_CDN_URL           "https://cdn.rp1.com/rubidium/")
set (PRODUCT_WINDOW_CLASS      "RubidiumFrameClass")
set (PRODUCT_URL_POPUP_CLASS   "RubidiumUrlPopupClass")
set (PRODUCT_MUTEX             "Metaversal.Rubidium.SingleInstance")
set (PRODUCT_BUNDLE_ID         "com.rp1.Rubidium")
set (PRODUCT_SETUP_CLASS       "RubidiumSetupClass")
set (PRODUCT_SETUP_TITLE       "Rubidium Setup")
