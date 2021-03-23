#!/bin/bash
# Check that library exists
if [ ! -f /usr/include/libevdev-1.0/libevdev/libevdev.h ]; then
    echo "install libevdev development library - apt install libevdev-dev"
    exit -1
fi

# Build
g++ -std=c++11 src/protontpr.cpp -I/usr/include/libevdev-1.0 -levdev -o protontpr
