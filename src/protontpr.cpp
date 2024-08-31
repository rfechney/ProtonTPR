#include <filesystem>
#include <csignal>
#include <iostream>
#include <fcntl.h>
#include <string.h>
#include <unistd.h> // For close and usleep
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>

// Set up constants for the search algorithm
const std::string SEARCH_PATH = "/dev/input/by-id";
const std::string SEARCH_PATTERN = "usb-Thrustmaster_T-Pendular-Rudder-event-";

// Handle signals
static volatile bool run = true;
void signalHandler(int dummy) {
    run = false;
}

// Function to find the correct Thrustmaster T-Pendular Rudder device path
// Return an empty string if nothing found.
std::string findRealTprDevicePath() {
	// We need to look in the SEARCH_PATH folder for entries.
	// For each entry in trhe directory
	std::string result;
	for (const auto& entry : std::filesystem::directory_iterator(SEARCH_PATH)) {
		// Get each element
		std::filesystem::path path = entry.path();
		std::string pathAsString = path.string();
		
		// Check for pattern match
		if(pathAsString.find(SEARCH_PATTERN) == std::string::npos) {
			// No match, skip.
			continue;
		}
		
		// We have a match!
		// We only want to use the first answer we find
		if (result.size() == 0) {
			// Keep this one
			result = pathAsString;
			std::cout << "Found: " << pathAsString << std::endl;
		} else {
			// Log out others
			std::cout << "Also found: " << pathAsString << std::endl;
		}
	}
	
	// Success
	return result;
}

int main(int argc, char** argv) {
	// Set up signal handler
	signal(SIGINT, signalHandler);
	signal(SIGQUIT, signalHandler);
	signal(SIGABRT, signalHandler);
	signal(SIGKILL, signalHandler);
	
	// Work out the TPR device path
	std::string realTprDevicePath;
	
	// If the user passed an argument, use the first argument as the path.
	if(argc > 1) {
		// Copy it in.
		realTprDevicePath = argv[1];
	} else {
		// Try and find a TPR device path by searching for it.
		realTprDevicePath = findRealTprDevicePath();
	}
	
	// Do a sanity check
	if (realTprDevicePath.size() == 0) {
		std::cerr << "No Thrustmaster T-Pendular-Rudder devices supplied as an argument, or discovered in " << SEARCH_PATH << std::endl;
		return -1;
	}

	// Get real TPR device
	int realTprFd = open(realTprDevicePath.c_str(), O_RDONLY|O_NONBLOCK);
	if (realTprFd < 0)
	{
		std::cerr << "Could not open Thrustmaster T-Pendular-Rudder device " << realTprDevicePath << ":" << errno << " " << strerror(errno) << std::endl;
		return -2;
	}
	
	// Get device
	struct libevdev *realTprDevice;
	int rc = libevdev_new_from_fd(realTprFd, &realTprDevice);
	if (rc < 0)
	{
		std::cerr << "Could not get Thrustmaster T-Pendular-Rudder device " << realTprDevicePath << ":" << -rc << " " << strerror(-rc) << std::endl;
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
		std::cerr << "Could not create uinput Thrustmaster T-Pendular-Rudder device " << realTprDevicePath << ":" << -rc << " " << strerror(-rc) << std::endl;
		libevdev_free(realTprDevice);
		libevdev_free(virtualTprDevice);
		close(realTprFd);
		
		return -4;
	}

	// Run loop until the controller is unplugged.
	std::cout << "Running virtual Thrustmaster T-Pendular-Rudder " << libevdev_uinput_get_devnode(virtualTprUinputDevice) << std::endl;
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
	std::cout << "Exiting." << std::endl;
	return 0;
}

