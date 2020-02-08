/* timeout.c - a little program to blank the RPi touchscreen and unblank it
   on touch.  Original by https://github.com/timothyhollabaugh

   Loosely based on the original. The main difference is that the 
   brightness is progressively reduced to zero.

   On a touch event we reset the brightness to the original value (read when
   the program started.

   Unless you change the permissions in udev this program needs to run as
   root or setuid as root.

   First Written by Dougie Lawson 01-22-2019 
   Forked by Nathan Ramanathan 02-08-2020 +
   (C) Copyright 2019, Dougie Lawson, all right reserved.
*/
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#define fade_amount 1

static FILE* brightfd;
static uint32_t actual_brightness;
static uint32_t max_brightness;
static uint32_t current_brightness;

static void sig_handler(int _);
static void set_screen_brightness(uint32_t brightness);
static uint32_t fast_atoi( const char * str );
static uint32_t GetIdleTime();

long int readint(char * filenm) {
    FILE * filefd;
    filefd = fopen(filenm, "r");
    if (filefd == NULL) {
        int err = errno;
        printf("Error opening %s file: %d", filenm, err);
        exit(1);
    }

    char number[10];
    char * end;
    fscanf(filefd, "%s", & number);
    printf("File: %s ,The number is: %s\n", filenm, number);

    fclose(filefd);
    return strtol(number, & end, 10);
}
///////////////////////////
void sleep_ms(int milliseconds);
#ifdef WIN32
#include <windows.h>
#elif _POSIX_C_SOURCE >= 199309L
#include <time.h>   // for nanosleep
#else
#include <unistd.h> // for usleep
#endif
// void sleep_ms(int milliseconds);
void sleep_ms(int milliseconds) // cross-platform sleep function
{
    #ifdef WIN32
    Sleep(milliseconds);
    #elif _POSIX_C_SOURCE >= 199309L
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
    #else
    usleep(milliseconds * 1000);
    #endif
}
////////////////////////////

static void sig_handler(int _)
{
   (void)_;	
   set_screen_brightness(max_brightness);
   exit(0);
}

static void set_screen_brightness(uint32_t brightness){
    fprintf(brightfd, "%d\n", brightness);
    fflush(brightfd);
    fseek(brightfd, 0, SEEK_SET);
}

static uint32_t fast_atoi( const char * str )
{
    uint32_t val = 0;
    while( *str ) {
        val = val*10 + (*str++ - '0');
    }
    return val;
}

static uint32_t GetIdleTime() {
        uint32_t idle_time;
        FILE *fp;
	char path[1035];
	
	/* Open the command for reading. */
	fp = popen("sudo -u pi env DISPLAY=:0 xprintidle", "r");
	if (fp == NULL) {
	  printf("Failed to run command\n" );
	  exit(1);
	}
	
	/* Read the output a line at a time - output it. */
	while (fgets(path, sizeof(path), fp) != NULL) {
          idle_time = fast_atoi(path);
          if((int)idle_time<0)idle_time=0;
	}
	/* close */
        pclose(fp);
        return idle_time;
}

int main(int argc, char * argv[]) {
    printf("Hello World\n");
    signal(SIGINT, sig_handler);
    if (argc < 2) {
        printf("Usage: timeout <timeout_sec>\n");
        exit(1);
    }
    int i;
    int tlen;
    int timeout;
    tlen = strlen(argv[1]);
    for (i = 0; i < tlen; i++)
        if (!isdigit(argv[1][i])) {
            printf("Entered timeout value is not a number\n");
            exit(1);
        }
    timeout = atoi(argv[1]);

    char actual[53] = "/sys/class/backlight/rpi_backlight/actual_brightness";
    char max[50] = "/sys/class/backlight/rpi_backlight/max_brightness";
    char bright[46] = "/sys/class/backlight/rpi_backlight/brightness";

    brightfd = fopen(bright, "w");
    if (brightfd == NULL) {
        int err = errno;
        printf("Error opening %s file: %d", bright, err);
        exit(1);
    }

    max_brightness = readint(max);
    current_brightness = max_brightness;
    actual_brightness = max_brightness; //readint(actual);

    set_screen_brightness(max_brightness);

    printf("actual_brightness %d, max_brightness %d\n", actual_brightness, max_brightness);

    bool fade_direction = true;
    while (1) {
        if (fade_direction && GetIdleTime() < timeout*1E4) {
            for (i = 0; i <= max_brightness; i++) {
                current_brightness += fade_amount;
                if (current_brightness > max_brightness) current_brightness = max_brightness;
                //printf("Brightness now %d\n", current_brightness);
                set_screen_brightness(current_brightness);
                sleep_ms(1);
            }
            fade_direction = false;
        } else if (!fade_direction && GetIdleTime() >= timeout*1E4) {
            if (current_brightness > 0) {
                for (i = max_brightness; i > 0; i--) {
                    current_brightness -= fade_amount;
                    if (current_brightness < 0) current_brightness = 0;
                    //printf("Brightness now %d\n", current_brightness);
                    set_screen_brightness(current_brightness);
                    sleep_ms(2);
                }
            }
            fade_direction = true;
        }
        //printf("Idle Time: %d\n", GetIdleTime());
        sleep_ms(1000);
    }
}
