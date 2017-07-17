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

// The filepath of the config file
#define CONFIG_FILENAME "../config/config.conf"

// Read the parameter after "=" from config file
#define DELIMITER "="

// Length of Bluetooth MAC address
#define LEN_OF_MAC_ADDRESS 18

// Maximum number of characters in each line of config file
#define MAX_BUFFER 64

// Transmission range limiter
#define RSSI_RANGE -60

// The length of interval time, in milliseconds, a user object is pushed
#define TIMEOUT 60000

// Command opcode pack/unpack from HCI library
#define cmd_opcode_pack(ogf, ocf) (uint16_t)((ocf & 0x03ff)|(ogf << 10))

// BlueZ bluetooth protocol: flags
#define EIR_FLAGS 0X01

// BlueZ bluetooth protocol: shorten local name
#define EIR_NAME_SHORT 0x08

// BlueZ bluetooth protocol: complete local name
#define EIR_NAME_COMPLETE 0x09

// BlueZ bluetooth protocol:: Manufacturer Specific Data
#define EIR_MANUFACTURE_SPECIFIC 0xFF

/*
 * GLOBAL VARIABLES
 */

// The path of object push file
char *g_filepath;

// The first timestamp of the output file used for tracking scanned devices
unsigned g_initial_timestamp_of_file = 0;

// The most recent time of the output file used for tracking scanned devices
unsigned g_most_recent_timestamp_of_file = 0;

// Number of lines in the output file used for tracking scanned devices
int g_size_of_file = 0;

/*
 * UNION
 */

// Transform float to Hex code
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
    char coordinate_X[MAX_BUFFER];
    char coordinate_Y[MAX_BUFFER];
    char coordinate_Z[MAX_BUFFER];
    char filename[MAX_BUFFER];
    char filepath[MAX_BUFFER];
    char max_devices[MAX_BUFFER];
    char num_groups[MAX_BUFFER];
    char num_messages[MAX_BUFFER];
    char num_push_dongles[MAX_BUFFER];
    char rssi_coverage[MAX_BUFFER];
    char uuid[MAX_BUFFER];
    int coordinate_X_len;
    int coordinate_Y_len;
    int coordinate_Z_len;
    int filename_len;
    int filepath_len;
    int max_devices_len;
    int num_groups_len;
    int num_messages_len;
    int num_push_dongles_len;
    int rssi_coverage_len;
    int uuid_len;
} Config;

// Struct for storing config information from the inputted file
Config g_config;

typedef struct ThreadStatus {
    char scanned_mac_addr[LEN_OF_MAC_ADDRESS];
    int idle;
    bool is_waiting_to_send;
} ThreadStatus;

// Struct for storing the status of each thread
ThreadStatus *g_idle_handler;

/*
 * FUNCTIONS
 */

// Get the current system time
long long get_system_time();

// Check whether the user's address is being used or can be pushed to again
bool is_used_addr(char addr[]);

// Scan continuously for bluetooth devices under the beacon
static void start_scanning();

// Send scanned user's MAC address to push dongle
static void send_to_push_dongle(bdaddr_t *bdaddr, int rssi);

// @todo
void *queue_to_array();

// Send the push message to the user's device by a working asynchronous thread
void *send_file(void *arg);

// Remove the user's MAC address from pushed list
void *cleanup_push_list(void);

// Print the result of RSSI value for scanned MAC address
static void print_RSSI_value(bdaddr_t *bdaddr, bool has_rssi, int rssi);

// Track scanned MAC addresses and store information in an output file
static void track_devices(bdaddr_t *bdaddr, char *filename);

// Receive filepath of designated message that will be broadcast to users
char *choose_file(char *messagetosend);

// Read parameter from config file and store in Config struct
Config get_config(char *filename);

// Determines the advertising capabilities and enables advertising
int enable_advertising(int advertising_interval, char *advertising_uuid,
                       int rssi_value);

// Determines the advertising capabilities and disables advertising
int disable_advertising();

// @todo
void *ble_beacon(void *ptr);