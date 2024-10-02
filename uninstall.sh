#!/bin/bash

# Removes ProtonTPR from host
# Tested on Ubuntu 22.04

# Verify executed as root
if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit -1
fi

# Remove service from services
systemctl stop ProtonTPR
systemctl disable ProtonTPR
rm /usr/lib/systemd/system/ProtonTPR.service

# Remove executable from /usr/bin
rm /usr/bin/ProtonTPR


