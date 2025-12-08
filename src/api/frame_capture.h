#ifndef __frame_capture_h__
#define __frame_capture_h__

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
	const string session_type = std::getenv("XDG_SESSION_TYPE") ? std::getenv("XDG_SESSION_TYPE") : "";

	if (session_type != "x11" && session_type != "wayland") {
		fprintf(stderr, "Unsupported session type: %s\n", session_type.c_str());
		fprintf(stderr, "Only X11 and Wayland are supported.\n");
		exit(1);
	}

	if (session_type == "x11")
		return SESSION_X11;
	else if (session_type == "wayland") 
		if (strcmp(getenv("XDG_CURRENT_DESKTOP"), "GNOME") == 0)
			return SESSION_WAYLAND_GNOME;
		else 
			return SESSION_WAYLAND_GRIM;
	else 
		return SESSION_UNKNOWN;
}

std::vector<uint8_t> capture_screen(SessionType session, int &width, int &height) {
	if (session != SESSION_WAYLAND_GNOME) {
		fprintf(stderr, "Screen capture is only supported in Wayland GNOME sessions.\n");
		return {};
	}

	const char *temp_file = "/tmp/screen_capture.jpg";
	char command[1024];

	// Use gnome-screenshot for Wayland GNOME
	snprintf(command, sizeof(command),
			 "gnome-screenshot -f '%s' 2>/dev/null", temp_file);

	if (system(command) != 0) {
		fprintf(stderr, "Screen capture failed\n");
		return {};
	}

	// Read the JPEG file
	FILE *f = fopen(temp_file, "rb");
	if (!f) {
		fprintf(stderr, "Cannot open captured image\n");
		return {};
	}

	// Get file size
	fseek(f, 0, SEEK_END);
	int size = ftell(f);
	rewind(f);

	// Read JPEG data
	std::vector<uint8_t> jpeg_data(size);
	size_t read_bytes = fread(jpeg_data.data(), 1, size, f);
	if (read_bytes != size) {
		fprintf(stderr, "Warning: read %zu bytes, expected %d\n", read_bytes,
				size);
	}
	fclose(f);

	// Get dimensions from JPEG using ffprobe
	char dimension_cmd[512];
	snprintf(dimension_cmd, sizeof(dimension_cmd),
			 "ffprobe -v error -show_entries stream=width,height "
			 "-of csv=s=x:p=0 '%s' 2>/dev/null",
			 temp_file);
	FILE *dim_fp = popen(dimension_cmd, "r");
	if (dim_fp) {
		if (fscanf(dim_fp, "%dx%d", &width, &height) != 2) {
			// Default fallback dimensions
			width = 1440;
			height = 900;
		}
		pclose(dim_fp);
	}

	unlink(temp_file);
	return jpeg_data;
}

// // Detect available capture backend
// capture_backend_t detect_capture_backend() {
//   // Check if WSL first
//   if (is_wsl()) {
// 	printf("WSL environment detected\n");
// 	return CAPTURE_WSL;
//   }

//   // Check environment first, then find compatible tool
// 	if (getenv("WAYLAND_DISPLAY") != NULL) {
// 		if (system("which grim > /dev/null 2>&1") == 0) 
// 			return CAPTURE_GRIM;

// 		} else if (getenv("DISPLAY") != NULL) {
// 	// We're on X11, find X11-compatible tool
// 	if (system("which scrot > /dev/null 2>&1") == 0) {
// 	  return CAPTURE_SCROT;
// 	}
// 	if (system("which import > /dev/null 2>&1") == 0) {
// 	  return CAPTURE_IMPORT;
// 	}
//   }
//   return CAPTURE_UNKNOWN;
// }


// void init_capture() {
//   // Detect which capture backend to use
//   capture_backend = detect_capture_backend();

//   switch (capture_backend) {
//   case CAPTURE_GRIM: {
// 	printf("Using grim for Wayland screen capture (JPEG output)\n");
// 	// For Hyprland/Wayland, detect the output
// 	FILE *fp = popen("hyprctl monitors -j 2>/dev/null | grep -o "
// 					 "'\"name\":\"[^\"]*' | head -1 | cut -d'\"' -f4",
// 					 "r");
// 	if (fp) {
// 	  char output_name[256];
// 	  if (fgets(output_name, sizeof(output_name), fp)) {
// 		output_name[strcspn(output_name, "\n")] = 0;
// 		if (strlen(output_name) > 0) {
// 		  display_output = strdup(output_name);
// 		  printf("Detected Hyprland output: %s\n", display_output);
// 		}
// 	  }
// 	  pclose(fp);
// 	}
// 	if (!display_output) {
// 	  printf("Using default Wayland display\n");
// 	}
// 	break;
//   }
//   case CAPTURE_SCROT:
// 	printf("Using scrot for X11 screen capture (JPEG output)\n");
// 	break;
//   case CAPTURE_IMPORT:
// 	printf("Using ImageMagick import for X11 screen capture (JPEG output)\n");
// 	break;
//   case CAPTURE_WSL:
// 	fprintf(stderr, "WSL detected but screen capture not supported\n");
// 	fprintf(stderr, "WSL cannot directly capture Windows screen.\n");
// 	fprintf(stderr, "Consider using a Windows-native capture tool with network "
// 					"streaming.\n");
// 	break;
//   case CAPTURE_UNKNOWN:
// 	fprintf(stderr, "No screen capture tool found!\n");
// 	fprintf(stderr, "Install one of the following:\n");
// 	fprintf(stderr, "  Wayland: sudo pacman -S grim\n");
// 	fprintf(stderr, "  X11: sudo pacman -S scrot\n");
// 	fprintf(stderr, "  X11 (fallback): sudo pacman -S imagemagick\n");
// 	exit(1);
//   }
// }

// void capture_screen(unsigned char **data, int *width, int *height, int *size) {
//   static int first_capture = 1;
//   if (first_capture) {
// 	init_capture();
// 	first_capture = 0;
//   }

//   if (capture_backend == CAPTURE_WSL) {
// 	fprintf(stderr, "Screen capture not supported in WSL\n");
// 	*data = NULL;
// 	return;
//   }

//   if (capture_backend == CAPTURE_UNKNOWN) {
// 	*data = NULL;
// 	return;
//   }

//   const char *temp_file = "/tmp/screen_capture.jpg";
//   char command[1024];

//   // Build capture command based on backend
//   switch (capture_backend) {
//   case CAPTURE_GRIM:
// 	// Use grim to capture directly to JPEG
// 	if (display_output) {
// 	  snprintf(command, sizeof(command),
// 			   "grim -o %s -t jpeg -q %d - > '%s' 2>/dev/null", display_output,
// 			   JPEG_QUALITY, temp_file);
// 	} else {
// 	  snprintf(command, sizeof(command),
// 			   "grim -t jpeg -q %d - > '%s' 2>/dev/null", JPEG_QUALITY,
// 			   temp_file);
// 	}
// 	break;

//   case CAPTURE_SCROT:
// 	// Use scrot for X11, then convert to JPEG if needed
// 	snprintf(command, sizeof(command),
// 			 "scrot -o '%s' 2>/dev/null && "
// 			 "convert '%s' -quality %d '%s' 2>/dev/null",
// 			 temp_file, temp_file, JPEG_QUALITY, temp_file);
// 	break;

//   case CAPTURE_IMPORT:
// 	// Use ImageMagick import for X11
// 	snprintf(command, sizeof(command),
// 			 "import -window root -quality %d '%s' 2>/dev/null", JPEG_QUALITY,
// 			 temp_file);
// 	break;

//   default:
// 	*data = NULL;
// 	return;
//   }

//   if (system(command) != 0) {
// 	fprintf(stderr, "Screen capture failed\n");
// 	*data = NULL;
// 	return;
//   }

//   // Read the JPEG file
//   FILE *f = fopen(temp_file, "rb");
//   if (!f) {
// 	fprintf(stderr, "Cannot open captured image\n");
// 	*data = NULL;
// 	return;
//   }

//   // Get file size
//   fseek(f, 0, SEEK_END);
//   *size = ftell(f);
//   rewind(f);

//   // Read JPEG data
//   *data = (unsigned char *)malloc(*size);
//   size_t read_bytes = fread(*data, 1, *size, f);
//   if (read_bytes != *size) {
// 	fprintf(stderr, "Warning: read %zu bytes, expected %d\n", read_bytes,
// 			*size);
//   }
//   fclose(f);

//   // Get dimensions from JPEG using ffprobe
//   char dimension_cmd[512];
//   snprintf(dimension_cmd, sizeof(dimension_cmd),
// 		   "ffprobe -v error -show_entries stream=width,height "
// 		   "-of csv=s=x:p=0 '%s' 2>/dev/null",
// 		   temp_file);
//   FILE *dim_fp = popen(dimension_cmd, "r");
//   if (dim_fp) {
// 	if (fscanf(dim_fp, "%dx%d", width, height) != 2) {
// 	  // Default fallback dimensions
// 	  *width = 1440;
// 	  *height = 900;
// 	}
// 	pclose(dim_fp);
//   }

//   unlink(temp_file);
// }

// #ifdef __cplusplus
// #include <cstdint>
// #include <vector>

// // C++ wrapper to send frame via WebSocket
// inline void capture_and_send_frame_ws(void *sender_ptr) {
//   unsigned char *jpeg_data = NULL;
//   int width, height, size;
//   capture_screen(&jpeg_data, &width, &height, &size);
//   if (!jpeg_data) {
//     return;
//   }

//   // Convert to vector for C++
//   std::vector<uint8_t> frame_bytes(jpeg_data, jpeg_data + size);
//   free(jpeg_data);

//   // Cast and send via WebSocket sender
//   Sender *sender = static_cast<Sender *>(sender_ptr);
//   sender->sendBinary(frame_bytes);

//   printf("JPEG frame sent via WebSocket: %dx%d (%d bytes)\n", width, height,
//          size);
// }
// #endif

#endif
