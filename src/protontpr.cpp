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

int main(void)
{
	// Set up signal handler
	signal(SIGINT, signalHandler);
	signal(SIGQUIT, signalHandler);
	signal(SIGABRT, signalHandler);
	signal(SIGKILL, signalHandler);

	// Get real TPR device
	// Open device path
	const char* realTprDevicePath = "/dev/input/by-id/usb-Thrustmaster_T-Pendular-Rudder-event-joysitck";
	int realTprFd = open(realTprDevicePath, O_RDONLY|O_NONBLOCK);
	if (realTprFd < 0)
	{
		fprintf(stderr, "Could not open Thrustmaster T-Pendular-Rudder device: %d %s\n", errno, strerror(errno));
		return -1;
	}
	
	// Get device
	struct libevdev *realTprDevice;
	int rc = libevdev_new_from_fd(realTprFd, &realTprDevice);
	if (rc < 0)
	{
		fprintf(stderr, "Could not get Thrustmaster T-Pendular-Rudder device: %d %s\n", -rc, strerror(-rc));
		close(realTprFd);
		return -2;
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
