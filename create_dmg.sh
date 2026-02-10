#!/bin/bash
set -e

APP_NAME="ThermalCamera"
VERSION="1.0.0"
BUILD_DIR="builddir"
STAGING_DIR="dmg_staging"
DMG_NAME="${APP_NAME}-${VERSION}.dmg"

echo "Building ${APP_NAME} v${VERSION} for macOS..."

# 1. Build Release Configuration
meson setup ${BUILD_DIR} --reconfigure --buildtype=release
meson compile -C ${BUILD_DIR}

# 2. Prepare Staging Directory
rm -rf ${STAGING_DIR}
mkdir -p ${STAGING_DIR}

# Copy .app bundle
cp -R ${BUILD_DIR}/${APP_NAME}.app ${STAGING_DIR}/

# Create Link to Applications
ln -s /Applications ${STAGING_DIR}/Applications

# Copy Documentation
cp README.md ${STAGING_DIR}/README.txt
cp LICENSE ${STAGING_DIR}/LICENSE.txt

# 3. Create DMG
rm -f ${DMG_NAME}
hdiutil create -volname "${APP_NAME} v${VERSION}" -srcfolder ${STAGING_DIR} -ov -format UDZO ${DMG_NAME}

echo "DMG created successfully: ${DMG_NAME}"
