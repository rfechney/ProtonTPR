#include <libudev.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <csignal>
#include <fcntl.h>
#include <errno.h>

// Forward declaration of classes
class VirtualDeviceManager;
class DeviceMonitor;

// Global variables
DeviceMonitor* g_deviceMonitor = nullptr;

/**
 * @brief Class to manage the virtual device and its associated real device.
 */
class VirtualDeviceManager {
public:
    VirtualDeviceManager()
        : realDevice(nullptr), virtualDevice(nullptr), realDeviceFd(-1) {}

    ~VirtualDeviceManager() {
        cleanup();
    }

    /**
     * @brief Initialize the virtual device from the real device.
     * @param devnode Device node path of the real device.
     * @return True if initialization is successful, false otherwise.
     */
    bool initialize(const char* devnode);

    /**
     * @brief Forward events from the real device to the virtual device.
     */
    void forward_events();

    /**
     * @brief Clean up resources associated with the virtual and real devices.
     */
    void cleanup();

    /**
     * @brief Get the file descriptor of the real device.
     * @return File descriptor of the real device.
     */
    int get_real_device_fd() const { return realDeviceFd; }

private:
    libevdev* realDevice;
    libevdev_uinput* virtualDevice;
    int realDeviceFd;

    /**
     * @brief Create a virtual device based on the real device's capabilities.
     * @return True if virtual device creation is successful, false otherwise.
     */
    bool create_virtual_device();
};

/**
 * @brief Initialize the virtual device from the real device.
 */
bool VirtualDeviceManager::initialize(const char* devnode) {
    // Open the real device
    realDeviceFd = open(devnode, O_RDONLY | O_NONBLOCK);
    if (realDeviceFd < 0) {
        std::cerr << "Failed to open device: " << devnode << " Error: " << strerror(errno) << std::endl;
        return false;
    }

    // Create a libevdev device from the file descriptor
    int rc = libevdev_new_from_fd(realDeviceFd, &realDevice);
    if (rc < 0) {
        std::cerr << "Failed to init libevdev (" << strerror(-rc) << ")" << std::endl;
        close(realDeviceFd);
        realDeviceFd = -1;
        return false;
    }

    // Create the virtual device
    if (!create_virtual_device()) {
        libevdev_free(realDevice);
        realDevice = nullptr;
        close(realDeviceFd);
        realDeviceFd = -1;
        return false;
    }

    std::cout << "Virtual device created successfully." << std::endl;
    return true;
}

/**
 * @brief Create a virtual device based on the real device's capabilities.
 */
bool VirtualDeviceManager::create_virtual_device() {
    // Create a new evdev instance for the virtual device
    libevdev* tempVirtualDevice = libevdev_new();
    if (!tempVirtualDevice) {
        std::cerr << "Failed to create virtual device" << std::endl;
        return false;
    }

    // Copy device properties from the real device
    libevdev_set_name(tempVirtualDevice, libevdev_get_name(realDevice));
    libevdev_set_id_vendor(tempVirtualDevice, libevdev_get_id_vendor(realDevice));
    libevdev_set_id_product(tempVirtualDevice, libevdev_get_id_product(realDevice));

    // Copy all event types and codes
    for (int type = 0; type <= EV_MAX; ++type) {
        if (libevdev_has_event_type(realDevice, type)) {
            libevdev_enable_event_type(tempVirtualDevice, type);
            for (int code = 0; code <= KEY_MAX; ++code) {
                if (libevdev_has_event_code(realDevice, type, code)) {
                    const struct input_absinfo* abs_info = nullptr;
                    if (type == EV_ABS) {
                        abs_info = libevdev_get_abs_info(realDevice, code);
                        libevdev_enable_event_code(tempVirtualDevice, type, code, abs_info);
                    } else {
                        libevdev_enable_event_code(tempVirtualDevice, type, code, nullptr);
                    }
                }
            }
        }
    }

    // Add a virtual button
    libevdev_enable_event_type(tempVirtualDevice, EV_KEY);
    libevdev_enable_event_code(tempVirtualDevice, EV_KEY, BTN_TRIGGER, nullptr);

    // Create the uinput device
    int rc = libevdev_uinput_create_from_device(tempVirtualDevice, LIBEVDEV_UINPUT_OPEN_MANAGED, &virtualDevice);
    if (rc != 0) {
        std::cerr << "Failed to create virtual uinput device: " << strerror(-rc) << std::endl;
        libevdev_free(tempVirtualDevice);
        return false;
    }

    std::cout << "Virtual device created: " << libevdev_uinput_get_devnode(virtualDevice) << std::endl;

    // Clean up the temporary virtual device
    libevdev_free(tempVirtualDevice);
    return true;
}

/**
 * @brief Forward events from the real device to the virtual device.
 */
void VirtualDeviceManager::forward_events() {
    struct input_event ev;
    int rc = libevdev_next_event(realDevice, LIBEVDEV_READ_FLAG_NORMAL, &ev);
    if (rc == 0) {
        // Write the event to the virtual device
        libevdev_uinput_write_event(virtualDevice, ev.type, ev.code, ev.value);
    } else if (rc == -EAGAIN) {
        // No events available
        return;
    } else if (rc == -ENODEV) {
        // Device has been removed
        std::cerr << "Device disconnected." << std::endl;
        cleanup();
    } else {
        std::cerr << "Error reading from real device: " << strerror(-rc) << std::endl;
        cleanup();
    }
}

/**
 * @brief Clean up resources associated with the virtual and real devices.
 */
void VirtualDeviceManager::cleanup() {
    if (realDeviceFd >= 0) {
        close(realDeviceFd);
        realDeviceFd = -1;
    }
    if (virtualDevice) {
        libevdev_uinput_destroy(virtualDevice);
        virtualDevice = nullptr;
    }
    if (realDevice) {
        libevdev_free(realDevice);
        realDevice = nullptr;
    }
}

/**
 * @brief Class to monitor device connection and events.
 */
class DeviceMonitor {
public:
    DeviceMonitor(const std::string& vendor_id, const std::string& product_id)
        : udev(nullptr), monitor(nullptr), vendorId(vendor_id), productId(product_id), virtualDeviceManager(nullptr) {}

    ~DeviceMonitor() {
        cleanup();
    }

    /**
     * @brief Initialize udev and set up monitoring.
     * @return True if initialization is successful, false otherwise.
     */
    bool initialize();

    /**
     * @brief Start monitoring device events and forward them as necessary.
     */
    void start_monitoring();

    /**
     * @brief Clean up resources.
     */
    void cleanup();

private:
    struct udev* udev;
    struct udev_monitor* monitor;
    std::string vendorId;
    std::string productId;
    VirtualDeviceManager* virtualDeviceManager;

    /**
     * @brief Check if the device is already connected on startup.
     */
    void check_existing_devices();

    /**
     * @brief Set up udev monitoring for device events.
     * @return True if successful, false otherwise.
     */
    bool setup_udev_monitor();

    /**
     * @brief Handle device connection events.
     * @param dev udev device object.
     */
    void handle_device_connected(struct udev_device* dev);

    /**
     * @brief Handle device removal events.
     */
    void handle_device_removed();

    /**
     * @brief Check if the device matches the target vendor and product IDs.
     * @param dev udev device object.
     * @return True if it matches, false otherwise.
     */
    bool is_target_device(struct udev_device* dev);
};

/**
 * @brief Initialize udev and set up monitoring.
 */
bool DeviceMonitor::initialize() {
    udev = udev_new();
    if (!udev) {
        std::cerr << "Failed to initialize udev" << std::endl;
        return false;
    }

    check_existing_devices();

    if (!setup_udev_monitor()) {
        return false;
    }

    return true;
}

/**
 * @brief Clean up resources.
 */
void DeviceMonitor::cleanup() {
    if (virtualDeviceManager) {
        virtualDeviceManager->cleanup();
        delete virtualDeviceManager;
        virtualDeviceManager = nullptr;
    }
    if (monitor) {
        udev_monitor_unref(monitor);
        monitor = nullptr;
    }
    if (udev) {
        udev_unref(udev);
        udev = nullptr;
    }
}

/**
 * @brief Check if the device is already connected on startup.
 */
void DeviceMonitor::check_existing_devices() {
    struct udev_enumerate* enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "input");
    udev_enumerate_scan_devices(enumerate);

    struct udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate);
    struct udev_list_entry* entry = nullptr;

    // Explicit loop replacing udev_list_entry_foreach macro
    for (entry = devices; entry != NULL; entry = udev_list_entry_get_next(entry)) {
        struct udev_device* dev = udev_device_new_from_syspath(udev, udev_list_entry_get_name(entry));

        if (is_target_device(dev)) {
            std::cout << "Device found! (connected on startup)" << std::endl;
            handle_device_connected(dev);
            udev_device_unref(dev);
            break;
        }

        udev_device_unref(dev);
    }

    udev_enumerate_unref(enumerate);
}

/**
 * @brief Set up udev monitoring for device events.
 */
bool DeviceMonitor::setup_udev_monitor() {
    monitor = udev_monitor_new_from_netlink(udev, "udev");
    if (!monitor) {
        std::cerr << "Failed to create udev monitor" << std::endl;
        return false;
    }

    udev_monitor_filter_add_match_subsystem_devtype(monitor, "input", nullptr);
    udev_monitor_enable_receiving(monitor);
    return true;
}

/**
 * @brief Start monitoring device events and forward them as necessary.
 */
void DeviceMonitor::start_monitoring() {
    int udev_fd = udev_monitor_get_fd(monitor);

    while (true) {
        int max_fd = udev_fd;
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(udev_fd, &fds);

        if (virtualDeviceManager && virtualDeviceManager->get_real_device_fd() >= 0) {
            int real_fd = virtualDeviceManager->get_real_device_fd();
            FD_SET(real_fd, &fds);
            if (real_fd > max_fd) {
                max_fd = real_fd;
            }
        }

        int ret = select(max_fd + 1, &fds, nullptr, nullptr, nullptr);

        if (ret > 0) {
            if (FD_ISSET(udev_fd, &fds)) {
                struct udev_device* dev = udev_monitor_receive_device(monitor);

                if (dev) {
                    const char* action = udev_device_get_action(dev);

                    if (strcmp(action, "add") == 0 && is_target_device(dev)) {
                        handle_device_connected(dev);
                    } else if (strcmp(action, "remove") == 0 && is_target_device(dev)) {
                        handle_device_removed();
                    }

                    udev_device_unref(dev);
                }
            }

            if (virtualDeviceManager && virtualDeviceManager->get_real_device_fd() >= 0) {
                int real_fd = virtualDeviceManager->get_real_device_fd();
                if (FD_ISSET(real_fd, &fds)) {
                    virtualDeviceManager->forward_events();
                }
            }
        } else if (ret < 0) {
            if (errno == EINTR) {
                continue; // Interrupted by signal, retry
            }
            std::cerr << "Error in select(): " << strerror(errno) << std::endl;
            break;
        }
    }
}

/**
 * @brief Check if the device matches the target vendor and product IDs.
 */
bool DeviceMonitor::is_target_device(struct udev_device* dev) {
    struct udev_device* parent_dev = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");

    if (parent_dev) {
        const char* vendor = udev_device_get_sysattr_value(parent_dev, "idVendor");
        const char* product = udev_device_get_sysattr_value(parent_dev, "idProduct");

        return (vendor != nullptr && product != nullptr && vendorId == vendor && productId == product);
    }
    return false;
}

/**
 * @brief Handle device connection events.
 */
void DeviceMonitor::handle_device_connected(struct udev_device* dev) {
    std::cout << "Device connected: " << udev_device_get_syspath(dev) << std::endl;

    if (virtualDeviceManager) {
        std::cout << "Device already connected and virtual device exists." << std::endl;
        return;
    }

    // Find the event device node
    char* devnode = nullptr;
    struct udev_enumerate* enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_parent(enumerate, dev);
    udev_enumerate_add_match_subsystem(enumerate, "input");
    udev_enumerate_scan_devices(enumerate);

    struct udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate);
    struct udev_list_entry* entry = nullptr;

    // Explicit loop replacing udev_list_entry_foreach macro
    for (entry = devices; entry != NULL; entry = udev_list_entry_get_next(entry)) {
        const char* path = udev_list_entry_get_name(entry);
        struct udev_device* child_dev = udev_device_new_from_syspath(udev, path);
        const char* child_devnode = udev_device_get_devnode(child_dev);

        if (child_devnode && strstr(child_devnode, "/dev/input/event")) {
            devnode = strdup(child_devnode); // Copy the string
            udev_device_unref(child_dev);
            break;
        }

        udev_device_unref(child_dev);
    }

    udev_enumerate_unref(enumerate);

    if (!devnode) {
        std::cerr << "Unable to get device node. No event device found among child devices." << std::endl;
        return;
    }

    // Initialize the virtual device manager
    virtualDeviceManager = new VirtualDeviceManager();
    if (!virtualDeviceManager->initialize(devnode)) {
        delete virtualDeviceManager;
        virtualDeviceManager = nullptr;
        free(devnode); // Free the allocated string
        return;
    }

    free(devnode); // Free the allocated string after use
}


/**
 * @brief Handle device removal events.
 */
void DeviceMonitor::handle_device_removed() {
    std::cout << "Device removed." << std::endl;

    if (virtualDeviceManager) {
        virtualDeviceManager->cleanup();
        delete virtualDeviceManager;
        virtualDeviceManager = nullptr;
        std::cout << "Virtual device destroyed." << std::endl;
    } else {
        std::cout << "No virtual device to destroy." << std::endl;
    }
}

// Signal handler for clean exit
void signal_handler(int signal) {
    std::cout << "\nSignal " << signal << " received, cleaning up and exiting..." << std::endl;
    if (g_deviceMonitor) {
        g_deviceMonitor->cleanup();
        delete g_deviceMonitor;
        g_deviceMonitor = nullptr;
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <vendor_id> <product_id>" << std::endl;
        return 1;
    }

    std::string vendor_id = argv[1];
    std::string product_id = argv[2];

    // Set up signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Initialize and start the device monitor
    g_deviceMonitor = new DeviceMonitor(vendor_id, product_id);
    if (!g_deviceMonitor->initialize()) {
        delete g_deviceMonitor;
        g_deviceMonitor = nullptr;
        return 1;
    }

    g_deviceMonitor->start_monitoring();

    // Clean up before exiting
    g_deviceMonitor->cleanup();
    delete g_deviceMonitor;
    g_deviceMonitor = nullptr;

    return 0;
}
