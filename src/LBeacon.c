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
 *      This file will detect a phone and then scan for its Bluetooth address.
 *      Depending on the RSSI value, it will determine if it should send
 *      location related files to the user. The detection is based on BLE or
 *      OBEX.
 *
 * File Name:
 *
 *      LBeacon.c
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

#include "LBeacon.h"

/*
 *  get_system_time:
 *
 *  Helper function called by is_used_addr to give each user's MAC address the
 *  time it is added to the push list. Helper function called by timeout_cleaner
 *  to check if it has been 20 seconds since the beacon was last added to the
 *  push list for sending the broadcast message.
 *
 *  Parameters:
 *
 *  None
 *
 *  Return value:
 *
 *  system_time - System time in milliseconds
 */
long long get_system_time() {
    struct timeb t;         // struct that stores the time
    long long system_time;  // return value as long long
    ftime(&t);              // get current time as Epoch and store in t

    /* Convert time from Epoch to time in milliseconds as long long type */
    system_time = 1000 * t.time + t.millitm;

    return system_time;
}

/*
 *  is_used_addr:
 *
 *  Helper function called by send_file to check whether the user's MAC address
 *  given as input is in the push list of the LBeacon. If it is, the function
 *  returns true. If the address is not in the list, the function adds the
 *  address into the list, stamps it with current system time, and then returns
 *  false.
 *
 *  Parameters:
 *
 *  addr - scanned MAC address of bluetooth device
 *
 *  Return value:
 *
 *  false - new MAC address, add to push list
 *  true - used MAC address
 */
bool is_used_addr(char addr[]) {
    LinkedListNode *temp = ll_head;
    while (temp != NULL) {
        if (0 == strcmp(addr, temp->data.scanned_mac_addr)) {
            return true;
        }
        temp = temp->next;
    }
    // printf("not in linked list: %s\n", &addr[0]);
    return false;
}

/*
 *  send_file:
 *
 *  Sends the push message to the scanned bluetooth device. This function is
 *  done asynchronously by another working thread while the beacon is still
 *  scanning for other user's bluetooth device under the beacon.
 *
 *  Parameters:
 *
 *  ptr - MAC address that will receive the message file
 *
 *  Return value:
 *
 *  None
 */
void *send_file(void *arg) {
    obexftp_client_t *cli = NULL;
    char *addr = NULL;
    char *filename;
    int channel = -1;
    int dev_id = 0;
    int socket;
    int i;  // iterate thru to see each send_file thread status
    int j;
    int k;
    int ret;
    bool cancelled = false;

    int num_push_dongles = atoi(g_config.num_push_dongles);
    int max_devices = atoi(g_config.max_devices);
    int max_devices_per_dongle = max_devices / num_push_dongles;

    int tid = (int)arg;

    while (cancelled == false) {
        for (i = 0; i < max_devices; i++) {
            if (i == tid && g_idle_handler[i].is_waiting_to_send == true) {
                /* Split the half of the thread ID to one dongle and the other
                 * half to the second dongle. */
                for (j = 0; j < num_push_dongles; j++) {
                    for (k = 0; k < max_devices_per_dongle; k++) {
                        if (tid == j * max_devices_per_dongle + k) {
                            dev_id = j + 1;
                        }
                    }
                }
                /* Open socket and use current time as start time to determine
                 * how long it takes to send the file to the bluetooth device.
                 */

                // printf("send_file thread id = %d mac addr = %s\n", tid,
                // &g_idle_handler[i].scanned_mac_addr[0]);
                // print_array();

                socket = hci_open_dev(dev_id);
                if (0 > dev_id || 0 > socket) {
                    /* handle error */
                    perror("Error opening socket");
                    for (j = 0; j < LEN_OF_MAC_ADDRESS; j++) {
                        g_idle_handler[i].scanned_mac_addr[j] = 0;
                    }
                    g_idle_handler[i].idle = -1;
                    g_idle_handler[i].is_waiting_to_send = false;
                    break;
                }
                // printf("Thread number %d\n", thread_addr->thread_id);
                long long start = get_system_time();
                addr = (char *)g_idle_handler[i].scanned_mac_addr;
                channel = obexftp_browse_bt_push(addr);

                /* Extract basename from filepath */
                filename = strrchr(g_filepath, '/');
                filename[g_config.filename_len] = '\0';
                printf("%s\n", filename);
                if (!filename) {
                    filename = g_filepath;
                } else {
                    filename++;
                }
                printf("Sending file %s to %s\n", filename, addr);

                /* Open connection */
                cli = obexftp_open(OBEX_TRANS_BLUETOOTH, NULL, NULL, NULL);
                long long end = get_system_time();
                // printf("Time it takes to open connection: %lld ms\n", end -
                // start);
                if (cli == NULL) {
                    /* handle error */
                    fprintf(stderr, "Error opening obexftp client\n");
                    for (j = 0; j < LEN_OF_MAC_ADDRESS; j++) {
                        g_idle_handler[i].scanned_mac_addr[j] = 0;
                    }
                    g_idle_handler[i].idle = -1;
                    g_idle_handler[i].is_waiting_to_send = false;
                    close(socket);
                    break;
                }

                /* Connect to the scanned device */
                ret = obexftp_connect_push(cli, addr, channel);

                if (0 > ret) {
                    /* handle error */
                    fprintf(stderr, "Error connecting to obexftp device\n");
                    obexftp_close(cli);
                    cli = NULL;
                    for (j = 0; j < LEN_OF_MAC_ADDRESS; j++) {
                        g_idle_handler[i].scanned_mac_addr[j] = 0;
                    }
                    g_idle_handler[i].idle = -1;
                    g_idle_handler[i].is_waiting_to_send = false;
                    close(socket);
                    break;
                }

                /* Push file to the scanned device */
                ret = obexftp_put_file(cli, g_filepath, filename);
                if (0 > ret) {
                    /* handle error */
                    fprintf(stderr, "Error putting file\n");
                }

                /* Disconnect connection */
                ret = obexftp_disconnect(cli);
                if (0 > ret) {
                    /* handle error */
                    fprintf(stderr, "Error disconnecting the client\n");
                }

                /* Close socket */
                obexftp_close(cli);
                cli = NULL;
                for (j = 0; j < LEN_OF_MAC_ADDRESS; j++) {
                    g_idle_handler[i].scanned_mac_addr[j] = 0;
                }
                g_idle_handler[i].idle = -1;
                g_idle_handler[i].is_waiting_to_send = false;
                close(socket);
            }
        }
    }
}

/*
 *  queue_to_array:
 *
 *  @todo
 *
 *  Parameters:
 *
 *  None
 *
 *  Return value:
 *
 *  None
 */
void *queue_to_array() {
    int max_devices = atoi(g_config.max_devices);
    int i;
    int j;
    bool cancelled = false;

    while (cancelled == false) {
        for (i = 0; i < max_devices; i++) {
            char *addr = peek();
            if (g_idle_handler[i].idle == -1 && addr != NULL) {
                print_array();
                for (j = 0; j < 18; j++) {
                    g_idle_handler[i].scanned_mac_addr[j] = addr[j];
                }
                dequeue();
                g_idle_handler[i].idle = i;
                g_idle_handler[i].is_waiting_to_send = true;
            }
        }
    }
}

/*
 *  send_to_push_dongle:
 *
 *  For each new MAC address of scanned bluetooth device, add to an array of
 *  ThreadAddr struct that will give it a thread ID and send the MAC address to
 *  send_file. Only add MAC address if it isn't in the push list and does not
 *  have a thread ID. If the bluetooth device is already in the push list, don't
 *  send to push dongle again.
 *
 *  Parameters:
 *
 *  bdaddr - bluetooth device address
 *  rssi - RSSI value of bluetooth device
 *
 *  Return value:
 *
 *  None
 */
static void send_to_push_dongle(bdaddr_t *bdaddr, int rssi) {
    int i;                          // iterator through number of push dongle
    char addr[LEN_OF_MAC_ADDRESS];  // store the MAC address as string

    ba2str(bdaddr, addr);

    // printf("scanned: %s\n", &addr[0]);
    if (is_used_addr(addr) == false) {
        // printf("need to add to linked list: %s\n", &addr[0]);
        PushList data;
        data.initial_scanned_time = get_system_time();
        for (i = 0; i < LEN_OF_MAC_ADDRESS; i++) {
            data.scanned_mac_addr[i] = addr[i];
        }
        insertFirst(data);
        enqueue(addr);
        printList();
        print();
    }
}

/*
 *  print_result:
 *
 *  Print the RSSI value along with the MAC address of user's scanned bluetooth
 *  device. When running, we will continuously see a list of scanned bluetooth
 *  devices running in the console.
 *
 *  Parameters:
 *
 *  bdaddr - bluetooth device address
 *  has_rssi - whether the bluetooth device has an RSSI value or not
 *  rssi - RSSI value of bluetooth device
 *
 *  Return value:
 *
 *  None
 */
static void print_RSSI_value(bdaddr_t *bdaddr, bool has_rssi, int rssi) {
    char addr[LEN_OF_MAC_ADDRESS];  // MAC address that will be printed

    ba2str(bdaddr, addr);

    printf("%17s", addr);
    if (has_rssi) {
        printf(" RSSI:%d", rssi);
    } else {
        printf(" RSSI:n/a");
    }
    printf("\n");
    fflush(NULL);
}

/*
 *  track_devices:
 *
 *  Track the MAC addresses of scanned bluetooth devices under the beacon. An
 *  output file will contain each timestamp and the MAC addresses of the scanned
 *  bluetooth devices at the given timestamp. Format timestamp and MAC addresses
 *  into a char[] and append new line to end of file. " - " is used to separate
 *  timestamp with MAC address and ", " is used to separate each MAC address.
 *
 *  Parameters:
 *
 *  bdaddr - bluetooth device address
 *  filename - name of the file where all the data will be stored
 *
 *  Return value:
 *
 *  None
 */
static void track_devices(bdaddr_t *bdaddr, char *filename) {
    char addr[LEN_OF_MAC_ADDRESS];
    char ll2c[10];  // used for converting long long to char[]

    /* Get current timestamp when tracking bluetooth devices. If file is empty,
     * create new file with LBeacon ID. */
    unsigned timestamp = (unsigned)time(NULL);
    sprintf(ll2c, "%u", timestamp);
    if (0 == g_size_of_file) {
        FILE *fd = fopen(filename, "w+");  // w+ overwrites the file
        if (fd == NULL) {
            /* handle error */
            perror("Error opening file");
            return;
        }
        fputs("LBeacon ID: ", fd);
        fputs(g_config.uuid, fd);
        fclose(fd);
        g_size_of_file++;
        g_initial_timestamp_of_file = timestamp;
        memset(&addr[0], 0, sizeof(addr));
    }

    /* If timestamp already exists add MAC address to end of previous line, else
     * create new line. Double check that MAC address is not already added at a
     * given timestamp using strstr. */
    ba2str(bdaddr, addr);
    if (timestamp != g_most_recent_timestamp_of_file) {
        FILE *output;
        char buffer[1024];
        output = fopen(filename, "a+");
        if (output == NULL) {
            /* handle error */
            perror("Error opening file");
            return;
        }
        while (fgets(buffer, sizeof(buffer), output) != NULL) {
        }
        fputs("\n", output);
        fputs(ll2c, output);
        fputs(" - ", output);
        fputs(addr, output);
        fclose(output);

        g_most_recent_timestamp_of_file = timestamp;
        g_size_of_file++;
    } else {
        FILE *output;
        char buffer[1024];
        output = fopen(filename, "a+");
        if (output == NULL) {
            /* handle error */
            perror("Error opening file");
            return;
        }
        while (fgets(buffer, sizeof(buffer), output) != NULL) {
        }
        if (strstr(buffer, addr) == NULL) {
            fputs(", ", output);
            fputs(addr, output);
        }
        fclose(output);
    }

    /* Send file to gateway if exceeds 1000 lines or reaches 6 hours. */
    /*
    unsigned diff = timestamp - g_initial_timestamp_of_file;
    if (1000 <= g_size_of_file || 21600 <= diff) {
        g_size_of_file = 0;
        g_most_recent_timestamp_of_file = 0;
        // send_file_to_gateway();
    }
    */
}

/*
 *  start_scanning:
 *
 *  Scan continuously for bluetooth devices under the beacon until need to
 *  cancel scanning. For each scanned device, it will fall under one of three
 *  cases. A bluetooth device with no RSSI value or with a RSSI value, or if the
 *  user wants to cancel the scanning. When the device is within RSSI value, the
 *  bluetooth device will be added to the push list so the message can be sent
 *  to the phone. This function running by the main thread.
 *
 *  Parameters:
 *
 *  None
 *
 *  Return value:
 *
 *  None
 */
static void start_scanning() {
    struct hci_filter flt;
    struct pollfd p;
    unsigned char buf[HCI_MAX_EVENT_SIZE];
    unsigned char *ptr;
    hci_event_hdr *hdr;
    inquiry_cp cp;
    inquiry_info_with_rssi *info_rssi;
    inquiry_info *info;
    bool cancelled = false;
    int dev_id = 0;
    int socket = 0;
    int len;
    int results;
    int i;

    /* Open Bluetooth device */
    socket = hci_open_dev(dev_id);
    if (0 > dev_id || 0 > socket) {
        /* handle error */
        perror("Error opening socket");
        return;
    }

    /* Setup filter */
    hci_filter_clear(&flt);
    hci_filter_set_ptype(HCI_EVENT_PKT, &flt);
    hci_filter_set_event(EVT_INQUIRY_RESULT, &flt);
    hci_filter_set_event(EVT_INQUIRY_RESULT_WITH_RSSI, &flt);
    hci_filter_set_event(EVT_INQUIRY_COMPLETE, &flt);
    if (0 > setsockopt(socket, SOL_HCI, HCI_FILTER, &flt, sizeof(flt))) {
        /* handle error */
        perror("Error setting HCI filter");
        return;
    }
    hci_write_inquiry_mode(socket, 0x01, 10);
    if (0 > hci_send_cmd(socket, OGF_HOST_CTL, OCF_WRITE_INQUIRY_MODE,
                         WRITE_INQUIRY_MODE_RP_SIZE, &cp)) {
        /* handle error */
        perror("Error setting inquiry mode");
        return;
    }

    /* @todo CONSTANTS */
    memset(&cp, 0, sizeof(cp));
    cp.lap[2] = 0x9e;
    cp.lap[1] = 0x8b;
    cp.lap[0] = 0x33;
    cp.num_rsp = 0;
    cp.length = 0x30;

    printf("Starting inquiry with RSSI...\n");

    if (0 >
        hci_send_cmd(socket, OGF_LINK_CTL, OCF_INQUIRY, INQUIRY_CP_SIZE, &cp)) {
        /* handle error */
        perror("Error starting inquiry");
        return;
    }

    p.fd = socket;
    p.events = POLLIN | POLLERR | POLLHUP;

    while (cancelled == false) {
        p.revents = 0;

        /* Poll the Bluetooth device for an event */
        if (0 < poll(&p, 1, -1)) {
            len = read(socket, buf, sizeof(buf));

            if (0 > len) {
                continue;
            } else if (0 == len) {
                break;
            }

            hdr = (void *)(buf + 1);
            ptr = buf + (1 + HCI_EVENT_HDR_SIZE);

            results = ptr[0];

            switch (hdr->evt) {
                /* Scanned device with no RSSI value. */
                case EVT_INQUIRY_RESULT: {
                    for (i = 0; i < results; i++) {
                        info = (void *)ptr + (sizeof(*info) * i) + 1;
                        // print_RSSI_value(&info->bdaddr, 0, 0);
                        track_devices(&info->bdaddr, "output.txt");
                    }
                } break;

                /* Scanned device with RSSI value. If within range, send message
                 * to bluetooth device. */
                case EVT_INQUIRY_RESULT_WITH_RSSI: {
                    for (i = 0; i < results; i++) {
                        info_rssi = (void *)ptr + (sizeof(*info_rssi) * i) + 1;
                        track_devices(&info_rssi->bdaddr, "output.txt");
                        print_RSSI_value(&info_rssi->bdaddr, 1,
                                         info_rssi->rssi);
                        if (info_rssi->rssi > RSSI_RANGE) {
                            send_to_push_dongle(&info_rssi->bdaddr,
                                                info_rssi->rssi);
                        }
                    }
                } break;

                /* Ending the scanning process. */
                case EVT_INQUIRY_COMPLETE: {
                    cancelled = true;
                } break;

                default:
                    break;
            }
        }
    }
    printf("Scanning done\n");
    close(socket);
}

/*
 *  cleanup_push_list:
 *
 *  When bluetooth device's MAC address is pushed by send_to_push_dongle, the
 *  MAC address will be stored in the used list then wait for timeout to be
 *  remove from list. This is continuously running in the background by a
 *  working thread along side the main thread that is scanning for devices.
 *
 *  Parameters:
 *
 *  None
 *
 *  Return value:
 *
 *  None
 */
void *cleanup_push_list(void) {
    bool cancelled = false;
    while (cancelled == false) {
        LinkedListNode *temp = ll_head;
        while (temp != NULL) {
            if (get_system_time() - temp->data.initial_scanned_time > TIMEOUT) {
                deleteNode(temp->data);
                printf("%s\n", "Removed a node");
            }
            temp = temp->next;
        }
    }
}

/*
 *  choose_file:
 *
 *  Receive name of message file and then sends user the filepath where message
 *  is located. Go through each directory in the messages folder of LBeacon and
 *  store the name into an array. Go through each of the directories to find the
 *  designated message we want to broadcast to the user's under the beacon. Once
 *  the message name is located, return the filepath as a string.
 *
 *  Parameters:
 *
 *  messagetosend - name of the message file we want to retrieve
 *
 *  Return value:
 *
 *  ret - message filepath
 */
char *choose_file(char *messagetosend) {
    DIR *groupdir;            // dirent that stores list of directories
    struct dirent *groupent;  // dirent struct that stores directory info
    int num_messages;         // number of message listed in config file
    int num_groups;           // number of groups listed in config file
    char path[256];           // store filepath of messagetosend location
    int count = 0;            // iterator for number of messages and groups
    int i = 0;                // iterator for number of groups
    char *ret;                // return value; converts path to a char *

    /* Convert number of groups and messages from string to integer. */
    num_groups = atoi(g_config.num_groups);
    num_messages = atoi(g_config.num_messages);

    char groups[num_groups][256];      // array of buffer for group file names
    char messages[num_messages][256];  // array of buffer for message file names

    /* Stores all the name of files and directories in groups. */
    groupdir = opendir("/home/pi/LBeacon/messages/");
    if (groupdir) {
        while ((groupent = readdir(groupdir)) != NULL) {
            if (strcmp(groupent->d_name, ".") != 0 &&
                strcmp(groupent->d_name, "..") != 0) {
                strcpy(groups[count], groupent->d_name);
                count++;
            }
        }
        closedir(groupdir);
    } else {
        /* handle error */
        perror("Directories do not exist");
        return NULL;
    }

    memset(path, 0, 256);
    count = 0;

    /* Go through each message in directory and store each file name. */
    for (i = 0; i < num_groups; i++) {
        /* Concatenate strings to make file path */
        sprintf(path, "/home/pi/LBeacon/messages/");
        strcat(path, groups[i]);
        DIR *messagedir;
        struct dirent *messageent;
        messagedir = opendir(path);
        if (messagedir) {
            while ((messageent = readdir(messagedir)) != NULL) {
                if (strcmp(messageent->d_name, ".") != 0 &&
                    strcmp(messageent->d_name, "..") != 0) {
                    strcpy(messages[count], messageent->d_name);
                    /* If message name found, return filepath. */
                    if (0 == strcmp(messages[count], messagetosend)) {
                        strcat(path, "/");
                        strcat(path, messages[count]);
                        // printf("%s\n", path);
                        ret = &path[0];
                        return ret;
                    }
                    count++;
                }
            }
            closedir(messagedir);
        } else {
            /* handle error */
            perror("Message files do not exist");
            return NULL;
        }
    }
    perror("Message files do not exist");
    return NULL;
}

/*
 *  get_config:
 *
 *  While not end of file, read config file line by line and store data into the
 *  global variable of a Config struct. The variable i is used to know which
 *  line is being processed.
 *
 *  Parameters:
 *
 *  filename - the name of the config file that stores all the beacon data
 *
 *  Return value:
 *
 *  config - Config struct including filepath, coordinates, etc.
 */
Config get_config(char *filename) {
    Config config;  // return variable is struct that contains all data

    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        /* handle error */
        fprintf(stderr, "Error opening file\n");
    } else {
        char line[MAX_BUFFER];  // stores the string of current line being read
        int i = 0;              // keeps track of which line being processed

        /* If there are still lines to read in the config file, read the line
         * and store each information into the config struct. */
        while (fgets(line, sizeof(line), file) != NULL) {
            char *config_message;
            config_message = strstr((char *)line, DELIMITER);
            config_message = config_message + strlen(DELIMITER);
            if (0 == i) {
                memcpy(config.coordinate_X, config_message,
                       strlen(config_message));
                config.coordinate_X_len = strlen(config_message);
            } else if (1 == i) {
                memcpy(config.coordinate_Y, config_message,
                       strlen(config_message));
                config.coordinate_Y_len = strlen(config_message);
            } else if (2 == i) {
                memcpy(config.coordinate_Z, config_message,
                       strlen(config_message));
                config.coordinate_Z_len = strlen(config_message);
            } else if (3 == i) {
                memcpy(config.filename, config_message, strlen(config_message));
                config.filename_len = strlen(config_message);
            } else if (4 == i) {
                memcpy(config.filepath, config_message, strlen(config_message));
                config.filepath_len = strlen(config_message);
            } else if (5 == i) {
                memcpy(config.max_devices, config_message,
                       strlen(config_message));
                config.max_devices_len = strlen(config_message);
            } else if (6 == i) {
                memcpy(config.num_groups, config_message,
                       strlen(config_message));
                config.num_groups_len = strlen(config_message);
            } else if (7 == i) {
                memcpy(config.num_messages, config_message,
                       strlen(config_message));
                config.num_messages_len = strlen(config_message);
            } else if (8 == i) {
                memcpy(config.num_push_dongles, config_message,
                       strlen(config_message));
                config.num_push_dongles_len = strlen(config_message);
            } else if (9 == i) {
                memcpy(config.rssi_coverage, config_message,
                       strlen(config_message));
                config.rssi_coverage_len = strlen(config_message);
            } else if (10 == i) {
                memcpy(config.uuid, config_message, strlen(config_message));
                config.uuid_len = strlen(config_message);
            }
            i++;
        }
        fclose(file);
    }

    return config;
}

/*
 *  enable_advertising:
 *
 *  @todo Determines the advertising capabilities and enables advertising.
 *
 *  Parameters:
 *
 *  advertising_interval - @todo
 *  advertising_uuid - @todo
 *  rssi_value - @todo
 *
 *  Return value:
 *
 *  data - @todo
 */
int enable_advertising(int advertising_interval, char *advertising_uuid,
                       int rssi_value) {
    int device_id = hci_get_route(NULL);
    // @todo test for error

    int device_handle = 0;
    if ((device_handle = hci_open_dev(device_id)) < 0) {
        /* handle error */
        fprintf(stderr, "Error opening device\n");
        exit(EXIT_FAILURE);
    }

    le_set_advertising_parameters_cp adv_params_cp;
    memset(&adv_params_cp, 0, sizeof(adv_params_cp));
    adv_params_cp.min_interval = htobs(advertising_interval);
    adv_params_cp.max_interval = htobs(advertising_interval);
    adv_params_cp.chan_map = 7;

    uint8_t status;
    struct hci_request rq;
    memset(&rq, 0, sizeof(rq));
    rq.ogf = OGF_LE_CTL;
    rq.ocf = OCF_LE_SET_ADVERTISING_PARAMETERS;
    rq.cparam = &adv_params_cp;
    rq.clen = LE_SET_ADVERTISING_PARAMETERS_CP_SIZE;
    rq.rparam = &status;
    rq.rlen = 1;

    int ret = hci_send_req(device_handle, &rq, 1000);
    if (ret < 0) {
        /* handle error */
        hci_close_dev(device_handle);
        fprintf(stderr, "Can't send request %s (%d)\n", strerror(errno), errno);
        return (1);
    }

    le_set_advertise_enable_cp advertise_cp;
    memset(&advertise_cp, 0, sizeof(advertise_cp));
    advertise_cp.enable = 0x01;

    memset(&rq, 0, sizeof(rq));
    rq.ogf = OGF_LE_CTL;
    rq.ocf = OCF_LE_SET_ADVERTISE_ENABLE;
    rq.cparam = &advertise_cp;
    rq.clen = LE_SET_ADVERTISE_ENABLE_CP_SIZE;
    rq.rparam = &status;
    rq.rlen = 1;

    ret = hci_send_req(device_handle, &rq, 1000);

    if (ret < 0) {
        /* handle error */
        hci_close_dev(device_handle);
        fprintf(stderr, "Can't send request %s (%d)\n", strerror(errno), errno);
        return (1);
    }

    le_set_advertising_data_cp adv_data_cp;
    memset(&adv_data_cp, 0, sizeof(adv_data_cp));

    uint8_t segment_length = 1;
    adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(EIR_FLAGS);
    segment_length++;
    adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(0x1A);
    segment_length++;
    adv_data_cp.data[adv_data_cp.length] = htobs(segment_length - 1);

    adv_data_cp.length += segment_length;

    segment_length = 1;
    adv_data_cp.data[adv_data_cp.length + segment_length] =
        htobs(EIR_MANUFACTURE_SPECIFIC);
    segment_length++;
    adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(0x4C);
    segment_length++;
    adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(0x00);
    segment_length++;
    adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(0x02);
    segment_length++;
    adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(0x15);
    segment_length++;

    unsigned int *uuid = uuid_str_to_data(advertising_uuid);
    int i;
    for (i = 0; i < strlen(advertising_uuid) / 2; i++) {
        adv_data_cp.data[adv_data_cp.length + segment_length] = htobs(uuid[i]);
        segment_length++;
    }

    /* RSSI calibration */
    adv_data_cp.data[adv_data_cp.length + segment_length] =
        htobs(twoc(rssi_value, 8));
    segment_length++;

    adv_data_cp.data[adv_data_cp.length] = htobs(segment_length - 1);

    adv_data_cp.length += segment_length;

    memset(&rq, 0, sizeof(rq));
    rq.ogf = OGF_LE_CTL;
    rq.ocf = OCF_LE_SET_ADVERTISING_DATA;
    rq.cparam = &adv_data_cp;
    rq.clen = LE_SET_ADVERTISING_DATA_CP_SIZE;
    rq.rparam = &status;
    rq.rlen = 1;

    ret = hci_send_req(device_handle, &rq, 1000);

    hci_close_dev(device_handle);

    if (ret < 0) {
        /* handle error */
        fprintf(stderr, "Can't send request %s (%d)\n", strerror(errno), errno);
        return (1);
    }

    if (status) {
        /* handle error */
        fprintf(stderr, "LE set advertise returned status %d\n", status);
        return (1);
    }
}

/*
 *  disable_advertising:
 *
 *  @todo Determines the advertising capabilities and disables advertising.
 *
 *  Parameters:
 *
 *  None
 *
 *  Return value:
 *
 *  @todo
 */
int disable_advertising() {
    int device_id = hci_get_route(NULL);
    int device_handle = 0;
    if ((device_handle = hci_open_dev(device_id)) < 0) {
        /* handle error */
        fprintf(stderr, "Could not open device\n");
        return (1);
    }

    le_set_advertise_enable_cp advertise_cp;
    uint8_t status;

    memset(&advertise_cp, 0, sizeof(advertise_cp));

    struct hci_request rq;
    memset(&rq, 0, sizeof(rq));
    rq.ogf = OGF_LE_CTL;
    rq.ocf = OCF_LE_SET_ADVERTISE_ENABLE;
    rq.cparam = &advertise_cp;
    rq.clen = LE_SET_ADVERTISE_ENABLE_CP_SIZE;
    rq.rparam = &status;
    rq.rlen = 1;

    int ret = hci_send_req(device_handle, &rq, 1000);

    hci_close_dev(device_handle);

    if (ret < 0) {
        /* handle error */
        fprintf(stderr, "Can't set advertise mode: %s (%d)\n", strerror(errno),
                errno);
        return (1);
    }

    if (status) {
        /* handle error */
        fprintf(stderr, "LE set advertise enable on returned status %d\n",
                status);
        return (1);
    }
}

/*
 *  ble_beacon:
 *
 *  @todo
 *
 *  Parameters:
 *
 *  ptr - @todo
 *
 *  Return value:
 *
 *  None
 */
void *ble_beacon(void *ptr) {
    int rc = enable_advertising(300, ptr, 20);

    /* @todo */
    if (rc == 0) {
        struct sigaction sigint_handler;
        sigint_handler.sa_handler = ctrlc_handler;
        sigemptyset(&sigint_handler.sa_mask);
        sigint_handler.sa_flags = 0;

        if (sigaction(SIGINT, &sigint_handler, NULL) == -1) {
            /* handle error */
            fprintf(stderr, "sigaction error\n");
            exit(EXIT_FAILURE);
        }

        fprintf(stderr, "Hit ctrl-c to stop advertising\n");

        while (g_done == false) {
            sleep(1);
        }

        /* When signal received, disable message advertising */
        fprintf(stderr, "Shutting down\n");
        disable_advertising();
    }
}

void print_array() {
    int max_devices = atoi(g_config.max_devices);
    int i;
    printf("%s", "Array: ");
    for (i = 0; i < max_devices; i++) {
        printf("%d ", g_idle_handler[i].idle);
    }
    printf("\n");
}

int main(int argc, char **argv) {
    char hex_c[MAX_BUFFER];          // buffer that contains the local of beacon
    pthread_t cleanup_push_list_id;  // cleanup_push_list thread ID
    pthread_t ble_beacon_id;         // ble_beacon thread ID
    pthread_t queue_to_array_id;     // queue_to_array thread ID
    int i;                           // iterator to loop through push list
    int j;
    bool cancelled = false;  // indicator of stopping the scanning

    /* Load Config */
    g_config = get_config(CONFIG_FILENAME);
    int max_devices = atoi(g_config.max_devices);
    // printf("%d\n", max_devices);
    g_idle_handler = malloc(max_devices * sizeof(ThreadStatus));
    if (g_idle_handler == NULL) {
        /* handle error */
        fprintf(stderr, "Failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }
    for (i = 0; i < max_devices; i++) {
        for (j = 0; j < LEN_OF_MAC_ADDRESS; j++) {
            g_idle_handler[i].scanned_mac_addr[j] = 0;
            // memcpy(g_idle_handler[i].scanned_mac_addr, &a, 18);
        }
        g_idle_handler[i].idle = -1;
        g_idle_handler[i].is_waiting_to_send = false;
    }
    g_filepath = malloc(g_config.filepath_len + g_config.filename_len);
    if (g_filepath == NULL) {
        /* handle error */
        fprintf(stderr, "Failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }
    memcpy(g_filepath, g_config.filepath, g_config.filepath_len - 1);
    memcpy(g_filepath + g_config.filepath_len - 1, g_config.filename,
           g_config.filename_len - 1);
    coordinate_X.f = (float)atof(g_config.coordinate_X);
    coordinate_Y.f = (float)atof(g_config.coordinate_Y);
    coordinate_Z.f = (float)atof(g_config.coordinate_Z);

    /* Store coordinates of beacon location */
    sprintf(hex_c, "E2C56DB5DFFB48D2B060D0F5%02x%02x%02x%02x%02x%02x%02x%02x",
            coordinate_X.b[0], coordinate_X.b[1], coordinate_X.b[2],
            coordinate_X.b[3], coordinate_Y.b[0], coordinate_Y.b[1],
            coordinate_Y.b[2], coordinate_Y.b[3]);

    /* Device cleaner */
    if (pthread_create(&cleanup_push_list_id, NULL, (void *)cleanup_push_list,
                       NULL)) {
        /* handle error */
        fprintf(stderr, "Error with cleanup_push_list using pthread_create\n");
        return -1;
    }

    /* Enable message advertising to BLE bluetooth devices */
    if (pthread_create(&ble_beacon_id, NULL, (void *)ble_beacon, hex_c)) {
        /* handle error */
        fprintf(stderr, "Error with ble_beacon using pthread_create\n");
        return -1;
    }

    /* @todo */
    if (pthread_create(&queue_to_array_id, NULL, (void *)queue_to_array,
                       NULL)) {
        fprintf(stderr, "Error with queue_to_array using pthread_create\n");
        return -1;
    }

    /* todo */
    pthread_t send_file_id[max_devices];
    for (i = 0; i < max_devices; i++) {
        pthread_create(&send_file_id[i], NULL, (void *)send_file, (void *)i);
    }

    /* Start scanning for devices */
    while (cancelled == false) {
        start_scanning();
    }

    free(g_filepath);
    return 0;
}