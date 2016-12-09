/* Author:      Yang An Tang
Description: Reads the user given file descriptor
and displays the value of the input event.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <linux/input.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>
#include <signal.h>
#include <sys/ioctl.h> //Used for reading frame buffer
#include <sys/kd.h>    //

//Definition for screen size (dimensions)
#define IMAGE_INTERVAL 1   //Sleep interval to allow full write of image
#define X              380
#define Y              380
#define BITS_PER_PIXEL 24
#define BITS_PER_BYTE  8
#define SCREEN_SIZE    (X * Y * (BITS_PER_PIXEL / BITS_PER_BYTE))

//#define debug   //Disable when release

void handler(int sig) {
	printf("\nexiting...(%d)\n", sig);
}

void perror_exit(char* error) {
	perror(error);
	handler(9);
}

// Setup check
void setup_check(char* argv[]) {
	if (argv[1] == NULL) {
		printf("Usage: %s [image location]\n", argv[0]);
		printf("Please specify the path to directory where images are stored.\n");
	}

	if ((getuid()) != 0) printf("You are not root! This may not work...\n");
}

//Writes bmp image onto frame buffer (fb0)
void show_bitmap(char directory[], const char image[]) {
	int status, image_fd, fb_fd;
	unsigned char buffer[SCREEN_SIZE];
	char pathname[50];
	strcpy(pathname, "\0");
	strcat(pathname, directory);
	strcat(pathname, image);
	printf(pathname);
	printf("\n");

	//Opens the file descriptor for frame buffer
	fb_fd = open("/dev/fb0", O_RDWR);
	if (!fb_fd) {
		printf("Could not open framebuffer.\n");
		exit(1);
	}

#ifdef debug
	printf("fb_fd open! %d\n", fb_fd);
#endif

	//Opens the bitmap image file descriptor
	image_fd = open(pathname, O_RDONLY);
	if (image_fd < 0) {
		printf("Failed to open image: %s\n", image);
		exit(1);
	}

#ifdef debug
	printf("image_fd open! %d\n", image_fd);
#endif 

	//Read bitmap image
	status = read(image_fd, (char*)buffer, SCREEN_SIZE);
	if (status < 0) {
		printf("Failed to read image: %s\n", image);
		exit(1);
	}

#ifdef debug
	printf("image_fd read! %d\n", status);
#endif 

	//Write bitmap onto frame buffer (fb0)
	status = write(fb_fd, (const char*)buffer, SCREEN_SIZE);
	if (status < 0) {
		printf("Failed to write image to buffer.\n");
		exit(1);
	}
	//printf("write done! %d\n", status);
	close(image_fd);
	close(fb_fd);
}

int main(int argc, char *argv[]) {
	//Setup input event variables
	struct input_event ev[64];
	int fd, rd, value;
	size_t size = sizeof(struct input_event);
	char name[256] = "Unknown";
	char *device = NULL;

	//Setup dirent and open needed directories
	DIR *dp;
	char* dir, curr;
	int i, MAX;
	struct dirent **ep = (struct dirent**) malloc(128);
	dir = argv[1];

#ifdef debug
	printf(dir);
	printf("\n");
#endif

	dp = opendir(dir);
	if (dp != NULL) {  //Checks if directory is empty
		i = 0;
		while (ep[i] = readdir(dp)) {   //Skips the first two, . and ..
			i++;
		}
		MAX = i - 1;
		i = 2;
	}
	else closedir(dp);
	printf(ep[i]->d_name);

#ifdef debug
	printf("size = %d\n", size);
	printf("argc = %d\n", argc);

	int j;
	for (j = 0; j <= MAX; j++) printf("ep[%d] = %s\n", j, ep[j]->d_name);
	printf("MAX = %d\n", MAX);
#endif

	setup_check(argv);

	if (argc > 1) device = "/dev/input/event1";

#ifdef debug 
	printf(device);
	printf("\n");
#endif

	//Open Device
	if ((fd = open(device, O_RDONLY)) == -1)
		printf("%s is not a valid device.\n'", device);

	//Print Device Name
	ioctl(fd, EVIOCGNAME(sizeof(name)), name);
	printf("Reading from : %s (%s)\n", device, name);

	//Disable blanking on console
	int vconsole_fd;

	//Open the framebuffer virtual console
	vconsole_fd = open("/dev/tty0", O_RDWR);
	if (!vconsole_fd) {
		printf("Could not open virtual console.\n");
		exit(1);
	}

	//Setting KD_GRAPHICS mode
	if (ioctl(vconsole_fd, KDSETMODE, KD_GRAPHICS)) {
		printf("Could not set virtual console to KD_GRAPHICS mode.\n");
		exit(1);
	}

	close(vconsole_fd);
	//daemon(1,1); //No chdir and stdio close
	printf("Console blanking disabled!\n");

	int flag = 0; //Set flag

	while (1) {
		if ((rd = read(fd, ev, size * 64)) < size) //Raise error if read smaller than buffer size
			perror_exit("read()");

		//Discard values not within threshold
		if (ev[0].value > 50 || ev[0].value < -50) {
			value = ev[0].value;

			//Throw away 2nd value from same read
			if (flag == 1) {
				flag = 0;
				continue;
			}
			else {
				flag++;
			}

		}
		else {
			continue;
		}

#ifdef debug
		printf("output = %d\n", value);
#endif

		show_bitmap(dir, ep[i]->d_name);   //Call fn that drives display

										   //Based on dial output, scroll through images.
		if (value > 50) {   //Anti-Clockwise 
			i--;
			if (i == 1) {
				i = MAX;
			}
		}
		else {   //Clockwise
			i++;
			if (i > MAX) {
				i = 2;
			}
		}

		//sleeps for 1s    
		sleep(IMAGE_INTERVAL);
	}

	closedir(dp);
	free(ep);
	return 0;
}
