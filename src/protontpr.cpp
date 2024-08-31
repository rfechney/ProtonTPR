#include <filesystem>
#include <csignal>
#include <iostream>
#include <fcntl.h>
#include <string.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>

// Set up constants for the search algorithm
const std::string SEARCH_PATH = "/dev/input/by-id";
const std::string SEARCH_PATTERN = "usb-Thrustmaster_T-Pendular-Rudder-event-";

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

// Close the libdev device and/or file descriptor is open
void closeRealTPRDevice(/*Output*/int &realTprFd, /*Outpuit*/libevdev *&realTprDevice) {
	// Free device
	if(realTprDevice != NULL) {
		libevdev_free(realTprDevice);
		realTprDevice = NULL;
	}
	
	// Close file descriptor
	if(realTprFd >= 0) {
		close(realTprFd);
		realTprFd = -1;
	}
}

// Ensure we have a connection to a real TPR.
// If the file descriptor is bad, then the TPR device must be null.
// realTprFd and realTprDevice will be modified 
// Return true if we made a new connection.
bool ensureRealTPRDevice(const std::string &realTprDevicePath, /*Output*/int &realTprFd, /*Output*/libevdev *&realTprDevice) {
	bool result = false;
	
	// Get real TPR device file descriptor if we don't have one
	if(realTprFd < 0) {
		// Ensure everything is closed at this point
		closeRealTPRDevice(realTprFd, realTprDevice);
	
		// Try opening the file.
		realTprFd = open(realTprDevicePath.c_str(), O_RDONLY|O_NONBLOCK);
	}

	// We can only continue the process if the file descriptor is open.
	if(realTprFd >= 0) {
		// Check to see if the file descriptor is ok by checking if we can read from it
		if(fcntl(realTprFd, F_GETFD) == -1 || errno == EBADF) {
			std::cerr << "Thrustmaster T-Pendular-Rudder device file descriptor not readable, possible disconnect: " << realTprDevicePath << std::endl;
			
			// File descriptor is bad, close the real TPD device.
			closeRealTPRDevice(realTprFd, realTprDevice);
			
			// We did not make a new connection
			return false;
		}
		
		// Get device from file descriptor if we don't have one
		if(realTprDevice == NULL) {
			int rc = libevdev_new_from_fd(realTprFd, &realTprDevice);
			if (rc < 0) {
				// We could not open the device.  Close everything.
				closeRealTPRDevice(realTprFd, realTprDevice);
			}
			
			// We made a new connection
			return true;
		}
	}
	
	// We did not make a new connection
	return false;
}

// Handle signals
static volatile bool run = true;
void signalHandler(int dummy) {
    run = false;
}

// Entry point
int main(int argc, char** argv) {
	// Set up signal handler to close the app on these signals
	signal(SIGINT, signalHandler);
	signal(SIGQUIT, signalHandler);
	signal(SIGABRT, signalHandler);
	signal(SIGKILL, signalHandler);
	
	// Work out the TPR device path
	// If the user passed an argument, use the first argument as the path, otherwise search for it.
	std::string realTprDevicePath;
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
	int realTprFd = -1;
	struct libevdev *realTprDevice = NULL;
	ensureRealTPRDevice(realTprDevicePath, realTprFd, realTprDevice);
	if (realTprFd < 0)
	{
		std::cerr << "Could not open file for Thrustmaster T-Pendular-Rudder device " << realTprDevicePath << std::endl;
		closeRealTPRDevice(realTprFd, realTprDevice);
		return -2;
	}
	if (realTprDevice == NULL)
	{
		std::cerr << "Could not open udev entry for Thrustmaster T-Pendular-Rudder device " << realTprDevicePath << std::endl;
		closeRealTPRDevice(realTprFd, realTprDevice);
		return -3;
	}

	// Configure virtual TPR given the device we found.
	// Create device
	struct libevdev* virtualTprDevice = libevdev_new();
	if (virtualTprDevice == NULL)
	{
		std::cerr << "Could not create virtual Thrustmaster T-Pendular-Rudder device " << realTprDevicePath << std::endl;
		closeRealTPRDevice(realTprFd, realTprDevice);
		
		return -4;
	}
	libevdev_set_name(virtualTprDevice, "virtual-T-Pendular-Rudder");
	
	// Configure properties for the virtual device using the properites of the real TPR device
	libevdev_set_id_product(virtualTprDevice, libevdev_get_id_product(realTprDevice));
	libevdev_set_id_vendor(virtualTprDevice, libevdev_get_id_vendor(realTprDevice));
	libevdev_set_id_version(virtualTprDevice, libevdev_get_id_version(realTprDevice) + 1); // Add one for next version

	// To make SDL2 see a proper joystick, we will need at least one button
	// Configure a button as an output on the virtual device.  This is the important part to convince LidSDL2 used by Proton that the TPRs are a
	// joystick and not an accelerometer.
	libevdev_enable_event_type(virtualTprDevice, EV_KEY);
	libevdev_enable_event_code(virtualTprDevice, EV_KEY, BTN_TRIGGER, NULL);
	
	// Copy axis configuration fields from real TPR device to the virtual one
	libevdev_enable_event_code(virtualTprDevice, EV_ABS, ABS_X, libevdev_get_abs_info(realTprDevice, ABS_X));
	libevdev_enable_event_code(virtualTprDevice, EV_ABS, ABS_Y, libevdev_get_abs_info(realTprDevice, ABS_Y));
	libevdev_enable_event_code(virtualTprDevice, EV_ABS, ABS_Z, libevdev_get_abs_info(realTprDevice, ABS_Z));
	
	// We are ready to create the cirtual device in uinput
    	struct libevdev_uinput* virtualTprUinputDevice;
	int rc = libevdev_uinput_create_from_device(virtualTprDevice, LIBEVDEV_UINPUT_OPEN_MANAGED, &virtualTprUinputDevice);
	if (rc < 0)
	{
		std::cerr << "Could not create uinput virtual Thrustmaster T-Pendular-Rudder device " << realTprDevicePath << ":" << -rc << " " << strerror(-rc) << std::endl;
		libevdev_free(virtualTprDevice);
		closeRealTPRDevice(realTprFd, realTprDevice);
		
		return -5;
	}

	// Now we can take data from the real TPRs and issue the same events like they came from our virtual one.
	// Run loop until a quit signal is issued.
	std::cout << "Running virtual Thrustmaster T-Pendular-Rudder " << libevdev_uinput_get_devnode(virtualTprUinputDevice) << std::endl;
	while (run)
	{
		// Make sure we have a connection to our device
		if(ensureRealTPRDevice(realTprDevicePath, realTprFd, realTprDevice)) {
			std::cerr << "Reconnected to Thrustmaster T-Pendular-Rudder device " << realTprDevicePath << std::endl;
		}
		
		// Set a default return code in case we don't set it in this loop.
		rc = -EAGAIN;
	
		// Process events if we have a device.
		if(realTprDevice != NULL) {
			// We have a device, so try and use it.
			// Read from TPR
			struct input_event realEvent;
			rc = libevdev_next_event(realTprDevice, LIBEVDEV_READ_FLAG_NORMAL|LIBEVDEV_READ_FLAG_BLOCKING, &realEvent);
			
			// Pass on successful event if we got one
			if (rc == LIBEVDEV_READ_STATUS_SUCCESS || rc == LIBEVDEV_READ_STATUS_SYNC) {
				// Emit successful event to virtual tpr
				libevdev_uinput_write_event(virtualTprUinputDevice, realEvent.type, realEvent.code, realEvent.value);
			}
		}
	
		// Sleep 1ms if there is nothing to do
		if (rc == -EAGAIN) {
			usleep(1000);
		}
	}

	// Close
	libevdev_free(virtualTprDevice);
	closeRealTPRDevice(realTprFd, realTprDevice);
	std::cout << "Exiting." << std::endl;
	return 0;
}

