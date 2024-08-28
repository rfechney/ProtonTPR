# ProtonTPR
A small repo that allows the Thrustmaster TPR rudders to work with Elite Dangerous under Steam Proton.

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

If it errors with "Too many Thrustmaster T-Pendular-Rudder devices detected." try the following in a terminal:

```
ls /dev/input/by-id/
```

Then with the name, you need to update ./src/protontpr.cpp line 57:

```    const char* realTprDevicePath = findRealTprDevicePath();```

Update it to:

```    const char* realTprDevicePath = "Name of your TPR device you found";```

Then run:
```
sudo ./uninstall.sh
./make.sh
sudo ./install.sh
```

Instead of searching, it'll look for your particular TPR device name.
