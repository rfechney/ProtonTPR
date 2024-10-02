#!/bin/bash

# Verify executed as root
if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit -1
fi

./ProtonTPR 044f b68f # TPR id's