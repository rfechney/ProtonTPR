
#include <csignal>
#include <cerrno>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>

// Handle signals
static volatile bool run = true;
void signalHandler(int dummy) {
    run = false;
}

// Function to find the correct Thrustmaster T-Pendular Rudder device path
const char* findRealTprDevicePath() {
    static char realTprDevicePath[512];
    const char* devicePathPattern = "usb-Thrustmaster_T-Pendular-Rudder-event-";
    char tempDevicePath[512];
    FILE* fp = popen("ls /dev/input/by-id/ | grep usb-Thrustmaster_T-Pendular-Rudder-event-", "r");

    if (fp == NULL) {
        fprintf(stderr, "Failed to run ls command: %d %s\n", errno, strerror(errno));
        return NULL;
    }

    int count = 0;
    while (fgets(tempDevicePath, sizeof(tempDevicePath)-1, fp) != NULL) {
        count++;
        strncpy(realTprDevicePath, tempDevicePath, sizeof(realTprDevicePath) - 1);
        realTprDevicePath[strcspn(realTprDevicePath, "\n")] = 0; // Remove newline character if present
    }
    pclose(fp);

    if (count == 0) {
        fprintf(stderr, "No Thrustmaster T-Pendular-Rudder devices detected.\n");
        return NULL;
    } else if (count > 1) {
        fprintf(stderr, "Too many Thrustmaster T-Pendular-Rudder devices detected.\n");
        return NULL;
    }

    snprintf(realTprDevicePath, sizeof(realTprDevicePath), "/dev/input/by-id/%s", realTprDevicePath);
    return realTprDevicePath;
}

int main(void) {
    // Set up signal handler
    signal(SIGINT, signalHandler);
    signal(SIGQUIT, signalHandler);
    signal(SIGABRT, signalHandler);
    signal(SIGKILL, signalHandler);

    // Get real TPR device path
    const char* realTprDevicePath = findRealTprDevicePath();
    if (realTprDevicePath == NULL) {
        return -1;
    }

    // Open device
    int realTprFd = open(realTprDevicePath, O_RDONLY|O_NONBLOCK);
    if (realTprFd < 0) {
        fprintf(stderr, "Could not open Thrustmaster T-Pendular-Rudder device: %d %s\n", errno, strerror(errno));
        return -1;
    }

	// Configure virtual TPR
	// Create device
	struct libevdev* virtualTprDevice = libevdev_new();
	libevdev_set_name(virtualTprDevice, "virtual-T-Pendular-Rudder");
	
	// Configure properties
	libevdev_set_id_product(virtualTprDevice, libevdev_get_id_product(realTprDevice));
	libevdev_set_id_vendor(virtualTprDevice, libevdev_get_id_vendor(realTprDevice));
	libevdev_set_id_version(virtualTprDevice, libevdev_get_id_version(realTprDevice) + 1);

	// To make SDL2 see a proper joystick, we will need at least one button
	// Configure buttons as outputs
	libevdev_enable_event_type(virtualTprDevice, EV_KEY);
	libevdev_enable_event_code(virtualTprDevice, EV_KEY, BTN_TRIGGER, NULL);
	
	// Copy axies from real TPR device
	libevdev_enable_event_code(virtualTprDevice, EV_ABS, ABS_X, libevdev_get_abs_info(realTprDevice, ABS_X));
	libevdev_enable_event_code(virtualTprDevice, EV_ABS, ABS_Y, libevdev_get_abs_info(realTprDevice, ABS_Y));
	libevdev_enable_event_code(virtualTprDevice, EV_ABS, ABS_Z, libevdev_get_abs_info(realTprDevice, ABS_Z));
	
	// Create device in uinput
    	struct libevdev_uinput* virtualTprUinputDevice;
	rc = libevdev_uinput_create_from_device(virtualTprDevice, LIBEVDEV_UINPUT_OPEN_MANAGED, &virtualTprUinputDevice);
	if (rc < 0)
	{
		fprintf(stderr, "Could not create uinput Thrustmaster T-Pendular-Rudder device: %d %s\n", -rc, strerror(-rc));
		libevdev_free(realTprDevice);
		libevdev_free(virtualTprDevice);
		close(realTprFd);
		
		return -3;
	}

	// Run loop until the controller is unplugged.
	printf("Running virtual Thrustmaster T-Pendular-Rudder %s\n", libevdev_uinput_get_devnode(virtualTprUinputDevice));
	while (run)
	{
		// Read from TPR
		struct input_event realEvent;
		rc = libevdev_next_event(realTprDevice, LIBEVDEV_READ_FLAG_NORMAL|LIBEVDEV_READ_FLAG_BLOCKING, &realEvent);
		
		// Process sync events
		while (rc == LIBEVDEV_READ_STATUS_SYNC && run) {		
			// Emit sync event to virtual tpr
			libevdev_uinput_write_event(virtualTprUinputDevice, realEvent.type, realEvent.code, realEvent.value);
			
			// Get next event
			rc = libevdev_next_event(realTprDevice, LIBEVDEV_READ_FLAG_SYNC, &realEvent);
		}
		
		// Pass on successful event if we got one
		if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
			// Emit successful event to virtual tpr
			libevdev_uinput_write_event(virtualTprUinputDevice, realEvent.type, realEvent.code, realEvent.value);
		}
		
		// Sleep 1ms if no event
		if (rc == -EAGAIN) {
			usleep(1000);
		}
	}

	// Close
	libevdev_free(realTprDevice);
	libevdev_free(virtualTprDevice);
	close(realTprFd);
	printf("Exiting.\n");
	return 0;
}
