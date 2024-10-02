# What is this?

This application was built to address a limitation in SDL2, which doesn't detect devices without buttons as game controllers. Specifically, it was developed for the Thrustmaster Pendular Rudder (TPR), a device with 3 axes but no buttons. By adding a virtual button to the device, the application allows it to be detected and used in Linux and Wine games, enabling proper interaction through SDL and Dinput.

# Tested on

## Operating System

* Linux Mint 21.3 (Based on Ubuntu 22.04)

## Devices
Thrustmaster Pendular Rudder (TPR)

# Notes

## Items used to figure out what's going on:

| Code | Function |
| --- | --- |
| ```udevadm monitor --udev``` | Show the device connect/disconnect events |
| ```lspci``` | Show the usb device connections (good to understand tree of usb hubgs connected to usb hubs before devices) |
| ```lsusb``` | Show the currently connected devices and their corresponding vendor + product id's |

## Todo

* Add testing
* Build github actions environments for test and release construction for:
 * Debian
 * Arch
 * Centos/Fedora/Redhat


```mermaid
---
title: Program Flowchart
---
flowchart TD
    A[Start] --> B["Parse command-line arguments<br/>(vendor_id and product_id)"]
    B --> C[Initialize DeviceMonitor<br/>with vendor_id and product_id]
    C --> D[DeviceMonitor initializes udev]
    D --> E[Check for existing devices<br/>matching vendor_id and product_id]
    E --> F{Device Found?}
    F -->|Yes| G[Handle device connected]
    F -->|No| H[Proceed to setup udev monitor]
    G --> I[Create VirtualDeviceManager]
    I --> H
    H --> J[Setup udev monitor<br/>to listen for device events]
    J --> K[Start monitoring loop]
    K --> L["Wait for events using select()"]
    L --> M{Event Type?}
    M -->|Udev Event| N[Handle udev event]
    M -->|Real Device Event| O[Forward events from real device<br/>to virtual device]
    N --> P{Udev Event Action}
    P -->|Add| Q[Check if device matches<br/>vendor_id and product_id]
    P -->|Remove| R[Check if device matches<br/>vendor_id and product_id]
    Q --> S{Device Matches?}
    S -->|Yes| T[Handle device connected]
    S -->|No| L
    R --> U{Device Matches?}
    U -->|Yes| V[Handle device removed]
    U -->|No| L
    T --> W[Create VirtualDeviceManager]
    W --> L
    V --> X[Cleanup VirtualDeviceManager]
    X --> L
    O --> L

```
