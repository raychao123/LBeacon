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
 *      BeDIPS uses LBeacons to deliver users' 3D coordinates and textual
 *      descriptions of their locations to user devices. Basically, a LBeacon
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
 *
 */

#include "LBeacon.h"

/*
 *  get_system_time:
 *
 *  Helper function called by is_unused_addr to give each user's MAC address the
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
    /* @todo add comments */
    struct timeb t;
    long long system_time;
    ftime(&t);
    system_time = 1000 * t.time + t.millitm;
    return system_time;
}

/*
 *  is_unused_addr:
 *
 *  Helper function called by send_file to check whether the user's MAC address
 *  given as input is in the push list. If the user's MAC address is not in the
 *  push list, the function puts the user's MAC address in the push list and
 *  waits for timeout_cleaner to remove the user's MAC address from the list.
 *
 *  Parameters:
 *
 *  char addr[] - scanned MAC address of bluetooth device
 *
 *  Return value:
 *
 *  false - MAC address is in pushing list
 *  true - unused MAC address
 */
bool is_unused_addr(char addr[]) {
    /* @todo add comments */
    int i;
    int j;

    /* @todo */
    for (i = 0; i < MAX_DEVICES; i++) {
        if (0 == strcmp(addr, g_device_queue.discovered_device_addr[i])) {
            return true;
        }
    }

    /* @todo */
    for (i = 0; i < MAX_DEVICES; i++) {
        if (false == g_device_queue.is_used_device[i]) {
            for (j = 0; j < LEN_OF_MAC_ADDRESS; j++) {
                g_device_queue.discovered_device_addr[i][j] = addr[j];
            }
            g_device_queue.is_used_device[i] = true;
            g_device_queue.first_appearance_time[i] = get_system_time();
            return false;
        }
    }
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
 *  void *ptr - @todo
 *
 *  Return value:
 *
 *  None
 */
void *send_file(void *ptr) {
    /* @todo add comments */
    ThreadAddr *thread_addr = (ThreadAddr *)ptr;
    char *addr = NULL;
    char *filename;
    int channel = -1;
    int dev_id;
    int socket;
    int i;
    int ret;

    /* @todo */
    pthread_t tid = pthread_self();
    obexftp_client_t *cli = NULL;
    if (thread_addr->thread_id >= NUM_OF_DEVICES_IN_BLOCK_OF_PUSH_DONGLE) {
        dev_id = PUSH_DONGLE_B;
    } else {
        dev_id = PUSH_DONGLE_A;
    }
    socket = hci_open_dev(dev_id);
    if (0 > dev_id || 0 > socket) {
        /* handle error */
        perror("Error opening socket");
        pthread_exit(NULL);
    }
    printf("Thread number %d\n", thread_addr->thread_id);
    long long start = get_system_time();
    addr = (char *)thread_addr->addr;
    channel = obexftp_browse_bt_push(addr);

    /* Extract basename from filepath */
    filename = strrchr(g_filepath, '/');
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

    printf("time: %lld ms\n", end - start);
    if (cli == NULL) {
        /* handle error */
        fprintf(stderr, "Error opening obexftp client\n");
        g_idle_handler[thread_addr->thread_id] = 0;
        for (i = 0; i < LEN_OF_MAC_ADDRESS; i++) {
            g_pushed_user_addr[thread_addr->thread_id][i] = 0;
        }
        close(socket);
        pthread_exit(NULL);
    }

    /* Connect to device */
    ret = obexftp_connect_push(cli, addr, channel);

    if (0 > ret) {
        /* handle error */
        fprintf(stderr, "Error connecting to obexftp device\n");
        obexftp_close(cli);
        cli = NULL;
        g_idle_handler[thread_addr->thread_id] = 0;
        for (i = 0; i < LEN_OF_MAC_ADDRESS; i++) {
            g_pushed_user_addr[thread_addr->thread_id][i] = 0;
        }
        close(socket);
        pthread_exit(NULL);
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
    g_idle_handler[thread_addr->thread_id] = 0;
    for (i = 0; i < LEN_OF_MAC_ADDRESS; i++) {
        g_pushed_user_addr[thread_addr->thread_id][i] = 0;
    }
    close(socket);
    pthread_exit(0);
}

/*
 *  send_to_push_dongle:
 *
 *  @todo
 *
 *  Parameters:
 *
 *  bdaddr_t *bdaddr - bluetooth device address
 *  int rssi - RSSI value
 *
 *  Return value:
 *
 *  None
 */
static void send_to_push_dongle(bdaddr_t *bdaddr, int rssi) {
    /* @todo add comments */
    int idle = -1;
    int i;
    int j;
    char addr[LEN_OF_MAC_ADDRESS];

    /* Converts the bluetooth device address to string */
    ba2str(bdaddr, addr);

    /* @todo */
    for (i = 0; i < NUM_OF_PUSH_DONGLES; i++) {
        for (j = 0; j < MAX_DEVICES_HANDLED_BY_EACH_PUSH_DONGLE; j++) {
            if (0 == strcmp(addr, g_pushed_user_addr[i * j + j])) {
                return;
            }
            if (1 != g_idle_handler[i * j + j] && -1 == idle) {
                idle = i * j + j;
            }
        }
    }

    /* @todo */
    if (-1 != idle && false == is_unused_addr(addr)) {
        ThreadAddr *thread_addr = &g_thread_addr[idle];
        g_idle_handler[idle] = 1;
        printf("%zu\n", sizeof(addr) / sizeof(addr[0]));
        for (i = 0; i < sizeof(addr) / sizeof(addr[0]); i++) {
            g_pushed_user_addr[idle][i] = addr[i];
            thread_addr->addr[i] = addr[i];
        }
        thread_addr->thread_id = idle;
        if (pthread_create(&g_thread_addr[idle].thread, NULL, send_file,
                           &g_thread_addr[idle])) {
            /* handle error */
            fprintf(stderr, "Error with send_file using pthread_create\n");
        }
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
 *  bdaddr_t *bdaddr - bluetooth device address
 *  bool has_rssi - whether the bluetooth device has an RSSI value or not
 *  int rssi - RSSI value
 *
 *  Return value:
 *
 *  None
 */
static void print_RSSI_value(bdaddr_t *bdaddr, bool has_rssi, int rssi) {
    char addr[LEN_OF_MAC_ADDRESS];  // MAC address that will be printed

    /* Converts the bluetooth device address to string */
    ba2str(bdaddr, addr);

    printf("%17s", addr);
    if (has_rssi) {
        printf(" RSSI:%d", rssi);
    } else {
        printf(" RSSI:n/a");
    }
    printf("\n");
    fflush(NULL);

    // char *ret = choose_file("message1");
    // printf("%s\n", ret);
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
 *  bdaddr_t *bdaddr - bluetooth device address
 *  char *filename - name of the file where all the data will be stored
 *
 *  Return value:
 *
 *  None
 */
static void track_devices(bdaddr_t *bdaddr, char *filename) {
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
        fputs("LBeacon ID: ########", fd);
        fclose(fd);
        g_size_of_file++; /* increment line */
        g_initial_timestamp_of_file = timestamp;
        memset(&g_addr[0], 0, sizeof(g_addr));
    }

    /* If timestamp already exists add MAC address to end of previous line, else
     * create new line. Double check that MAC address is not already added at a
     * given timestamp using strstr(). */
    ba2str(bdaddr, g_addr);
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
        fputs(g_addr, output);
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
        if (strstr(buffer, g_addr) == NULL) {
            fputs(", ", output);
            fputs(g_addr, output);
        }
        fclose(output);
    }

    /* @todo: send file to gateway if exceeds 1000 lines or reaches 6 hours */
    // unsigned diff = timestamp - g_initial_timestamp_of_file;
    // if (1000 <= g_size_of_file || 21600 <= diff) {
    //     g_size_of_file = 0;
    //     g_most_recent_timestamp_of_file = 0;
    //     // send_file_to_gateway();
    // }
}

/*
 *  start_scanning:
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
static void start_scanning() {
    /* @todo add comments */
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

    dev_id = SCAN_DONGLE;
    // printf("%d", dev_id);

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

    while (!cancelled) {
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
                /* @todo */
                case EVT_INQUIRY_RESULT: {
                    for (i = 0; i < results; i++) {
                        info = (void *)ptr + (sizeof(*info) * i) + 1;
                        print_RSSI_value(&info->bdaddr, 0, 0);
                        track_devices(&info->bdaddr, "output.txt");
                    }
                } break;

                /* @todo */
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

                /* @todo */
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
 *  timeout_cleaner:
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
void *timeout_cleaner(void) {
    /* @todo add comments */
    int i;
    int j;

    /* @todo */
    while (1) {
        for (i = 0; i < MAX_DEVICES; i++) {
            if (get_system_time() - g_device_queue.first_appearance_time[i] >
                    TIMEOUT &&
                true == g_device_queue.is_used_device[i]) {
                printf("Cleaner time: %lld ms\n",
                       get_system_time() -
                           g_device_queue.first_appearance_time[i]);
                for (j = 0; j < LEN_OF_MAC_ADDRESS; j++) {
                    g_device_queue.discovered_device_addr[i][j] = 0;
                }
                g_device_queue.first_appearance_time[i] = 0;
                g_device_queue.is_used_device[i] = false;
            }
        }
    }
}

/*
 *  choose_file:
 *
 *  @todo Receive message and then sends user the filepath where message is
 *  located.
 *
 *  Parameters:
 *
 *  char *messagetosend - The name of the message file we want to retreive.
 *
 *  Return value:
 *
 *  ret - message filepath
 */
char *choose_file(char *messagetosend) {
    /* @todo add comments */
    DIR *groupdir;
    struct dirent *groupent;
    char messages[num_messages][256];
    char groups[num_groups][256];
    char path[256];
    int num_messages = atoi(g_config.num_messages);
    int num_groups = atoi(g_config.num_groups);
    int count = 0;
    int i = 0;
    char *ret;

    /* @todo */
    groupdir = opendir("/home/pi/LBeacon/messages/");
    if (groupdir) {
        /* stores all the files and directories within directory */
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

    /* @todo */
    memset(path, 0, 256);
    count = 0;
    for (i = 0; i < num_groups; i++) {
        /* combine strings to make file path */
        sprintf(path, "/home/pi/LBeacon/messages/");
        strcat(path, groups[i]);
        DIR *messagedir;
        struct dirent *messageent;
        messagedir = opendir(path);
        if (messagedir) {
            /* go through each message in directory and store each
             * file name */
            while ((messageent = readdir(messagedir)) != NULL) {
                if (strcmp(messageent->d_name, ".") != 0 &&
                    strcmp(messageent->d_name, "..") != 0) {
                    strcpy(messages[count], messageent->d_name);
                    if (strcmp(messages[count], messagetosend) == 0) {
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
 *  get_config
 *
 *  While not end of file, read config file line by line and store data into the
 *  global variable of a Config struct. The variable i is used to know which
 *  line is being processed.
 *
 *  Parameters:
 *
 *  char *filename - the name of the config file that stores all the beacon data
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

        /* @todo */
        while (fgets(line, sizeof(line), file) != NULL) {
            char *config_message;
            config_message = strstr((char *)line, DELIMITER);
            config_message = config_message + strlen(DELIMITER);
            if (0 == i) {
                memcpy(config.filepath, config_message, strlen(config_message));
                config.filepath_len = strlen(config_message);
            } else if (1 == i) {
                memcpy(config.filename, config_message, strlen(config_message));
                config.filename_len = strlen(config_message);
            } else if (2 == i) {
                memcpy(config.coordinate_X, config_message,
                       strlen(config_message));
                config.coordinate_X_len = strlen(config_message);
            } else if (3 == i) {
                memcpy(config.coordinate_Y, config_message,
                       strlen(config_message));
                config.coordinate_Y_len = strlen(config_message);
            } else if (4 == i) {
                memcpy(config.level, config_message, strlen(config_message));
                config.level_len = strlen(config_message);
            } else if (5 == i) {
                memcpy(config.rssi_coverage, config_message,
                       strlen(config_message));
                config.rssi_coverage_len = strlen(config_message);
            } else if (6 == i) {
                memcpy(config.num_groups, config_message,
                       strlen(config_message));
                config.num_groups_len = strlen(config_message);
            } else if (7 == i) {
                memcpy(config.num_messages, config_message,
                       strlen(config_message));
                config.num_messages_len = strlen(config_message);
            }
            i++;
        }
        fclose(file);
    }

    return config;
}

/*
 *  uuid_str_to_data:
 *
 *  Converts the uuid to a data value.
 *
 *  Parameters:
 *
 *  char *uuid - unique identifier @todo
 *
 *  Return value:
 *
 *  data - @todo
 */
unsigned int *uuid_str_to_data(char *uuid) {
    char conv[] = "0123456789ABCDEF";
    int len = strlen(uuid);
    unsigned int *data = (unsigned int *)malloc(sizeof(unsigned int) * len);
    if (data == NULL) {
        /* handle error */
        fprintf(stderr, "Failed to allocate memory\n");
        return NULL;
    }

    unsigned int *dp = data;
    char *cu = uuid;

    for (; cu < uuid + len; dp++, cu += 2) {
        *dp = ((strchr(conv, toupper(*cu)) - conv) * 16) +
              (strchr(conv, toupper(*(cu + 1))) - conv);
    }

    return data;
}

/*
 *  twoc:
 *
 *  @todo
 *
 *  Parameters:
 *
 *  int in - @todo
 *  int t - @todo
 *
 *  Return value:
 *
 *  @todo
 */
unsigned int twoc(int in, int t) {
    return (in < 0) ? (in + (2 << (t - 1))) : in;
}

/*
 *  enable_advertising:
 *
 *  Determines the advertising capabilities and enables advertising.
 *
 *  Parameters:
 *
 *  int advertising_interval - @todo
 *  char *advertising_uuid - @todo
 *  int rssi_value - @todo
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
        fprintf(stderr, "LE set advertise returned status %d\n", status);
        return (1);
    }
}

/*
 *  disable_advertising:
 *
 *  Determines the advertising capabilities and disables advertising.
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
        fprintf(stderr, "LE set advertise enable on returned status %d\n",
                status);
        return (1);
    }
}

/*
 *  ctrlc_handler:
 *
 *  If the user presses CTRL-C, the global variable g_done will be set to true
 *  and a signal will be thrown to stop running the LBeacon program.
 *
 *  Parameters:
 *
 *  int s - @todo
 *
 *  Return value:
 *
 *  None
 */
void ctrlc_handler(int s) { g_done = true; }

/*
 *  ble_beacon:
 *
 *  BLE beacon initialization
 *
 *  Parameters:
 *
 *  ptr - pointer
 *
 *  Return value:
 *
 *  None
 */
void *ble_beacon(void *ptr) {
    int rc = enable_advertising(300, ptr, 20);
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

        while (!g_done) {
            sleep(1);
        }

        fprintf(stderr, "Shutting down\n");
        disable_advertising();
    }
}

int main(int argc, char **argv) {
    char cmd[100];
    char ble_buffer[100];  // HCI command for BLE beacon
    char hex_c[32];
    pthread_t device_cleaner_id, ble_beacon_id;
    int i;

    /* Load Config */
    g_config = get_config(CONFIG_FILENAME);
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
    level.f = (float)atof(g_config.level);

    sprintf(hex_c, "E2C56DB5DFFB48D2B060D0F5%02x%02x%02x%02x%02x%02x%02x%02x",
            coordinate_X.b[0], coordinate_X.b[1], coordinate_X.b[2],
            coordinate_X.b[3], coordinate_Y.b[0], coordinate_Y.b[1],
            coordinate_Y.b[2], coordinate_Y.b[3]);

    /* Device Cleaner */
    if (pthread_create(&device_cleaner_id, NULL, (void *)timeout_cleaner,
                       NULL)) {
        /* handle error */
        fprintf(stderr, "Error with timeout_cleaner using pthread_create\n");
        return -1;
    }

    /* @todo */
    if (pthread_create(&ble_beacon_id, NULL, (void *)ble_beacon, hex_c)) {
        /* handle error */
        fprintf(stderr, "Error with ble_beacon using pthread_create\n");
        return -1;
    }

    /* Reset the device queue */
    for (i = 0; i < MAX_DEVICES; i++) {
        g_device_queue.is_used_device[i] = false;
    }

    /* Start scanning for devices */
    while (1) {
        start_scanning();
    }

    free(g_filepath);
    return 0;
}