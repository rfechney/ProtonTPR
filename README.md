# ProtonTPR
A small repo that allows the Thrustmaster TPR rudders to work with Elite Dangerous under Steam Proton.

Tested on:
Ubuntu 22.04
Linux Mint 22 Wilma

## Installation

Build:
```
./make.sh
```

Install:
```
sudo ./install.sh
```

Uninstall:
```
sudo ./uninstall.sh
```

## Troubleshooting

If it errors out, you can run ProtonTPR directly.

```
systemctl stop protontpr
protontpr

```

If your TPR device isn't discovered, you can use the following in a terminal to find what you have:

```
ls /dev/input/by-id/
```

If you have a good candidate for your device, you can test ProtonTPR with your direct path using:

```protontpr /path/of/your/TPR/device/you/found```

If this works then you can update the service to use strictly only your discovered path.  Edit ./usr/lib/systemd/system/protontpr.service in your ProtonTPR checkout at line 9:

```ExecStart=/usr/bin/protontpr```

Update it to:

```ExecStart=/usr/bin/protontpr /path/of/your/TPR/device/you/found```

Then use the following to reinstall the services:
```
sudo ./uninstall.sh
sudo ./install.sh
```

