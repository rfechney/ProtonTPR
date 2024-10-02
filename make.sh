#!/bin/bash
# Check that library exists
if [ ! -f /usr/include/libevdev-1.0/libevdev/libevdev.h ]; then
    echo "install libevdev development library - apt install libevdev-dev"
    exit -1
fi

# Build ProtonTPR
g++ -std=c++17 -I /usr/include/libevdev-1.0 src/ProtonTPR.cpp -levdev -ludev -o ProtonTPR

