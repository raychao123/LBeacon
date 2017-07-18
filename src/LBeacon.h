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

#include "bluetooth/bluetooth.h"
#include "bluetooth/hci.h"
#include "bluetooth/hci_lib.h"
#include "ctype.h"
#include "dirent.h"
#include "errno.h"
#include "limits.h"
#include "netdb.h"
#include "netinet/in.h"
#include "obexftp/client.h"
#include "pthread.h"
#include "semaphore.h"
#include "signal.h"
#include "stdbool.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "sys/ioctl.h"
#include "sys/poll.h"
#include "sys/socket.h"
#include "sys/time.h"
#include "sys/timeb.h"
#include "sys/types.h"
#include "time.h"
#include "unistd.h"

/*
 * CONSTANTS
 */

// File path of the config file
#define CONFIG_FILENAME "../config/config.conf"

// Parameter that determines the start of the config file
#define DELIMITER "="

// Length of Bluetooth MAC address
#define LENGTH_OF_MAC_ADDRESS 18

// Maximum number of characters in each line of config file
#define MAXIMUM_BUFFER 64

// Maximum number of devices to be handled by all PUSH dongles
#define MAXIMUM_NUMBER_OF_DEVICES 18

// Maximum number of devices that a PUSH dongle can handle
#define MAXIMUM_NUMBER_OF_DEVICES_HANDLED_BY_EACH_PUSH_DONGLE 9

// Maximum number of the Bluetooth Object PUSH threads
#define MAXIMUM_NUMBER_OF_THREADS 18

// Optimal number of user devices handled by each PUSH dongle
#define OPTIMAL_NUMBER_OF_DEVICES_HANDLED_BY_A_PUSH_DONGLE 5

// Number of the Bluetooth dongles used for PUSH function
#define NUMBER_OF_PUSH_DONGLES 2

// Device ID of the primary PUSH dongle
#define PUSH_DONGLE_PRIMARY 2

// Device ID of the secondary PUSH dongle
#define PUSH_DONGLE_SECONDARY 3

// Device ID of the SCAN dongle
#define SCAN_DONGLE 1

// Transmission range limiter that only allows devices in the RSSI range to
// connect
#define RSSI_RANGE -60

// Time interval, in milliseconds, that determines if the bluetooth device can
// be removed from the push list
#define TIMEOUT 20000

// Command opcode pack/unpack from HCI library
#define cmd_opcode_pack(ogf, ocf) (uint16_t)((ocf & 0x03ff)|(ogf << 10))

// BlueZ bluetooth extended inquiry response protocol: flags
#define EIR_FLAGS 0X01

// BlueZ bluetooth extended inquiry response protocol: shorten local name
#define EIR_NAME_SHORT 0x08

// BlueZ bluetooth extended inquiry response protocol: complete local name
#define EIR_NAME_COMPLETE 0x09

// BlueZ bluetooth extended inquiry response protocol:: Manufacturer Specific
// Data
#define EIR_MANUFACTURE_SPECIFIC_DATA 0xFF

/*
 * GLOBAL VARIABLES
 */

// An array used for tracking MAC addresses of scanned devices
char g_addr[LENGTH_OF_MAC_ADDRESS] = {0};

// A flag that is used to check if CTRL-C is pressed
bool g_done = false;

// The path of object push file
char *g_filepath;

// An array of flags needed to indicate whether each push thread is idle
int g_idle_handler[MAXIMUM_NUMBER_OF_DEVICES] = {0};

// The first timestamp of the output file used for tracking scanned devices
unsigned g_initial_timestamp_of_file = 0;

// The most recent time of the output file used for tracking scanned devices
unsigned g_most_recent_timestamp_of_file = 0;

// An array used for storing pushed users and bluetooth device addresses
char g_pushed_user_addr[MAXIMUM_NUMBER_OF_DEVICES][LENGTH_OF_MAC_ADDRESS];

// Number of lines in the output file used for tracking scanned devices
int g_size_of_file = 0;

/*
 * UNION
 */

// Theis union will convert floats into Hex code which will be used in the main
// code
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
} level;

/*
 * TYPEDEF STRUCTS
 */

typedef struct Config {
    char coordinate_X
        [MAXIMUM_BUFFER];  // A string
                                                                     // with
    // information
    // about
    // the X
    // coordinate
    // of the
    // beacon
    // location
    // from the
    // config
    // file
    char coordinate_Y
        [MAXIMUM_BUFFER];  // A string
                                                                     // with
    // information
    // about
    // the Y
    // coordinate
    // of the
    // beacon
    // location
    // from the
    // config
    // file
    char filename
        [MAXIMUM_BUFFER];  // A string
                                                                     // with
    // information
    // about
    // the
    // filename
    // from the
    // config
    // file
    // from the
    // config
    // file
    char filepath
        [MAXIMUM_BUFFER];  // A string
                                                                     // with
    // information
    // about
    // the
    // filepath
    // from the
    // config
    // file
    // from the
    // config
    // file
    char level
        [MAXIMUM_BUFFER];  // A string
                                                                     // with
    // information
    // about
    // the
    // current
    // level in
    // the
    // building
    // from the
    // config
    // file
    char rssi_coverage
        [MAXIMUM_BUFFER];  // A string
                                                                     // with
    // information
    // about
    // the
    // required
    // signal
    // strength
    // from the
    // config
    // file
    char num_groups
        [MAXIMUM_BUFFER];  // A string
                                                                     // with
    // information
    // about
    // the
    // number
    // of
    // groups
    // from the
    // config
    // file
    char num_messages
        [MAXIMUM_BUFFER];  // A string
                                                                     // with
    // information
    // about
    // the
    // number
    // of
    // messages
    // from the
    // config
    // file
    char uuid
        [MAXIMUM_BUFFER];  // A string
                                                                     // with
    // information
    // about
    // the
    // universally
    // unique
    // identifer
    // from the
    // config
    // file
    int coordinate_X_len;  // Stores the string length needed to store the X
                           // coordinate
    int coordinate_Y_len;  // Stores the string length needed to store the Y
                           // coordinate
    int filename_len;  // Stores the string length needed to store the filename
    int filepath_len;  // Stores the string length needed to store the filepath
    int level_len;     // Stores the string length needed to store the current
                       // level position
    int rssi_coverage_len;  // Stores the string length needed to store the
                            // required signal strength
    int num_groups_len;  // Stores the string length needed to store the number
                         // of groups
    int num_messages_len;  // Stores the string length needed to store the
                           // number of messages
    int uuid_len;  // Stores the string length needed to store the universally
                   // unique identifer
} Config;

// Struct for storing config information from the input file
Config g_config;

typedef struct PushList {
    long long first_appearance_time[MAXIMUM_NUMBER_OF_DEVICES];
    char discovered_device_addr[MAXIMUM_NUMBER_OF_DEVICES]
                               [LENGTH_OF_MAC_ADDRESS];
    bool is_used_device[MAXIMUM_NUMBER_OF_DEVICES];
} PushList;

// Struct for storing information on users' devices discovered by each becon
PushList g_push_list;

typedef struct ThreadAddr {
    pthread_t thread;
    int thread_id;
    char addr[LENGTH_OF_MAC_ADDRESS];
} ThreadAddr;

// Struct for storing information for each thread
ThreadAddr g_thread_addr[MAXIMUM_NUMBER_OF_DEVICES];

/*
 * FUNCTIONS
 */
long long get_system_time();
bool is_used_addr(char addr[]);
void *send_file(void *ptr);
static void send_to_push_dongle(bdaddr_t *bdaddr, int rssi);
static void print_RSSI_value(bdaddr_t *bdaddr, bool has_rssi, int rssi);
static void track_devices(bdaddr_t *bdaddr, char *filename);
static void start_scanning();
void *timeout_cleaner(void);
char *choose_file(char *messagetosend);
Config get_config(char *filename);