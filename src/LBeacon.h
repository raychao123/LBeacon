/*
 * Copyright (c) 2016 Academia Sinica, Institute of Information Science
 *
 * License:
 *
 *      GPL 3.0 : The content of this file is subject to the terms and
 *      conditions defined in file 'COPYING.txt', which is part of this source
 *      code package.
 *
 * Project Name:
 *
 *      BeDIPS
 *
 * File Description:
 *
 *      This is the header file containing the function declarations and
 *      variables used in the LBeacon.c file.
 *
 * File Name:
 *
 *      LBeacon.h
 *
 * Abstract:
 *
 *      BeDIPS uses LBeacons to deliver 3D coordinates and textual
 *      descriptions of their locations to users' devices. Basically, a LBeacon
 *      is an inexpensive, Bluetooth Smart Ready device. The 3D coordinates and
 *      location description of every LBeacon are retrieved from BeDIS
 *      (Building/environment Data and Information System) and stored locally
 *      during deployment and maintenance times. Once initialized, each LBeacon
 *      broadcasts its coordinates and location description to Bluetooth
 *      enabled user devices within its coverage area.
 *
 * Authors:
 *
 *      Jake Lee, jakelee@iis.sinica.edu.tw
 *      Johnson Su, johnsonsu@iis.sinica.edu.tw
 *      Shirley Huang, shirley.huang.93@gmail.com
 *      Han Hu, hhu14@illinois.edu
 *      Jeffrey Lin, lin.jeff03@gmail.com
 *      Howard Hsu, haohsu0823@gmail.com
 */

/*
* INCLUDES
*/

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <obexftp/client.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "LinkedList.h"
#include "Queue.h"
#include "Utilities.h"

/*
 * CONSTANTS
 */

/* Maximum number of characters in each line of config file */
#define CONFIG_BUFFER 64

/* File path of the config file */
#define CONFIG_FILENAME "../config/config.conf"

/* Parameter that determines the start of the config file */
#define DELIMITER "="

/* Maximum number of characters in message filenames */
#define FILENAME_BUFFER 256

/* Length of Bluetooth MAC address */
#define LENGTH_OF_MAC_ADDRESS 18

/* Transmission range limiter that only allows devices in the RSSI range to
 * connect */
#define RSSI_RANGE -60

/* Time interval, in milliseconds, that determines if the bluetooth device can
 * be removed from the push list */
#define TIMEOUT 60000

/* Maximum number of characters in each line of output.txt file for tracking
 * timestamps and MAC addresses */
#define TRACKING_BUFFER 1024

/* Command opcode pack/unpack from HCI library */
#define cmd_opcode_pack(ogf, ocf) (uint16_t)((ocf & 0x03ff)|(ogf << 10))

/* BlueZ bluetooth extended inquiry response protocol: flags */
#define EIR_FLAGS 0X01

/* BlueZ bluetooth extended inquiry response protocol: shorten local name */
#define EIR_NAME_SHORT 0x08

/* BlueZ bluetooth extended inquiry response protocol: complete local name */
#define EIR_NAME_COMPLETE 0x09

/* BlueZ bluetooth extended inquiry response protocol: Manufacturer Specific
 * Data */
#define EIR_MANUFACTURE_SPECIFIC_DATA 0xFF

/*
 * GLOBAL VARIABLES
 */

/* The path of object push file */
char *g_filepath;

/* The first timestamp of the output file used for tracking scanned devices */
unsigned g_initial_timestamp_of_file = 0;

/* The most recent time of the output file used for tracking scanned devices */
unsigned g_most_recent_timestamp_of_file = 0;

/* Number of lines in the output file used for tracking scanned devices */
int g_size_of_file = 0;

/*
 * UNION
 */

/* This union will convert floats into Hex code which will be used in the main
 */
union {
    float f;
    unsigned char b[sizeof(float)];
    int d[2];
} coordinate_X;

union {
    float f;
    unsigned char b[sizeof(float)];
} coordinate_Y;

union {
    float f;
    unsigned char b[sizeof(float)];
} coordinate_Z;

/*
 * TYPEDEF STRUCTS
 */

typedef struct Config {
    /* A string with information about the X coordinate of the beacon location
     */
    char coordinate_X[CONFIG_BUFFER];

    /* A string with information about the Y coordinate of the beacon location
     */
    char coordinate_Y[CONFIG_BUFFER];

    /* A string with information about the Z coordinate of the beacon location
     */
    char coordinate_Z[CONFIG_BUFFER];

    /* A string with information about the filename */
    char filename[CONFIG_BUFFER];

    /* A string with information about the filepath */
    char filepath[CONFIG_BUFFER];

    /* A string with information about the maximum number of devices to be
     * handled by all PUSH dongles */
    char maximum_number_of_devices[CONFIG_BUFFER];

    /* A string with information about the number of groups */
    char number_of_groups[CONFIG_BUFFER];

    /* A string with information about the number of messages */
    char number_of_messages[CONFIG_BUFFER];

    /* A string with information about the number of push dongles */
    char number_of_push_dongles[CONFIG_BUFFER];

    /* A string with information about the required signal strength */
    char rssi_coverage[CONFIG_BUFFER];

    /* A string with information about the universally unique identifer */
    char uuid[CONFIG_BUFFER];

    /* Stores the string length needed to store the X coordinate */
    int coordinate_X_length;

    /* Stores the string length needed to store the Y coordinate */
    int coordinate_Y_length;

    /* Stores the string length needed to store the Z coordinate */
    int coordinate_Z_length;

    /* Stores the string length needed to store the filename */
    int filename_length;

    /* Stores the string length needed to store the filepath */
    int filepath_length;

    /* Stores the string length needed to store the maximum number of devices */
    int maximum_number_of_devices_length;

    /* Stores the string length needed to store the number of groups */
    int number_of_groups_length;

    /* Stores the string length needed to store the number of messages */
    int number_of_messages_length;

    /* Stores the string length needed to store the number of push dongles */
    int number_of_push_dongles_length;

    /* Stores the string length needed to store the required signal strength */
    int rssi_coverage_length;

    /* Stores the string length needed to store the universally unique identifer
     */
    int uuid_length;
} Config;

/* Struct for storing config information from the input file */
Config g_config;

typedef struct ThreadStatus {
    char scanned_mac_address[LENGTH_OF_MAC_ADDRESS];
    int idle;
    bool is_waiting_to_send;
} ThreadStatus;

/* An array of struct for storing information for each thread */
ThreadStatus *g_idle_handler;

/*
 * FUNCTIONS
 */

Config get_config(char *filename);
long long get_system_time();
bool is_used_addr(char address[]);
void send_to_push_dongle(bdaddr_t *bluetooth_device_address, int rssi);
void *queue_to_array();
void *send_file(void *id);
void print_RSSI_value(bdaddr_t *bluetooth_device_address, bool has_rssi,
                      int rssi);
void start_scanning();
void *cleanup_push_list(void);
void track_devices(bdaddr_t *bluetooth_device_address, char *filename);
char *choose_file(char *message_to_send);
void pthread_create_error_message(int error_code);
int enable_advertising(int advertising_interval, char *advertising_uuid,
                       int rssi_value);
int disable_advertising();
void *ble_beacon(void *beacon_location);