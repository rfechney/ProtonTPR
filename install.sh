#!/bin/bash

# Verify executed as root
if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit -1
fi

# Verify that the files aren't already there
if [ -f /usr/bin/protontpr ]; then
    echo "/usr/bin/protontpr already exists"
    exit -1
fi
if [ -f /etc/udev/rules.d/60-thrustmaster-tpr.rules ]; then
    echo "/etc/udev/rules.d/60-thrustmaster-tpr.rules already exists"
    exit -1
fi
if [ -f /usr/lib/systemd/system/protontpr.service ]; then
    echo "/usr/lib/systemd/system/protontpr.service already exists"
    exit -1
fi
if [ ! -f protontpr ]; then
    echo "protontpr has not been compiled"
    exit -1
fi

# Add executable to /usr/bin
cp protontpr /usr/bin

# Add udev rule to udev
cp etc/udev/rules.d/60-thrustmaster-tpr.rules /etc/udev/rules.d
udevadm control --reload-rules

# Add service to services
cp usr/lib/systemd/system/protontpr.service /usr/lib/systemd/system
systemctl enable protontpr
systemctl start protontpr

# Verify running
systemctl status protontpr.service
