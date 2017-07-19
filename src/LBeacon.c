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
 *      This file contains the program to allow the beacon to detect phones and
 *      then scan for the phones' Bluetooth addresses. Depending on the RSSI
 *      value, it will determine if it should send location related files to the
 *      user. The detection is based on BLE or OBEX.
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
 *  get_config:
 *
 *  While not end of file, this function will go throught the config file, read
 *  line by line, and store the data into the global variable of a Config
 *  struct.
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
    Config config; /* return variable is struct that contains all data */

    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        /* Error handling */
        perror("Error opening file");
    } else {
        /* stores the string of current line being read */
        char config_setting[CONFIG_BUFFER];

        /* keeps track of which line is being processed */
        int line = 0;

        /* If there are still lines to read in the config file, read the line
         * and store each information into the config struct. */
        while (fgets(config_setting, sizeof(config_setting), file) != NULL) {
            char *config_message;
            config_message = strstr((char *)config_setting, DELIMITER);
            config_message = config_message + strlen(DELIMITER);
            if (0 == line) {
                memcpy(config.coordinate_X, config_message,
                       strlen(config_message));
                config.coordinate_X_length = strlen(config_message);
            } else if (1 == line) {
                memcpy(config.coordinate_Y, config_message,
                       strlen(config_message));
                config.coordinate_Y_length = strlen(config_message);
            } else if (2 == line) {
                memcpy(config.coordinate_Z, config_message,
                       strlen(config_message));
                config.coordinate_Z_length = strlen(config_message);
            } else if (3 == line) {
                memcpy(config.filename, config_message, strlen(config_message));
                config.filename_length = strlen(config_message);
            } else if (4 == line) {
                memcpy(config.filepath, config_message, strlen(config_message));
                config.filepath_length = strlen(config_message);
            } else if (5 == line) {
                memcpy(config.maximum_number_of_devices, config_message,
                       strlen(config_message));
                config.maximum_number_of_devices_length =
                    strlen(config_message);
            } else if (6 == line) {
                memcpy(config.number_of_groups, config_message,
                       strlen(config_message));
                config.number_of_groups_length = strlen(config_message);
            } else if (7 == line) {
                memcpy(config.number_of_messages, config_message,
                       strlen(config_message));
                config.number_of_messages_length = strlen(config_message);
            } else if (8 == line) {
                memcpy(config.number_of_push_dongles, config_message,
                       strlen(config_message));
                config.number_of_push_dongles_length = strlen(config_message);
            } else if (9 == line) {
                memcpy(config.rssi_coverage, config_message,
                       strlen(config_message));
                config.rssi_coverage_length = strlen(config_message);
            } else if (10 == line) {
                memcpy(config.uuid, config_message, strlen(config_message));
                config.uuid_length = strlen(config_message);
            }
            line++;
        }
        fclose(file);
    }

    return config;
}

/*
 *  get_system_time:
 *
 *  This helper function fetches the current time according to the system clock
 *  in terms of the number of milliseconds since January 1, 1970.
 *
 *  Parameters:
 *
 *  None
 *
 *  Return value:
 *
 *  system_time - system time in milliseconds
 */
long long get_system_time() {
    struct timeb t;        /* struct that stores the time */
    long long system_time; /* return value as long long */
    ftime(&t);             /* get current time as Epoch and store in t */

    /* Convert time from Epoch to time in milliseconds as long long type */
    system_time = 1000 * t.time + t.millitm;

    return system_time;
}

/*
 *  is_used_addr:
 *
 *  This helper function checks whether the specified MAC address given as input
 *  is in the push list of the LBeacon. If it is, the function returns true. If
 *  the address is not in the list, the function adds the address into the list,
 *  stamps it with current system time, and then returns false. @todo
 *
 *  Parameters:
 *
 *  address - scanned MAC address of bluetooth device
 *
 *  Return value:
 *
 *  false - new MAC address, add to push list
 *  true - used MAC address
 */
bool is_used_addr(char address[]) {
    LinkedListNode *temp = linked_list_head;
    while (temp != NULL) {
        if (0 == strcmp(address, temp->data.scanned_mac_address)) {
            return true;
        }
        temp = temp->next;
    }
    return false;
}

/*
 *  send_to_push_dongle:
 *
 *  For each new MAC address of a scanned bluetooth device, the function adds
 *  the MAC address to an array of ThreadAddr struct that will give it a thread
 *  ID and sends the MAC address to send_file. The function only adds a MAC
 *  address if it isn't in the push list and does not have a thread ID. If the
 *  bluetooth device is already in the push list, the function won't send to
 *  push dongle again.
 *
 *  Parameters:
 *
 *  bluetooth_device_address - bluetooth device address
 *  rssi - RSSI value of bluetooth device
 *
 *  Return value:
 *
 *  None
 */
void send_to_push_dongle(bdaddr_t *bluetooth_device_address, int rssi) {
    int i; /* iterator through number of push dongle @todo */
    char address[LENGTH_OF_MAC_ADDRESS]; /* store the MAC address as string */

    /* Converts the bluetooth device address to string */
    ba2str(bluetooth_device_address, address);

    /* If the MAC address is already in the push list, return. THe function
     * won't send to push dongle again. */
    if (is_used_addr(address) == false) {
        PushList data;
        data.initial_scanned_time = get_system_time();
        for (i = 0; i < LENGTH_OF_MAC_ADDRESS; i++) {
            data.scanned_mac_address[i] = address[i];
        }
        insert_first(data);
        enqueue(address);
        print_linked_list();
        print_queue();
    }
}

/*
 *  queue_to_array:
 *
 *  This function will continuously look through the ThreadStatus array that
 *  contains all the send_file thread statuses. Once a thread becomes available,
 *  queue_to_array working thread will check if the queue has any MAC addresses
 *  that is waiting to send message file. If so, remove the MAC address from the
 *  queue and add it to the ThreadStatus array. We need to update the
 *  is_waiting_to_send boolean variable as true so the send_file function knows
 *  that a file needs to be sent to the scanned MAC address.
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
    int maximum_number_of_devices = atoi(g_config.maximum_number_of_devices);
    int i; /* @todo */
    int j;
    bool cancelled = false;

    while (cancelled == false) {
        for (i = 0; i < maximum_number_of_devices; i++) {
            char *address = peek();
            if (g_idle_handler[i].idle == -1 && address != NULL) {
                for (j = 0; j < LENGTH_OF_MAC_ADDRESS; j++) {
                    g_idle_handler[i].scanned_mac_address[j] = address[j];
                }
                dequeue();
                g_idle_handler[i].idle = i;
                g_idle_handler[i].is_waiting_to_send = true;
            }
        }
    }
}

/*
 *  send_file:
 *
 *  This function enables the caller to send the push message to the specified
 *  bluetooth device asynchronously.
 *
 *  [N.B. The beacon may still be scanning for other bluetooth devices for which
 *  the message is being pushed to.]
 *
 *  Parameters:
 *
 *  id - thread ID for each send_file thread
 *
 *  Return value:
 *
 *  None
 */
void *send_file(void *id) {
    obexftp_client_t *client = NULL;
    char *address = NULL;
    char *filename;
    int thread_id = (int)id;
    int channel = -1;
    int device_id = 0;
    int socket;
    int i; /* @todo */
    int j;
    int k;
    int return_value; /* if the operation failed, success is set */
    bool cancelled = false;

    int number_of_push_dongles = atoi(g_config.number_of_push_dongles);
    int maximum_number_of_devices = atoi(g_config.maximum_number_of_devices);
    int maximum_number_of_devices_per_dongle =
        maximum_number_of_devices / number_of_push_dongles;

    while (cancelled == false) {
        for (i = 0; i < maximum_number_of_devices; i++) {
            if (i == thread_id &&
                g_idle_handler[i].is_waiting_to_send == true) {
                /* Split the half of the thread ID to one dongle and the other
                 * half to the second dongle. */
                for (j = 0; j < number_of_push_dongles; j++) {
                    for (k = 0; k < maximum_number_of_devices_per_dongle; k++) {
                        if (thread_id ==
                            j * maximum_number_of_devices_per_dongle + k) {
                            device_id = j + 1;
                        }
                    }
                }

                /* Open socket and use current time as start time to determine
                 * how long it takes to send the file to the bluetooth device.
                 */
                socket = hci_open_dev(device_id);
                if (0 > device_id || 0 > socket) {
                    /* Error handling */
                    perror("Error opening socket");
                    for (j = 0; j < LENGTH_OF_MAC_ADDRESS; j++) {
                        g_idle_handler[i].scanned_mac_address[j] = 0;
                    }
                    g_idle_handler[i].idle = -1;
                    g_idle_handler[i].is_waiting_to_send = false;
                    break;
                }
                long long start = get_system_time();
                address = (char *)g_idle_handler[i].scanned_mac_address;
                channel = obexftp_browse_bt_push(address);

                /* Extract basename from filepath */
                filename = strrchr(g_filepath, '/');
                filename[g_config.filename_length] = '\0';
                printf("%s\n", filename);
                if (!filename) {
                    filename = g_filepath;
                } else {
                    filename++;
                }
                printf("Sending file %s to %s\n", filename, address);

                /* Open connection */
                client = obexftp_open(OBEX_TRANS_BLUETOOTH, NULL, NULL, NULL);
                long long end = get_system_time();
                printf("Time to open connection: %lld ms\n", end - start);
                if (client == NULL) {
                    /* Error handling */
                    perror("Error opening obexftp client");
                    for (j = 0; j < LENGTH_OF_MAC_ADDRESS; j++) {
                        g_idle_handler[i].scanned_mac_address[j] = 0;
                    }
                    g_idle_handler[i].idle = -1;
                    g_idle_handler[i].is_waiting_to_send = false;
                    close(socket);
                    break;
                }

                /* Connect to the scanned device */
                return_value = obexftp_connect_push(client, address, channel);

                /* If obexftp_connect_push returns a negative integer, then it
                 * goes to error handling. */
                if (0 > return_value) {
                    /* Error handling */
                    perror("Error connecting to obexftp device");
                    obexftp_close(client);
                    client = NULL;
                    for (j = 0; j < LENGTH_OF_MAC_ADDRESS; j++) {
                        g_idle_handler[i].scanned_mac_address[j] = 0;
                    }
                    g_idle_handler[i].idle = -1;
                    g_idle_handler[i].is_waiting_to_send = false;
                    close(socket);
                    break;
                }

                /* Push file to the scanned device */
                return_value = obexftp_put_file(client, g_filepath, filename);
                if (0 > return_value) {
                    /* Error handling */
                    perror("Error putting file");
                }

                /* Disconnect connection */
                return_value = obexftp_disconnect(client);
                if (0 > return_value) {
                    /* Error handling */
                    perror("Error disconnecting the client");
                }

                /* Close socket */
                obexftp_close(client);
                client = NULL;
                for (j = 0; j < LENGTH_OF_MAC_ADDRESS; j++) {
                    g_idle_handler[i].scanned_mac_address[j] = 0;
                }
                g_idle_handler[i].idle = -1;
                g_idle_handler[i].is_waiting_to_send = false;
                close(socket);
            }
        }
    }
}

/*
 *  print_RSSI_value:
 *
 *  This function prints the RSSI value along with the MAC address of the user's
 *  scanned bluetooth device. When running, we will continuously see a list of
 *  scanned bluetooth devices running in the console.
 *
 *  Parameters:
 *
 *  bluetooth_device_address - bluetooth device address
 *  has_rssi - whether the bluetooth device has an RSSI value or not
 *  rssi - RSSI value of bluetooth device
 *
 *  Return value:
 *
 *  None
 */
void print_RSSI_value(bdaddr_t *bluetooth_device_address, bool has_rssi,
                      int rssi) {
    char address[LENGTH_OF_MAC_ADDRESS]; /* MAC address that will be printed */

    /* Converts the bluetooth device address to string */
    ba2str(bluetooth_device_address, address);

    printf("%17s", address);
    if (has_rssi) {
        printf(" RSSI:%d", rssi);
    } else {
        printf(" RSSI:n/a");
    }
    printf("\n");
    fflush(NULL);
}

/*
 *  start_scanning:
 *
 *  This function scans continuously for bluetooth devices under the beacon
 *  until there is a need to cancel scanning. For each scanned device, it will
 *  fall under one of three cases. A bluetooth device with no RSSI value, a
 *  bluetooth device with a RSSI value, or if the user wants to cancel scanning.
 *  When the device is within RSSI value, the bluetooth device will be added to
 *  the push list so the message can be sent to the phone. This function is
 *  running under the main thread.
 *
 *  Parameters:
 *
 *  None
 *
 *  Return value:
 *
 *  None
 */
void start_scanning() {
    struct hci_filter filter;
    struct pollfd file_descriptor;
    unsigned char event_buffer[HCI_MAX_EVENT_SIZE];
    unsigned char *event_buffer_pointer;
    hci_event_hdr *event_handler;
    inquiry_cp inquiry_copy;
    inquiry_info_with_rssi *info_rssi;
    inquiry_info *info;
    bool cancelled = false;
    int device_id = 0;
    int socket = 0;
    int event_buffer_length;
    int results;
    int i;

    /* Open Bluetooth device */
    socket = hci_open_dev(device_id);
    if (0 > device_id || 0 > socket) {
        /* Error handling */
        perror("Error opening socket");
        return;
    }

    /* Setup filter */
    hci_filter_clear(&filter);
    hci_filter_set_ptype(HCI_EVENT_PKT, &filter);
    hci_filter_set_event(EVT_INQUIRY_RESULT, &filter);
    hci_filter_set_event(EVT_INQUIRY_RESULT_WITH_RSSI, &filter);
    hci_filter_set_event(EVT_INQUIRY_COMPLETE, &filter);
    if (0 > setsockopt(socket, SOL_HCI, HCI_FILTER, &filter, sizeof(filter))) {
        /* Error handling */
        perror("Error setting HCI filter");
        hci_close_dev(socket);
        return;
    }
    hci_write_inquiry_mode(socket, 0x01, 10);
    if (0 > hci_send_cmd(socket, OGF_HOST_CTL, OCF_WRITE_INQUIRY_MODE,
                         WRITE_INQUIRY_MODE_RP_SIZE, &inquiry_copy)) {
        /* Error handling */
        perror("Error setting inquiry mode");
        return;
    }

    memset(&inquiry_copy, 0, sizeof(inquiry_copy));
    inquiry_copy.lap[2] = 0x9e;
    inquiry_copy.lap[1] = 0x8b;
    inquiry_copy.lap[0] = 0x33;
    inquiry_copy.num_rsp = 0;
    inquiry_copy.length = 0x30;

    printf("Starting inquiry with RSSI...\n");

    if (0 > hci_send_cmd(socket, OGF_LINK_CTL, OCF_INQUIRY, INQUIRY_CP_SIZE,
                         &inquiry_copy)) {
        /* Error handling */
        perror("Error starting inquiry");
        return;
    }

    file_descriptor.fd = socket;
    file_descriptor.events = POLLIN | POLLERR | POLLHUP;

    while (cancelled == false) {
        file_descriptor.revents = 0;

        /* Poll the Bluetooth device for an event */
        if (0 < poll(&file_descriptor, 1, -1)) {
            event_buffer_length =
                read(socket, event_buffer, sizeof(event_buffer));

            if (0 > event_buffer_length) {
                continue;
            } else if (0 == event_buffer_length) {
                break;
            }

            event_handler = (void *)(event_buffer + 1);
            event_buffer_pointer = event_buffer + (1 + HCI_EVENT_HDR_SIZE);

            results = event_buffer_pointer[0];

            switch (event_handler->evt) {
                /* Scanned device with no RSSI value. */
                case EVT_INQUIRY_RESULT: {
                    for (i = 0; i < results; i++) {
                        info = (void *)event_buffer_pointer +
                               (sizeof(*info) * i) + 1;
                        print_RSSI_value(&info->bdaddr, 0, 0);
                        track_devices(&info->bdaddr, "output.txt");
                    }
                } break;

                /* Scanned device with RSSI value. If within range, send message
                 * to bluetooth device. */
                case EVT_INQUIRY_RESULT_WITH_RSSI: {
                    for (i = 0; i < results; i++) {
                        info_rssi = (void *)event_buffer_pointer +
                                    (sizeof(*info_rssi) * i) + 1;
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
 *  This function determines when the bluetooth device's MAC address will be
 *  removed from the used list. The MAC address is pushed by send_to_push_dongle
 *  and stored in the used list. This is continuously running in the background
 *  by a working thread alongside the main thread that is scanning for devices.
 *
 *  Parameters:
 *
 *  None
 *
 *  Return value:
 *
 *  None
 */
/* @todo documentation */
void *cleanup_push_list(void) {
    bool cancelled = false;

    /* In the background, continuously check if it has been 20 seconds since the
     * bluetooth device was added to the push list. If so, remove from list. */
    while (cancelled == false) {
        LinkedListNode *temp = linked_list_head;
        while (temp != NULL) {
            if (get_system_time() - temp->data.initial_scanned_time > TIMEOUT) {
                printf("Removed %s from linked list\n",
                       &temp->data.scanned_mac_address[0]);
                delete_node(temp->data);
            }
            temp = temp->next;
        }
    }
}

/*
 *  track_devices:
 *
 *  This function tracks the MAC addresses of scanned bluetooth devices under
 *  the beacon. An output file will contain each timestamp and the MAC addresses
 *  of the scanned bluetooth devices at the given timestamp. Format timestamp
 *  and MAC addresses into a char[] and append new line to end of file. " - " is
 *  used to separate timestamp with MAC address and ", " is used to separate
 *  each MAC address.
 *
 *  Parameters:
 *
 *  bluetooth_device_address - bluetooth device address
 *  filename - name of the file where all the data will be stored
 *
 *  Return value:
 *
 *  None
 */
void track_devices(bdaddr_t *bluetooth_device_address, char *filename) {
    char address[LENGTH_OF_MAC_ADDRESS];
    char long_long_to_string[10]; /* used for converting long long to string */

    /* Get current timestamp when tracking bluetooth devices. If file is empty,
     * create new file with LBeacon ID. */
    unsigned timestamp = (unsigned)time(NULL);
    sprintf(long_long_to_string, "%u", timestamp);
    if (0 == g_size_of_file) {
        FILE *file_descriptor =
            fopen(filename, "w+"); /* w+ overwrites the file */
        if (file_descriptor == NULL) {
            /* Error handling */
            perror("Error opening file");
            return;
        }
        fputs("LBeacon ID: ", file_descriptor);
        fputs(g_config.uuid, file_descriptor);
        fclose(file_descriptor);
        g_size_of_file++;
        g_initial_timestamp_of_file = timestamp;
        memset(&address[0], 0, sizeof(address));
    }

    /* If timestamp already exists add MAC address to end of previous line, else
     * create new line. Double check that MAC address is not already added at a
     * given timestamp using strstr. */
    ba2str(bluetooth_device_address, address);

    FILE *output;
    char line[TRACKING_BUFFER];
    output = fopen(filename, "a+"); /* a+ appends to the file */

    if (output == NULL) {
        /* Error handling */
        perror("Error opening file");
        return;
    }

    while (fgets(line, TRACKING_BUFFER, output) != NULL) {
    }

    if (timestamp != g_most_recent_timestamp_of_file) {
        fputs("\n", output);
        fputs(long_long_to_string, output);
        fputs(" - ", output);
        fputs(address, output);
        fclose(output);

        g_most_recent_timestamp_of_file = timestamp;
        g_size_of_file++;
    } else {
        if (strstr(line, address) == NULL) {
            fputs(", ", output);
            fputs(address, output);
        }
        fclose(output);
    }

    unsigned difference = timestamp - g_initial_timestamp_of_file;
    if (1000 <= g_size_of_file || 300 <= difference) {
        g_size_of_file = 0;
        g_most_recent_timestamp_of_file = 0;
        /* send to gateway */
    }
}

/*
 *  choose_file:
 *
 *  This function receives the name of the message file and then sends the user
 *  the filepath where the message is located. It goes through each directory in
 *  the messages folder of LBeacon and stores the directory names into an array.
 *  The function then goes through each of the directories to find the
 *  designated message we want to broadcast to the users under the beacon. Once
 *  the message name is located, the filepath is returned as a string.
 *
 *  Parameters:
 *
 *  message_to_send - name of the message file we want to retrieve
 *
 *  Return value:
 *
 *  return_value - message filepath
 */
char *choose_file(char *message_to_send) {
    DIR *groupdir;           /* dirent that stores list of directories */
    struct dirent *groupent; /* dirent struct that stores directory info */
    int number_of_messages;  /* number of message listed in config file */
    int number_of_groups;    /* number of groups listed in config file */
    char filepath[FILENAME_BUFFER]; /* store filepath of message_to_send */
    int message_count = 0; /* iterator for number of messages and groups */
    int i = 0;             /* iterator for number of groups */
    char *return_value;    /* return value; converts path to a char */

    /* Convert number of groups and messages from string to integer. */
    number_of_groups = atoi(g_config.number_of_groups);
    number_of_messages = atoi(g_config.number_of_messages);

    char groups[number_of_groups]
               [FILENAME_BUFFER]; /* array of buffer for group file names */
    char messages[number_of_messages]
                 [FILENAME_BUFFER]; /* array of buffer for message file names */

    /* Stores all the name of files and directories in groups. */
    groupdir = opendir("/home/pi/LBeacon/messages/");
    if (groupdir) {
        while ((groupent = readdir(groupdir)) != NULL) {
            if (strcmp(groupent->d_name, ".") != 0 &&
                strcmp(groupent->d_name, "..") != 0) {
                strcpy(groups[message_count], groupent->d_name);
                message_count++;
            }
        }
        closedir(groupdir);
    } else {
        /* Error handling */
        perror("Directories do not exist");
        return NULL;
    }

    memset(filepath, 0, FILENAME_BUFFER);
    message_count = 0;

    /* Go through each message in directory and store each file name. */
    for (i = 0; i < number_of_groups; i++) {
        /* Concatenate strings to make file path */
        sprintf(filepath, "/home/pi/LBeacon/messages/");
        strcat(filepath, groups[i]);
        DIR *messagedir;
        struct dirent *messageent;
        messagedir = opendir(filepath);
        if (messagedir) {
            while ((messageent = readdir(messagedir)) != NULL) {
                if (strcmp(messageent->d_name, ".") != 0 &&
                    strcmp(messageent->d_name, "..") != 0) {
                    strcpy(messages[message_count], messageent->d_name);
                    /* If message name found, return filepath. */
                    if (0 == strcmp(messages[message_count], message_to_send)) {
                        strcat(filepath, "/");
                        strcat(filepath, messages[message_count]);
                        return_value = &filepath[0];
                        return return_value;
                    }
                    message_count++;
                }
            }
            closedir(messagedir);
        } else {
            /* Error handling */
            perror("Message files do not exist");
            return NULL;
        }
    }

    /* Error handling */
    perror("Message files do not exist");
    return NULL;
}

/*
 *  pthread_create_error_message:
 *
 *  Print error message when creating a pthread @todo
 *
 *  Parameters:
 *
 *  error_code - return value of the pthread_create function
 *
 *  Return value:
 *
 *  None
 */
void pthread_create_error_message(int error_code) {
    if (error_code == 1)
        perror("[EPERM] Operation not permitted");
    else if (error_code == 11)
        perror("[EAGAIN] Resource temporarily unavailable");
    else if (error_code == 22)
        perror("[EINAL] Invalid argument");
}

/*
 *  enable_advertising:
 *
 *  @todo
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
    int device_handle = 0;
    if ((device_handle = hci_open_dev(device_id)) < 0) {
        /* Error handling */
        perror("Error opening device");
        return (1);
    }
    le_set_advertising_parameters_cp advertising_parameters_copy;
    memset(&advertising_parameters_copy, 0,
           sizeof(advertising_parameters_copy));
    advertising_parameters_copy.min_interval = htobs(advertising_interval);
    advertising_parameters_copy.max_interval = htobs(advertising_interval);
    advertising_parameters_copy.chan_map = 7;

    uint8_t status;
    struct hci_request request;
    memset(&request, 0, sizeof(request));
    request.ogf = OGF_LE_CTL;
    request.ocf = OCF_LE_SET_ADVERTISING_PARAMETERS;
    request.cparam = &advertising_parameters_copy;
    request.clen = LE_SET_ADVERTISING_PARAMETERS_CP_SIZE;
    request.rparam = &status;
    request.rlen = 1;

    int return_value = hci_send_req(device_handle, &request, 1000);
    if (return_value < 0) {
        /* Error handling */
        hci_close_dev(device_handle);
        fprintf(stderr, "Can't send request %s (%d)\n", strerror(errno), errno);
        return (1);
    }

    le_set_advertise_enable_cp advertisement_copy;
    memset(&advertisement_copy, 0, sizeof(advertisement_copy));
    advertisement_copy.enable = 0x01;

    memset(&request, 0, sizeof(request));
    request.ogf = OGF_LE_CTL;
    request.ocf = OCF_LE_SET_ADVERTISE_ENABLE;
    request.cparam = &advertisement_copy;
    request.clen = LE_SET_ADVERTISE_ENABLE_CP_SIZE;
    request.rparam = &status;
    request.rlen = 1;

    return_value = hci_send_req(device_handle, &request, 1000);

    if (return_value < 0) {
        /* Error handling */
        hci_close_dev(device_handle);
        fprintf(stderr, "Can't send request %s (%d)\n", strerror(errno), errno);
        return (1);
    }

    le_set_advertising_data_cp advertisement_data_copy;
    memset(&advertisement_data_copy, 0, sizeof(advertisement_data_copy));

    uint8_t segment_length = 1;
    advertisement_data_copy
        .data[advertisement_data_copy.length + segment_length] =
        htobs(EIR_FLAGS);
    segment_length++;
    advertisement_data_copy
        .data[advertisement_data_copy.length + segment_length] = htobs(0x1A);
    segment_length++;
    advertisement_data_copy.data[advertisement_data_copy.length] =
        htobs(segment_length - 1);

    advertisement_data_copy.length += segment_length;

    segment_length = 1;
    advertisement_data_copy
        .data[advertisement_data_copy.length + segment_length] =
        htobs(EIR_MANUFACTURE_SPECIFIC_DATA);
    segment_length++;
    advertisement_data_copy
        .data[advertisement_data_copy.length + segment_length] = htobs(0x4C);
    segment_length++;
    advertisement_data_copy
        .data[advertisement_data_copy.length + segment_length] = htobs(0x00);
    segment_length++;
    advertisement_data_copy
        .data[advertisement_data_copy.length + segment_length] = htobs(0x02);
    segment_length++;
    advertisement_data_copy
        .data[advertisement_data_copy.length + segment_length] = htobs(0x15);
    segment_length++;

    unsigned int *uuid = uuid_str_to_data(advertising_uuid);
    int i;
    for (i = 0; i < strlen(advertising_uuid) / 2; i++) {
        advertisement_data_copy
            .data[advertisement_data_copy.length + segment_length] =
            htobs(uuid[i]);
        segment_length++;
    }

    /* RSSI calibration */
    advertisement_data_copy
        .data[advertisement_data_copy.length + segment_length] =
        htobs(twoc(rssi_value, 8));
    segment_length++;

    advertisement_data_copy.data[advertisement_data_copy.length] =
        htobs(segment_length - 1);

    advertisement_data_copy.length += segment_length;

    memset(&request, 0, sizeof(request));
    request.ogf = OGF_LE_CTL;
    request.ocf = OCF_LE_SET_ADVERTISING_DATA;
    request.cparam = &advertisement_data_copy;
    request.clen = LE_SET_ADVERTISING_DATA_CP_SIZE;
    request.rparam = &status;
    request.rlen = 1;

    return_value = hci_send_req(device_handle, &request, 1000);

    hci_close_dev(device_handle);

    if (return_value < 0) {
        /* Error handling */
        fprintf(stderr, "Can't send request %s (%d)\n", strerror(errno), errno);
        return (1);
    }

    if (status) {
        /* Error handling */
        fprintf(stderr, "LE set advertise returned status %d\n", status);
        return (1);
    }
}

/*
 *  disable_advertising:
 *
 *  @todo
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
        /* Error handling */
        perror("Could not open device");
        return (1);
    }

    le_set_advertise_enable_cp advertisement_copy;
    uint8_t status;

    memset(&advertisement_copy, 0, sizeof(advertisement_copy));

    struct hci_request request;
    memset(&request, 0, sizeof(request));
    request.ogf = OGF_LE_CTL;
    request.ocf = OCF_LE_SET_ADVERTISE_ENABLE;
    request.cparam = &advertisement_copy;
    request.clen = LE_SET_ADVERTISE_ENABLE_CP_SIZE;
    request.rparam = &status;
    request.rlen = 1;

    int return_value = hci_send_req(device_handle, &request, 1000);

    hci_close_dev(device_handle);

    if (return_value < 0) {
        /* Error handling */
        fprintf(stderr, "Can't set advertise mode: %s (%d)\n", strerror(errno),
                errno);
        return (1);
    }

    if (status) {
        /* Error handling */
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
 *  beacon_location - @todo
 *
 *  Return value:
 *
 *  None
 */
void *ble_beacon(void *beacon_location) {
    int enable_advertising_success =
        enable_advertising(300, beacon_location, 20);

    if (enable_advertising_success == 0) {
        struct sigaction sigint_handler;
        sigint_handler.sa_handler = ctrlc_handler;
        sigemptyset(&sigint_handler.sa_mask);
        sigint_handler.sa_flags = 0;

        if (sigaction(SIGINT, &sigint_handler, NULL) == -1) {
            /* Error handling */
            perror("sigaction error");
            return;
        }

        perror("Hit ctrl-c to stop advertising");

        while (g_done == false) {
            sleep(1);
        }

        /* When signal received, disable message advertising */
        perror("Shutting down");
        disable_advertising();
    }
}

int main(int argc, char **argv) {
    pthread_t cleanup_push_list_id; /* cleanup_push_list thread ID */
    pthread_t ble_beacon_id;        /* ble_beacon thread ID */
    pthread_t queue_to_array_id;    /* queue_to_array thread ID */
    char hex_c[CONFIG_BUFFER]; /* buffer that contains the local of beacon */
    int thread_id;
    int i; /* iterator to loop through push list */
    int j;
    int return_value;
    bool cancelled = false; /* indicator of stopping the scanning */

    /* Load Config */
    g_config = get_config(CONFIG_FILENAME);

    int maximum_number_of_devices = atoi(g_config.maximum_number_of_devices);
    g_idle_handler = malloc(maximum_number_of_devices * sizeof(ThreadStatus));
    if (g_idle_handler == NULL) {
        /* Error handling */
        perror("Failed to allocate memory");
        return -1;
    }
    for (i = 0; i < maximum_number_of_devices; i++) {
        for (j = 0; j < LENGTH_OF_MAC_ADDRESS; j++) {
            g_idle_handler[i].scanned_mac_address[j] = 0;
        }
        g_idle_handler[i].idle = -1;
        g_idle_handler[i].is_waiting_to_send = false;
    }

    g_filepath = malloc(g_config.filepath_length + g_config.filename_length);
    if (g_filepath == NULL) {
        /* Error handling */
        perror("Failed to allocate memory");
        return -1;
    }
    memcpy(g_filepath, g_config.filepath, g_config.filepath_length - 1);
    memcpy(g_filepath + g_config.filepath_length - 1, g_config.filename,
           g_config.filename_length - 1);
    coordinate_X.f = (float)atof(g_config.coordinate_X);
    coordinate_Y.f = (float)atof(g_config.coordinate_Y);
    coordinate_Z.f = (float)atof(g_config.coordinate_Z);

    /* Store coordinates of beacon location */
    sprintf(hex_c, "E2C56DB5DFFB48D2B060D0F5%02x%02x%02x%02x%02x%02x%02x%02x",
            coordinate_X.b[0], coordinate_X.b[1], coordinate_X.b[2],
            coordinate_X.b[3], coordinate_Y.b[0], coordinate_Y.b[1],
            coordinate_Y.b[2], coordinate_Y.b[3]);

    /* Enable message advertising to BLE bluetooth devices */
    return_value =
        pthread_create(&ble_beacon_id, NULL, (void *)ble_beacon, hex_c);
    if (return_value != 0) {
        /* Error handling */
        perror("Error with ble_beacon using pthread_create");
        pthread_create_error_message(return_value);
        pthread_exit(NULL);
    }

    /* Clean up linked list */
    return_value = pthread_create(&cleanup_push_list_id, NULL,
                                  (void *)cleanup_push_list, NULL);
    if (return_value != 0) {
        /* Error handling */
        perror("Error with cleanup_push_list using pthread_create");
        pthread_create_error_message(return_value);
        pthread_exit(NULL);
    }

    /* Send MAC address in queue to an available thread */
    return_value =
        pthread_create(&queue_to_array_id, NULL, (void *)queue_to_array, NULL);
    if (return_value != 0) {
        perror("Error with queue_to_array using pthread_create");
        pthread_create_error_message(return_value);
        pthread_exit(NULL);
    }

    /* Send message to scanned MAC address */
    pthread_t send_file_id[maximum_number_of_devices];
    for (thread_id = 0; thread_id < maximum_number_of_devices; thread_id++) {
        return_value = pthread_create(&send_file_id[thread_id], NULL,
                                      (void *)send_file, (void *)thread_id);
        if (return_value != 0) {
            perror("Error with send_file using pthread_create");
            pthread_create_error_message(return_value);
            pthread_exit(NULL);
        }
    }

    /* Start scanning for bluetooth devices */
    while (cancelled == false) {
        start_scanning();
    }

    free(g_filepath);
    return 0;
}