
#include <csignal>
#include <cerrno>
#include <stdio.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>

// Set up constants for the search algorithm
#define PATH_SIZE 512
#define SEARCH_PATH "/dev/input/by-id"
#define SEARCH_PATTERN "usb-Thrustmaster_T-Pendular-Rudder-event-"

// Handle signals
static volatile bool run = true;
void signalHandler(int dummy) {
    run = false;
}

// Function to find the correct Thrustmaster T-Pendular Rudder device path
// Return status code, zero if good.
int findRealTprDevicePath(/*output*/char* realTprDevicePath) {
	// We need to look in the /dev/input/by-id folder for entries.
	// Open it, and get the names of the entries to check.
	DIR *dir = opendir(SEARCH_PATH);
	if (dir == NULL) 
	{
		// Could not open directory, search fails.
		fprintf(stderr, "Failed to open searhc path %s: %d %s\n", SEARCH_PATH, errno, strerror(errno));
		return -1;
	}

	// For each entry in trhe directory
	struct dirent *entry;
	int count = 0;
	while((entry = readdir(dir))) {
		// Get the file name into a convenience pointer
		const char* filename = entry->d_name;
	
		// Check for pattern match
		if(strstr(filename, SEARCH_PATTERN) == NULL) {
			// No match, skip.
			continue;
		}

		// We have a match, keep it and write it out to the console as logging
		count++;
		strncpy(realTprDevicePath, filename, PATH_SIZE);
		printf("Found %s\n", filename);
	}

	// Close the path.
	closedir(dir);

	// Do a sanity check
	if (count == 0) {
		fprintf(stderr, "No Thrustmaster T-Pendular-Rudder devices detected.\n");
		return -2;
	} else if (count > 1) {
		fprintf(stderr, "Too many Thrustmaster T-Pendular-Rudder devices detected.\n");
		memset(realTprDevicePath, 0, PATH_SIZE);
		return -3;
	}

	// Add the search path back into the string
	snprintf(realTprDevicePath, PATH_SIZE, "%s/%s", SEARCH_PATH, realTprDevicePath);
	return 0;
}

int main(int argc, char** argv) {
	// Set up signal handler
	signal(SIGINT, signalHandler);
	signal(SIGQUIT, signalHandler);
	signal(SIGABRT, signalHandler);
	signal(SIGKILL, signalHandler);
	
	// Work out the TPR device path
	static char realTprDevicePath[PATH_SIZE];
	memset(realTprDevicePath, 0, PATH_SIZE);
	
	// If the user passed an argument, use the first argument as the path.
	if(argc > 1) {
		// Copy it in.
		strncpy(realTprDevicePath, argv[1], PATH_SIZE);
	} else {
		// Try and find a TPR device path by searching for it.
		if(findRealTprDevicePath(realTprDevicePath) != 0) {
			return -1;
		}
	}

	// Get real TPR device
	int realTprFd = open(realTprDevicePath, O_RDONLY|O_NONBLOCK);
	if (realTprFd < 0)
	{
		fprintf(stderr, "Could not open Thrustmaster T-Pendular-Rudder device \"%s\": %d %s\n", realTprDevicePath, errno, strerror(errno));
		return -2;
	}
	
	// Get device
	struct libevdev *realTprDevice;
	int rc = libevdev_new_from_fd(realTprFd, &realTprDevice);
	if (rc < 0)
	{
		fprintf(stderr, "Could not get Thrustmaster T-Pendular-Rudder device \"%s\": %d %s\n", realTprDevicePath, -rc, strerror(-rc));
		close(realTprFd);
		return -3;
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
		fprintf(stderr, "Could not create uinput Thrustmaster T-Pendular-Rudder device \"%s\": %d %s\n", realTprDevicePath, -rc, strerror(-rc));
		libevdev_free(realTprDevice);
		libevdev_free(virtualTprDevice);
		close(realTprFd);
		
		return -4;
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
