# Copyright 2026 Metaversal Corporation. All rights reserved.
#
# Source from bash scripts:  . "$REPO_ROOT/branding/read-product.sh"
# Sets PRODUCT_NAME, PRODUCT_NAME_SETUP, PRODUCT_CDN_URL.

_product_cmake="${_PRODUCT_CMAKE:-$REPO_ROOT/branding/Product.cmake}"

_product_value () {
   sed -n "s/^set ($1 \"\\(.*\\)\")/\\1/p" "$_product_cmake" | head -n 1
}

PRODUCT_NAME="$(_product_value PRODUCT_NAME)"
PRODUCT_NAME_SETUP="$(_product_value PRODUCT_NAME_SETUP)"
PRODUCT_CDN_URL="$(_product_value PRODUCT_CDN_URL)"

unset -f _product_value
unset _product_cmake
