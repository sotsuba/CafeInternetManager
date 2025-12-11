#pragma once 

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <cstdint>

using std::string; 

#define PORT 9002
#define BACKLOG 128
#define FRAME_RATE 30
#define JPEG_QUALITY 75

typedef struct client_s {
  int socket_fd;
  struct client_s *next;
} client_t;

// Capture backend detection
typedef enum {
	CAPTURE_UNKNOWN,
	CAPTURE_GRIM,   // Wayland (grim)
	CAPTURE_SCROT,  // X11 (scrot)
	CAPTURE_IMPORT, // X11 fallback (ImageMagick)
	CAPTURE_WSL    // WSL
} capture_backend_t;


enum SessionType {
	SESSION_X11,
	SESSION_WAYLAND_GRIM,
	SESSION_WAYLAND_GNOME,
	SESSION_WSL,
	SESSION_UNKNOWN
};

SessionType check_environment() {
	// Try multiple methods to detect session type
	// sudo doesn't preserve XDG_SESSION_TYPE, so check other indicators
	
	const char* session_type_env = std::getenv("XDG_SESSION_TYPE");
	const char* wayland_display = std::getenv("WAYLAND_DISPLAY");
	const char* display = std::getenv("DISPLAY");
	const char* desktop = std::getenv("XDG_CURRENT_DESKTOP");
	
	std::string session_type = session_type_env ? session_type_env : "";
	
	// If XDG_SESSION_TYPE is not set or is "tty", try to detect from other vars
	if (session_type.empty() || session_type == "tty") {
		if (wayland_display && strlen(wayland_display) > 0) {
			session_type = "wayland";
		} else if (display && strlen(display) > 0) {
			session_type = "x11";
		}
	}
	
	// Still unknown? Try running detection commands
	if (session_type.empty() || session_type == "tty") {
		// Check if we can access Wayland socket
		if (system("test -e \"$XDG_RUNTIME_DIR/wayland-0\" 2>/dev/null") == 0) {
			session_type = "wayland";
		} else if (system("xset q > /dev/null 2>&1") == 0) {
			session_type = "x11";
		}
	}

	if (session_type == "x11") {
		return SESSION_X11;
	} else if (session_type == "wayland") {
		if (desktop && strcmp(desktop, "GNOME") == 0) {
			return SESSION_WAYLAND_GNOME;
		}
		return SESSION_WAYLAND_GRIM;
	}
	
	// Return unknown but don't exit - let capture_screen try all methods
	fprintf(stderr, "Warning: Could not detect session type (got '%s')\n", session_type.c_str());
	fprintf(stderr, "Will try all available capture methods.\n");
	return SESSION_UNKNOWN;
}

std::vector<uint8_t> capture_screen(SessionType session, int &width, int &height) {
	// Use unique temp file based on PID to avoid permission conflicts
	char temp_file[64];
	char png_temp[64];
	snprintf(temp_file, sizeof(temp_file), "/tmp/screen_capture_%d.jpg", getpid());
	snprintf(png_temp, sizeof(png_temp), "/tmp/screen_capture_%d.png", getpid());
	
	char command[1024];
	bool capture_success = false;

	// For unknown session, try all methods
	if (session == SESSION_UNKNOWN) {
		// Try Wayland methods first (more common now)
		if (system("which grim > /dev/null 2>&1") == 0) {
			snprintf(command, sizeof(command),
					 "grim -t jpeg -q %d '%s' 2>/dev/null", JPEG_QUALITY, temp_file);
			capture_success = (system(command) == 0);
		}
		if (!capture_success && system("which gnome-screenshot > /dev/null 2>&1") == 0) {
			snprintf(command, sizeof(command),
					 "gnome-screenshot -f '%s' 2>/dev/null", temp_file);
			capture_success = (system(command) == 0);
		}
		// Try X11 methods
		if (!capture_success && system("which scrot > /dev/null 2>&1") == 0) {
			snprintf(command, sizeof(command),
					 "scrot -o '%s' 2>/dev/null", temp_file);
			capture_success = (system(command) == 0);
		}
		if (!capture_success && system("which import > /dev/null 2>&1") == 0) {
			snprintf(command, sizeof(command),
					 "import -window root -quality %d '%s' 2>/dev/null", JPEG_QUALITY, temp_file);
			capture_success = (system(command) == 0);
		}
	} else if (session == SESSION_X11) {
		// Try scrot first (most common X11 tool)
		if (system("which scrot > /dev/null 2>&1") == 0) {
			snprintf(command, sizeof(command),
					 "scrot -o '%s' 2>/dev/null", temp_file);
			capture_success = (system(command) == 0);
		}
		// Fallback to ImageMagick import
		if (!capture_success && system("which import > /dev/null 2>&1") == 0) {
			snprintf(command, sizeof(command),
					 "import -window root -quality %d '%s' 2>/dev/null", JPEG_QUALITY, temp_file);
			capture_success = (system(command) == 0);
		}
	} else if (session == SESSION_WAYLAND_GNOME || session == SESSION_WAYLAND_GRIM) {
		// Note: GNOME Wayland has strict security - no silent screenshots possible
		// gnome-screenshot will show a flash effect (can't be disabled)
		// grim only works on wlroots compositors (Sway, Hyprland), not GNOME
		
		// Try gnome-screenshot first for GNOME (it works, but shows flash)
		if (system("which gnome-screenshot > /dev/null 2>&1") == 0) {
			// Use PNG format then convert to JPEG for better quality
			snprintf(command, sizeof(command),
					 "gnome-screenshot -f '%s' && "
					 "ffmpeg -y -i '%s' -update 1 -q:v 2 '%s' 2>/dev/null",
					 png_temp, png_temp, temp_file);
			fprintf(stderr, "Running: %s\n", command);
			int ret = system(command);
			fprintf(stderr, "Command returned: %d\n", ret);
			capture_success = (ret == 0);
			unlink(png_temp);
		}
		// Try grim (works on Sway, Hyprland, other wlroots compositors)
		if (!capture_success && system("which grim > /dev/null 2>&1") == 0) {
			snprintf(command, sizeof(command),
					 "grim -t jpeg -q %d '%s' 2>/dev/null", JPEG_QUALITY, temp_file);
			capture_success = (system(command) == 0);
		}
		// Try spectacle for KDE Plasma Wayland
		if (!capture_success && system("which spectacle > /dev/null 2>&1") == 0) {
			snprintf(command, sizeof(command),
					 "spectacle -b -n -o '%s' 2>/dev/null", temp_file);
			capture_success = (system(command) == 0);
		}
		// Try ksnip as another fallback
		if (!capture_success && system("which ksnip > /dev/null 2>&1") == 0) {
			snprintf(command, sizeof(command),
					 "ksnip -f -p '%s' 2>/dev/null", temp_file);
			capture_success = (system(command) == 0);
		}
	} else {
		fprintf(stderr, "Unsupported session type for screen capture.\n");
		return {};
	}

	if (!capture_success) {
		fprintf(stderr, "Screen capture failed. Install one of the following:\n");
		fprintf(stderr, "  Wayland (wlroots/Hyprland/Sway): sudo pacman -S grim\n");
		fprintf(stderr, "  GNOME Wayland: gnome-screenshot (usually pre-installed)\n");
		fprintf(stderr, "  KDE Wayland: spectacle (usually pre-installed)\n");
		fprintf(stderr, "  X11: sudo pacman -S scrot\n");
		return {};
	}

	// Read the captured image file
	FILE *f = fopen(temp_file, "rb");
	if (!f) {
		fprintf(stderr, "Cannot open captured image\n");
		return {};
	}

	// Get file size
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	rewind(f);

	// Read image data
	std::vector<uint8_t> image_data(size);
	size_t read_bytes = fread(image_data.data(), 1, size, f);
	fclose(f);

	if (read_bytes != static_cast<size_t>(size)) {
		fprintf(stderr, "Warning: read %zu bytes, expected %ld\n", read_bytes, size);
	}

	// Get dimensions using file command or ffprobe
	char dimension_cmd[512];
	snprintf(dimension_cmd, sizeof(dimension_cmd),
			 "ffprobe -v error -show_entries stream=width,height "
			 "-of csv=s=x:p=0 '%s' 2>/dev/null",
			 temp_file);
	FILE *dim_fp = popen(dimension_cmd, "r");
	if (dim_fp) {
		if (fscanf(dim_fp, "%dx%d", &width, &height) != 2) {
			// Default fallback dimensions
			width = 1920;
			height = 1080;
		}
		pclose(dim_fp);
	}

	unlink(temp_file);
	return image_data;
}
