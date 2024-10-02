#!/bin/bash

# Run the uninstall script with sudo
sudo ./uninstall.sh

# Check if the uninstall script was successful
if [ $? -ne 0 ]; then
  echo "Uninstall failed. Exiting."
  exit 1
fi

# Run the make script
./make.sh

# Check if the make script was successful
if [ $? -ne 0 ]; then
  echo "Make failed. Exiting."
  exit 1
fi

# Run the install script
sudo ./install.sh

# Check if the install script was successful
if [ $? -ne 0 ]; then
  echo "Install failed. Exiting."
  exit 1
fi

echo "All scripts ran successfully."
journalctl -u protontpr.service -f
