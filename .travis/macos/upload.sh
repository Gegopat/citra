#!/bin/bash -ex

. .travis/common/pre-upload.sh

REV_NAME="citra-macos-${GITDATE}-${GITREV}"
ARCHIVE_NAME="${REV_NAME}.tar.gz"
COMPRESSION_FLAGS="-czvf"

mkdir "$REV_NAME"

cp -r build/bin/citra.app "$REV_NAME"
cp build/bin/citra-room "$REV_NAME"

# Move libraries into folder for deployment
dylibbundler -b -x "${REV_NAME}/citra.app/Contents/MacOS/citra" -cd -d "${REV_NAME}/citra.app/Contents/Frameworks/" -p "@executable_path/../Frameworks/" -of

# Move Qt frameworks into app bundle for deployment
$(brew --prefix)/opt/qt5/bin/macdeployqt "${REV_NAME}/citra.app" -executable="${REV_NAME}/citra.app/Contents/MacOS/citra"

# Make the citra.app application launch a debugging terminal.
# Store away the actual binary
mv ${REV_NAME}/citra.app/Contents/MacOS/citra ${REV_NAME}/citra.app/Contents/MacOS/citra-bin

cat > ${REV_NAME}/citra.app/Contents/MacOS/citra <<EOL
#!/usr/bin/env bash
cd "\`dirname "\$0"\`"
chmod +x citra-bin
open citra-bin --args "\$@"
EOL
# Content that will serve as the launching script for citra (within the .app folder)

# Make the launching script executable
chmod +x ${REV_NAME}/citra.app/Contents/MacOS/citra

# Verify loader instructions
find "$REV_NAME" -exec otool -L {} \;

. .travis/common/post-upload.sh
