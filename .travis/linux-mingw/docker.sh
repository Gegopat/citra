#!/bin/bash -ex

cd /citra

# Override Travis CI unreasonable ccache size
echo 'max_size = 3.0G' > "$HOME/.ccache/ccache.conf"

mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE="$(pwd)/../CMakeModules/MinGWCross.cmake" -DUSE_CCACHE=ON -DCMAKE_BUILD_TYPE=Release -DENABLE_SCRIPTING=ON
make -j4

ccache -s

echo 'Preparing binaries...'
cd ..
mkdir package

QT_PLATFORM_DLL_PATH='/usr/x86_64-w64-mingw32/lib/qt5/plugins/platforms/'
find build/ -name "citra*.exe" -exec cp {} 'package' \;

# Copy Qt plugins
mkdir package/platforms
cp "${QT_PLATFORM_DLL_PATH}/qwindows.dll" package/platforms/
cp -rv "${QT_PLATFORM_DLL_PATH}/../mediaservice/" package/
cp -rv "${QT_PLATFORM_DLL_PATH}/../imageformats/" package/
rm -f package/mediaservice/*d.dll

pip3 install pefile
python3 .travis/linux-mingw/scan_dll.py package/*.exe package/imageformats/*.dll "package/"
