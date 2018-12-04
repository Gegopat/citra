#!/bin/bash -ex

cd /citra

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=/usr/lib/ccache/gcc -DCMAKE_CXX_COMPILER=/usr/lib/ccache/g++ -DENABLE_DISCORD_RPC=ON -DENABLE_SCRIPTING=ON
make -j4
