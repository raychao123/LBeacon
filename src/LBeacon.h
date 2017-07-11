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
 *      BeDIPS uses LBeacons to deliver to users' devices 3D coordinates and
 *      textual descriptions of their locations. Basically, a LBeacon is an
 *      inexpensive, Bluetooth Smart Ready device. The 3D coordinates and
 *      location description of every LBeacon are retrieved from BeDIS
 *      (Building/environment Data and Information System) and stored locally
 *      during deployment and maintenance times. Once initialized, each LBeacon
 *      broadcasts its coordinates and location description to Bluetooth
 *      enabled devices within its coverage area.
 *
 * Authors:
 *
 *      Jake Lee, jakelee@iis.sinica.edu.tw
 *      Johnson Su, johnsonsu@iis.sinica.edu.tw
 *      Shirley Huang, shirley.huang.93@gmail.com
 *      Han Hu, hhu14@illinois.edu
 *      Jeffrey Lin, lin.jeff03@gmail.com
 *      Howard Hsu, haohsu0823@gmail.com
 *
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

/*
 * CONSTANTS
 */

// The filepath of the config file
#define CONFIG_FILENAME "../config/config.conf"

// Read the parameter after "=" from config file
#define DELIMITER "="

// Length of Bluetooth MAC addr
#define LEN_OF_MAC_ADDRESS 18

// Maximum number of characters in each line of config file
#define MAX_BUFFER 64

// Maximum number of devices that all PUSH dongles can handle
#define MAX_DEVICES 18

// Maximum number of devices that a PUSH dongle can handle
#define MAX_DEVICES_HANDLED_BY_EACH_PUSH_DONGLE 9

// Maximum number of the Bluetooth Object PUSH threads
#define MAX_THREADS 18

// The optimal number of user devices each PUSH dongle is responsible for
#define NUM_OF_DEVICES_IN_BLOCK_OF_PUSH_DONGLE 5

// Transmission range limiter
#define RSSI_RANGE -60

// The length of interval time, in milliseconds, a user object is pushed
#define TIMEOUT 20000

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

// An array used for tracking MAC addresses of scanned devices
char g_addr[LEN_OF_MAC_ADDRESS] = {0};

// A flag that is used to check if CTRL-C is pressed
bool g_done = false;

// The path of object push file
char *g_filepath;

// The first timestamp of the output file used for tracking scanned devices
unsigned g_initial_timestamp_of_file = 0;

// An array of flags needed to indicate whether each push thread is idle or not
int g_idle_handler[MAX_THREADS] = {0};

// The most recent time of the output file used for tracking scanned devices
unsigned g_most_recent_timestamp_of_file = 0;

// An array used for handling pushed users and bluetooth device address
char g_pushed_user_addr[MAX_DEVICES][LEN_OF_MAC_ADDRESS] = {0};

// An array for saving the MAC address so it can be stored into database
char g_saved_user_addr[MAX_DEVICES][LEN_OF_MAC_ADDRESS] = {0};

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
    char dongle_push_A[MAX_BUFFER];
    char dongle_push_B[MAX_BUFFER];
    char dongle_scan[MAX_BUFFER];
    char filename[MAX_BUFFER];
    char filepath[MAX_BUFFER];
    char num_groups[MAX_BUFFER];
    char num_messages[MAX_BUFFER];
    char num_push_dongles[MAX_BUFFER];
    char rssi_coverage[MAX_BUFFER];
    char uuid[MAX_BUFFER];
    int coordinate_X_len;
    int coordinate_Y_len;
    int coordinate_Z_len;
    int dongle_push_A_len;
    int dongle_push_B_len;
    int dongle_scan_len;
    int filename_len;
    int filepath_len;
    int num_groups_len;
    int num_messages_len;
    int num_push_dongles_len;
    int rssi_coverage_len;
    int uuid_len;
} Config;

// Struct for storing config information from the inputted file
Config g_config;

typedef struct PushList {
    long long first_appearance_time[MAX_DEVICES];
    char discovered_device_addr[MAX_DEVICES][LEN_OF_MAC_ADDRESS];
    bool is_used_device[MAX_DEVICES];
} PushList;

// Struct for storing information on users' devices discovered by each becon
PushList g_push_list;

typedef struct ThreadAddr {
    pthread_t thread;
    int thread_id;
    char addr[LEN_OF_MAC_ADDRESS];
} ThreadAddr;

// Struct for storing information for each thread
ThreadAddr g_thread_addr[MAX_THREADS];

/*
 * UTILITY FUNCTIONS
 */

unsigned int *uuid_str_to_data(char *uuid);
unsigned int twoc(int in, int t);
void ctrlc_handler(int s);

/*
 * FUNCTIONS
 */

// Get the current system time
long long get_system_time();

// Check whether the user's address is being used or can be pushed to again
bool is_used_addr(char addr[]);

// Send the push message to the user's device by a working asynchronous thread
void *send_file(void *ptr);

// Send scanned user's MAC address to push dongle
static void send_to_push_dongle(bdaddr_t *bdaddr, int rssi);

// Print the result of RSSI value for scanned MAC address
static void print_RSSI_value(bdaddr_t *bdaddr, bool has_rssi, int rssi);

// Track scanned MAC addresses and store information in an output file
static void track_devices(bdaddr_t *bdaddr, char *filename);

// Scan continuously for bluetooth devices under the beacon
static void start_scanning();

// Remove the user's MAC address from pushed list
void *cleanup_push_list(void);

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