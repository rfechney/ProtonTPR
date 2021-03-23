#!/bin/bash

# Verify executed as root
if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit -1
fi

# Remove service from services
systemctl stop protontpr
systemctl disable protontpr
rm /usr/lib/systemd/system/protontpr.service

# Remove udev rule to udev
rm /etc/udev/rules.d/60-thrustmaster-tpr.rules
udevadm control --reload-rules

# Remove executable to /usr/bin
rm /usr/bin/protontpr


