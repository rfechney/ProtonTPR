#!/bin/bash

# Installs ProtonTPR on host
# Tested on Ubuntu 22.04

# Verify executed as root
if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit -1
fi

# Verify that the files aren't already there
if [ -f /usr/bin/ProtonTPR ]; then
    echo "/usr/bin/ProtonTPR already exists"
    exit -1
fi
if [ -f /usr/lib/systemd/system/ProtonTPR.service ]; then
    echo "/usr/lib/systemd/system/ProtonTPR.service already exists"
    exit -1
fi
if [ ! -f ProtonTPR ]; then
    echo "ProtonTPR has not been compiled"
    exit -1
fi

# Add executable to /usr/bin
cp ProtonTPR /usr/bin

# Add service to services
cp service/ProtonTPR.service /usr/lib/systemd/system
systemctl enable ProtonTPR
systemctl start ProtonTPR

# Verify running
systemctl status ProtonTPR.service
