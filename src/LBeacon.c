/*
 * Copyright (c) 2016 Academia Sinica, Institute of Information Science
 *
 * License:
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
 *     This file will detect a phone and then scan for its Bluetooth address.
 * Depending on the RSSI value, it will determine if it should send location
 * related files to the user. The detection is based on BLE or OBEX.
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
 *  Helper function called by check_addr_status to give each addr the start
 *  of the push time.
 *
 *  Parameters:
 *
 *  None
 *
 *  Return value:
 *
 *  System time
 */
long long get_system_time() {
    struct timeb t;
    ftime(&t);
    return 1000 * t.time + t.millitm;
}

/*
 *  check_addr_status:
 *
 *  Helper function called by send_file to check whether the addr given as input
 *  is in the push list. If the addr is not in the push list, the function puts
 *  the addr in the push list and waits for timeout to remove the addr from the
 *  list.
 *
 *  Parameters:
 *
 *  char addr[] - @todo
 *
 *  Return value:
 *
 * return false - addr in pushing list * true -
unused addr */
bool is_unused_addr(char addr[]) {
    int i;
    int j;
    for (i = 0; i < MAX_DEVICES; i++) {
        if (0 == strcmp(addr, g_device_queue.discovered_device_addr[i])) {
            return true;
        }
    }
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
 *  Sends the push message to the device. @todo(which device?)
 *
 *  Parameters:
 *
 *  void *ptr - The scanned bluetooth addr @todo
 *
 *  Return value:
 *
 *  None
 */
void *send_file(void *ptr) {
    ThreadAddr *thread_addr = (ThreadAddr *)ptr;
    char *addr = NULL;
    int channel = -1;

    char *filename;
    int dev_id;
    int socket;
    int i;
    int ret;
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
        perror("opening socket");
        pthread_exit(NULL);
    }
    printf("Thread number %d\n", thread_addr->thread_id);
    long long start = get_system_time();
    addr = (char *)thread_addr->addr;
    channel = obexftp_browse_bt_push(addr);

    /* Extract basename from file path */
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

    /* Push file */
    ret = obexftp_put_file(cli, g_filepath, filename);
    if (0 > ret) {
        /* handle error */
        fprintf(stderr, "Error putting file\n");
    }

    /* Disconnect */
    ret = obexftp_disconnect(cli);
    if (0 > ret) {
        /* handle error */
        fprintf(stderr, "Error disconnecting the client\n");
    }

    /* Close */
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
 *  Sends the MAC addr of user device to send_file function. @todo
 *
 *  Parameters:
 *
 *  bdaddr_t *bdaddr - Bluetooth addr
 *
 *  char has_rssi - @todo
 *
 *  int rssi - RSSI value
 *
 *  Return value:
 *
 *  None
 */
static void send_to_push_dongle(bdaddr_t *bdaddr, int rssi) {
    int idle = -1;
    int i;
    int j;
    char addr[LEN_OF_MAC_ADDRESS];

    ba2str(bdaddr, addr);
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
            fprintf(stderr, "g_thread_addr pthread_create error\n");
        }
    }
}

/*
 *  print_result:
 *
 *  Print the RSSI value of the user's addr scanned by the scan function.
 *
 *  Parameters:
 *
 *  bdaddr_t *bdaddr - Bluetooth addr
 *
 *  char has_rssi - has RSSI value or not
 *
 *  int rssi - RSSI value
 *
 *  Return value:
 *
 *  None
 */
static void print_RSSI_value(bdaddr_t *bdaddr, bool has_rssi, int rssi) {
    char *ret = choose_file("message1");
    // printf("%s\n", ret);
    char addr[LEN_OF_MAC_ADDRESS];
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
 *  Track the scanned MAC addresses under the LBeacon at a time.
 *
 *  Parameters:
 *
 *  bdaddr_t *bdaddr - Bluetooth addr
 *
 *  char *filename - @todo
 *
 *  Return value:
 *
 *  None
 */
static void track_devices(bdaddr_t *bdaddr, char *filename) {
    /* Initialize variables */
    int i = 0;
    int j = 0;
    int k = 0;

    /* Get current timestamp when tracking Bluetooth devices. If
     * file is empty,
     * create new file with LBeacon ID. */
    unsigned timestamp = (unsigned)time(NULL);
    char temp[10]; /* converting long long to char[] */
    sprintf(temp, "%u", timestamp);
    if (0 == g_size_of_file) {
        /* Overwrites the file and clears the content */
        FILE *fd = fopen(filename, "w+");
        if (fd == NULL) {
            /* handle error */
            perror("Error opening file.");
            return;
        }
        fputs("LBeacon ID: ########", fd);
        fclose(fd);
        g_size_of_file++; /* increment line */
        g_initial_timestamp_of_file = timestamp;
        memset(&g_addr[0], 0, sizeof(g_addr));
    }

    /* If timestamp already exists add MAC address to end of
     * previous line, else
     * create new line. Format timestamp and MAC addresses into a
     * char[] and
     * append new line to end of file; ":" to separate timestamp
     * with MAC
     * address and "," to separate different MAC addresses */
    ba2str(bdaddr, g_addr);
    if (timestamp != g_most_recent_timestamp_of_file) {
        FILE *output;
        char buffer[1024];
        output = fopen(filename, "a+");
        if (output == NULL) {
            /* handle error */
            perror("Error opening file.");
            return;
        }
        while (fgets(buffer, sizeof(buffer), output) != NULL) {
        }
        fputs("\n", output);
        fputs(temp, output);
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
            perror("Error opening file.");
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

    /* TODO: Send new file to gateway */
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
 *  Asynchronous scanning bluetooth device. @todo
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
    inquiry_cp cp;
    unsigned char buf[HCI_MAX_EVENT_SIZE];
    unsigned char *ptr;
    hci_event_hdr *hdr;
    inquiry_info_with_rssi *info_rssi;
    inquiry_info *info;
    char cancelled = 0;
    int dev_id = 0;
    int socket = 0;
    int len;
    int results;
    int i;
    struct pollfd p;

    dev_id = SCAN_DONGLE;
    // printf("%d", dev_id);

    /* Open Bluetooth device */
    socket = hci_open_dev(dev_id);
    if (0 > dev_id || 0 > socket) {
        /* handle error */
        perror("Can't open socket");
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
        perror("Can't set HCI filter");
        return;
    }
    hci_write_inquiry_mode(socket, 0x01, 10);
    if (0 > hci_send_cmd(socket, OGF_HOST_CTL, OCF_WRITE_INQUIRY_MODE,
                         WRITE_INQUIRY_MODE_RP_SIZE, &cp)) {
        /* handle error */
        perror("Can't set inquiry mode");
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
        perror("Can't start inquiry");
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
                case EVT_INQUIRY_RESULT: {
                    for (i = 0; i < results; i++) {
                        info = (void *)ptr + (sizeof(*info) * i) + 1;
                        print_RSSI_value(&info->bdaddr, 0, 0);
                        track_devices(&info->bdaddr, "output.txt");
                    }
                } break;

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

                case EVT_INQUIRY_COMPLETE: {
                    cancelled = 1;
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
 *  Working asynchronous thread of TIMEOUT cleaner. When
 *  Bluetooth was pushed by push function it addr will store in
 *  used list then wait for timeout to be remove from list.
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
    int i;
    int j;
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
 *  Receive message and then sends user the filepath where
 *  message is located.
 *
 *  Parameters:
 *
 *  char *messagetosend - The name of the message file we want to
 * retreive the
 *  file path for.
 *
 *  Return value:
 *
 *  ret - message filepath
 */
char *choose_file(char *messagetosend) {
    // printf("%s\n", "choose_file entered");
    int num_messages = atoi(g_config.num_messages);
    int num_groups = atoi(g_config.num_groups);
    char messages[num_messages][256];
    char groups[num_groups][256];
    int count = 0;
    int i = 0;
    DIR *groupdir;
    struct dirent *groupent;
    groupdir = opendir("/home/pi/LBeacon/messages/");
    if (groupdir) {
        /* stores all the files and directories within directory */
        while ((groupent = readdir(groupdir)) != NULL) {
            if (strcmp(groupent->d_name, ".") != 0 &&
                strcmp(groupent->d_name, "..") != 0) {
                strcpy(groups[count], groupent->d_name);
                // printf("%d, %s\n", count, groups[count]);
                count++;
            }
        }
        closedir(groupdir);
    } else {
        /* handle error */
        perror("Directories do not exist");
        return NULL;
    }
    char path[256];
    char *ret;
    memset(path, 0, 256);
    count = 0;
    for (i = 0; i < num_groups; i++) {
        /* combine strings to make file path */
        sprintf(path, "/home/pi/LBeacon/messages/");
        strcat(path, groups[i]);
        // printf("%s\n", path);
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
                    // printf("%d, %s\n", count, messages[count]);
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
 *  Read the config file and initialize parameters.
 *
 *  Parameters:
 *
 *  char *filename - @todo
 *
 *  Return value:
 *
 *  config - Config struct including filepath, coordinates, etc.
 */
Config get_config(char *filename) {
    Config config;
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        /* handle error */
        fprintf(stderr, "Error opening file\n");
    } else {
        char line[MAX_BUFFER];
        int i = 0;

        while (fgets(line, sizeof(line), file) != NULL) {
            char *config_message;
            config_message = strstr((char *)line, DELIMITER);
            config_message = config_message + strlen(DELIMITER);

            if (0 == i) {
                memcpy(config.filepath, config_message, strlen(config_message));
                config.filepath_len = strlen(config_message);
                /* printf("%s",config.imgserver); */
            } else if (1 == i) {
                memcpy(config.filename, config_message, strlen(config_message));
                config.filename_len = strlen(config_message);
                /* printf("%s",config.ccserver); */
            } else if (2 == i) {
                memcpy(config.coordinate_X, config_message,
                       strlen(config_message));
                config.coordinate_X_len = strlen(config_message);
                /* printf("%s",config.port); */
            } else if (3 == i) {
                memcpy(config.coordinate_Y, config_message,
                       strlen(config_message));
                config.coordinate_Y_len = strlen(config_message);
                /* printf("%s",config.imagename); */
            } else if (4 == i) {
                memcpy(config.level, config_message, strlen(config_message));
                config.level_len = strlen(config_message);
                /* printf("%s",config.getcmd); */
            } else if (5 == i) {
                memcpy(config.rssi_coverage, config_message,
                       strlen(config_message));
                config.rssi_coverage_len = strlen(config_message);
                /* printf("%s",config.coordinate_X); */
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
            /* End while */
        }
        fclose(file);
        /* End if file */
    }

    return config;
}

/* Start BLE */
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

unsigned int twoc(int in, int t) {
    return (in < 0) ? (in + (2 << (t - 1))) : in;
}
/*
 *  enable_advertising:
 *
 *  Determines the advertising capabilities and enables
 *  advertising.
 *
 *  Parameters:
 *
 *  int advertising_interval - @todo
 *
 *  char *advertising_uuid - @todo
 *
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
        fprintf(stderr, "Could not open device\n");
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

    // RSSI calibration
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
 *  Determines the advertising capabilities and disables
 *  advertising.
 *
 *  Parameters:
 *
 *  None
 *
 *  Return value:
 *
 *  @todo returns if successful
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

void ctrlc_handler(int s) { global_done = 1; }

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

        while (!global_done) {
            sleep(1);
        }

        fprintf(stderr, "Shutting down\n");
        disable_advertising();
    }
}

/* Startup function */
int main(int argc, char **argv) {
    char cmd[100];
    char ble_buffer[100]; /* HCI command for BLE beacon */
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
    // printf("%f\n", coordinate_X.f);

    sprintf(hex_c, "E2C56DB5DFFB48D2B060D0F5%02x%02x%02x%02x%02x%02x%02x%02x",
            coordinate_X.b[0], coordinate_X.b[1], coordinate_X.b[2],
            coordinate_X.b[3], coordinate_Y.b[0], coordinate_Y.b[1],
            coordinate_Y.b[2], coordinate_Y.b[3]);

    /* Device Cleaner */
    if (pthread_create(&device_cleaner_id, NULL, (void *)timeout_cleaner,
                       NULL)) {
        /* handle error */
        fprintf(stderr, "decive_cleaner_id pthread_create error\n");
        return -1;
    }
    if (pthread_create(&ble_beacon_id, NULL, (void *)ble_beacon, hex_c)) {
        /* handle error */
        fprintf(stderr, "ble_beacon_id pthread_create error\n");
        return -1;
    }

    for (i = 0; i < MAX_DEVICES; i++) {
        g_device_queue.is_used_device[i] = false;
    }
    while (1) {
        start_scanning();
    }

    free(g_filepath);
    return 0;
}
