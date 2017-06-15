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
 * File Description: This file will detect a phone and then scan it for its 
 *                   Bluetooth address. Depending on the RSSI value, it will determine if it should
 *                   location related files to the user. The detection is based on BLE or OBEX. 
 * File Name:
 *
 *      LBeacon.c
 *
 * Abstract:
 *
 *      BeDIPS uses LBeacons to deliver users' 3D coordinates and textual 
 *      descriptions of their locations to their devices. Basically, LBeacon is 
 *      an inexpensive, Bluetooth Smart Ready device. The 3D coordinates and 
 *      location descriptions of every LBeacon are retrieved from BeDIS
 *      (Building/environment Data and Information System) and stored locally
 *      during deployment and maintenance times. Once initialized, each LBeacon
 *      broadcasts its coordinates and location description to Bluetooth
 *      enabled devices within its coverage area. 
 *
 * Authors:
 *
 *      Jake Lee, jakelee@iis.sinica.edu.tw
 *
 */

#include "LBeacon.h"

/* gcc LBeacon.c -g -o LBeacon -L/usr/local/lib -lrt -lpthread -lmulticobex -lbfb -lbluetooth -lobexftp -lopenobex */

/*****************************************************************************
 * @fn      get_system_time
 *
 * @brief   helper function for check_addr_status to give each addr the start of 
 *          the push time.
 *
 * @thread_addr   none
 *
 * @return  System time
 */
long long get_system_time()
{
    struct timeb t;
    ftime(&t);
    return 1000 * t.time + t.millitm;
}

/*****************************************************************************
 * @fn      check_addr_status
 *
 * @brief   helper function for start_scanning used to check if the addr is pushing
 *          or not. If addr is not in the push list it will put the addr in
 *          the push list and wait for timeout to be removed from list.
 *
 * @thread_addr   addr - addr scanned by Scan function
 *
 * @return  0: addr in pushing list
 *          1: Unused addr
 */
int check_addr_status(char addr[])
{
    int if_used = 0;
    int i;
    int j;
    for (i = 0; i < MAX_DEVICES; i++)
    {
        if (0 == strcmp(addr, g_device_queue.appear_addr[i]))
        {
            if_used = 1;
            return if_used;
        }
    }
    for (i = 0; i < MAX_DEVICES; i++)
    {
        if (0 == g_device_queue.used[i])
        {
            for (j = 0; j < LEN_OF_MAC_ADDRESS; j++)
            {
                g_device_queue.appear_addr[i][j] = addr[j];
            }
            g_device_queue.used[i] = 1;
            g_device_queue.appear_time[i] = get_system_time();
            return if_used;
        }
    }
    return if_used;
}

/*****************************************************************************
 * @fn      send_file
 *
 * @brief   sends the push message to the device
 *
 * @thread_addr   ptr - scanned bluetooth addr
 *
 * @return  none
 */
void *send_file(void *ptr)
{
    ThreadAddr *thread_addr = (ThreadAddr *)ptr;
    char *addr = NULL;
    char *filename;
    int dev_id;
    int socket;
    int channel = -1;
    int i;
    obexftp_client_t *cli = NULL;
    int ret;
    pthread_t tid = pthread_self();
    if (thread_addr->thread_id >= NUM_OF_DEVICES_IN_BLOCK_OF_PUSH_DONGLE)
    {
        dev_id = PUSH_DONGLE_B;
    }
    else
    {
        dev_id = PUSH_DONGLE_A;
    }
    socket = hci_open_dev(dev_id);
    if (0 > dev_id || 0 > socket)
    {
        perror("opening socket");
        pthread_exit(NULL);
    }
    printf("Thread number %d\n", thread_addr->thread_id);
    // pthread_exit(0);
    long long start = get_system_time();
    addr = (char *)thread_addr->addr;
    channel = obexftp_browse_bt_push(addr);

    /* Extract basename from file path */
    filename = strrchr(g_filepath, '/');
    printf("%s", filename);
    if (!filename)
    {
        filename = g_filepath;
    }
    else
    {
        filename++;
    }
    printf("Sending file %s to %s\n", filename, addr);

    /* Open connection */
    cli = obexftp_open(OBEX_TRANS_BLUETOOTH, NULL, NULL, NULL);
    long long end = get_system_time();

    printf("time: %lld ms\n", end - start);
    if (cli == NULL)
    {
        fprintf(stderr, "Error opening obexftp client\n");
        g_idle_handler[thread_addr->thread_id] = 0;
        for (i = 0; i < LEN_OF_MAC_ADDRESS; i++)
        {
            g_pushed_user_addr[thread_addr->thread_id][i] = 0;
        }

        close(socket);
        pthread_exit(NULL);
    }

    /* Connect to device */
    ret = obexftp_connect_push(cli, addr, channel);

    if (0 > ret)
    {
        fprintf(stderr, "Error connecting to obexftp device\n");
        obexftp_close(cli);
        cli = NULL;
        g_idle_handler[thread_addr->thread_id] = 0;
        for (i = 0; i < LEN_OF_MAC_ADDRESS; i++)
        {
            g_pushed_user_addr[thread_addr->thread_id][i] = 0;
        }
        close(socket);
        pthread_exit(NULL);
    }

    /* Push file */
    ret = obexftp_put_file(cli, g_filepath, filename);
    if (0 > ret)
    {
        fprintf(stderr, "Error putting file\n");
    }

    /* Disconnect */
    ret = obexftp_disconnect(cli);
    if (0 > ret)
    {
        fprintf(stderr, "Error disconnecting the client\n");
    }

    /* Close */
    obexftp_close(cli);
    cli = NULL;
    g_idle_handler[thread_addr->thread_id] = 0;
    for (i = 0; i < LEN_OF_MAC_ADDRESS; i++)
    {
        g_pushed_user_addr[thread_addr->thread_id][i] = 0;
    }
    close(socket);
    pthread_exit(0);
}

/*****************************************************************************
 * @fn      send_to_push_dongle
 *
 * @brief   sends the MAC addr of device to send_file function
 *
 * @thread_addr   bdaddr - Bluetooth addr
 *                has_rssi - has RSSI value or not
 *                rssi - RSSI value
 * 
 * @return  none
 */
static void send_to_push_dongle(bdaddr_t *bdaddr, char has_rssi, int rssi)
{
    int idle = -1;
    int i;
    int j;
    char addr[LEN_OF_MAC_ADDRESS];

    ba2str(bdaddr, addr);
    for (i = 0; i < NUM_OF_PUSH_DONGLES; i++)
    {
        for (j = 0; j < MAX_DEVICES_HANDLED_BY_EACH_PUSH_DONGLE; j++)
        {
            if (0 == strcmp(addr, g_pushed_user_addr[i * j + j]))
            {
                return;
            }
            if (1 != g_idle_handler[i * j + j] && -1 == idle)
            {
                idle = i * j + j;
            }
        }
    }
    if (-1 != idle && 0 == check_addr_status(addr))
    {
        ThreadAddr *thread_addr = &g_thread_addr[idle];
        g_idle_handler[idle] = 1;
        printf("%zu\n", sizeof(addr) / sizeof(addr[0]));
        for (i = 0; i < sizeof(addr) / sizeof(addr[0]); i++)
        {
            g_pushed_user_addr[idle][i] = addr[i];
            thread_addr->addr[i] = addr[i];
        }
        thread_addr->thread_id = idle;
        pthread_create(&g_thread_addr[idle].thread, NULL, send_file, &g_thread_addr[idle]);
    }
}

/*****************************************************************************
 * @fn      print_result
 *
 * @brief   print the RSSI value of the user's addr scanned by the
 *          scan function
 *
 * @thread_addr   bdaddr - Bluetooth addr
 *                has_rssi - has RSSI value or not
 *                rssi - RSSI value
 *
 * @return  none
 */
static void print_result(bdaddr_t *bdaddr, char has_rssi, int rssi)
{
    char addr[LEN_OF_MAC_ADDRESS];
    ba2str(bdaddr, addr);
    printf("%17s", addr);
    if (has_rssi)
    {
        printf(" RSSI:%d", rssi);
    }
    else
    {
        printf(" RSSI:n/a");
    }
    printf("\n");
    fflush(NULL);
}

/*****************************************************************************
 * @fn      start_scanning
 *
 * @brief   asynchronous scanning bluetooth device
 *
 * @thread_addr   none
 *
 * @return  none
 */
static void start_scanning()
{
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
    printf("%d", dev_id);

    // Open Bluetooth device
    socket = hci_open_dev(dev_id);
    if (0 > dev_id || 0 > socket)
    {
        perror("Can't open socket");
        return;
    }

    // Setup filter
    hci_filter_clear(&flt);
    hci_filter_set_ptype(HCI_EVENT_PKT, &flt);
    hci_filter_set_event(EVT_INQUIRY_RESULT, &flt);
    hci_filter_set_event(EVT_INQUIRY_RESULT_WITH_RSSI, &flt);
    hci_filter_set_event(EVT_INQUIRY_COMPLETE, &flt);
    if (0 > setsockopt(socket, SOL_HCI, HCI_FILTER, &flt, sizeof(flt)))
    {
        perror("Can't set HCI filter");
        return;
    }
    hci_write_inquiry_mode(socket, 0x01, 10);
    if (0 > hci_send_cmd(socket, OGF_HOST_CTL, OCF_WRITE_INQUIRY_MODE,
                         WRITE_INQUIRY_MODE_RP_SIZE, &cp))
    {
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

    if (0 > hci_send_cmd(socket, OGF_LINK_CTL, OCF_INQUIRY, INQUIRY_CP_SIZE, &cp))
    {
        perror("Can't start inquiry");
        return;
    }

    p.fd = socket;
    p.events = POLLIN | POLLERR | POLLHUP;

    while (!cancelled)
    {
        p.revents = 0;

        // Poll the Bluetooth device for an event
        if (0 < poll(&p, 1, -1))
        {
            len = read(socket, buf, sizeof(buf));

            if (0 > len)
            {
                continue;
            }
            else if (0 == len)
            {
                break;
            }

            hdr = (void *)(buf + 1);
            ptr = buf + (1 + HCI_EVENT_HDR_SIZE);

            results = ptr[0];

            switch (hdr->evt)
            {
            case EVT_INQUIRY_RESULT:
            {
                for (i = 0; i < results; i++)
                {
                    info = (void *)ptr + (sizeof(*info) * i) + 1;
                    print_result(&info->bdaddr, 0, 0);
                }
            }
            break;

            case EVT_INQUIRY_RESULT_WITH_RSSI:
            {
                for (i = 0; i < results; i++)
                {
                    info_rssi = (void *)ptr + (sizeof(*info_rssi) * i) + 1;
                    print_result(&info_rssi->bdaddr, 1, info_rssi->rssi);
                    if (info_rssi->rssi > RSSI_RANGE)
                    {
                        send_to_push_dongle(&info_rssi->bdaddr, 1, info_rssi->rssi);
                    }
                }
            }
            break;

            case EVT_INQUIRY_COMPLETE:
            {
                cancelled = 1;
            }
            break;

            default:
                break;
            }
        }
    }
    printf("Scanning done\n");
    close(socket);
}

/*****************************************************************************
 * @fn      timeout_cleaner
 *
 * @brief   thread of TIMEOUT cleaner. When Bluetooth was pushed by push
 *          function it addr will store in used list then wait for timeout
 *          to be remove from list.
 *
 * @thread_addr   none
 *
 * @return  none
 */
void *timeout_cleaner(void)
{
    int i;
    int j;
    while (1)
    {
        for (i = 0; i < MAX_DEVICES; i++)
        {
            if (get_system_time() - g_device_queue.appear_time[i] > TIMEOUT &&
                1 == g_device_queue.used[i])
            {
                printf("Cleaner time: %lld ms\n", get_system_time() - g_device_queue.appear_time[i]);
                for (j = 0; j < LEN_OF_MAC_ADDRESS; j++)
                {
                    g_device_queue.appear_addr[i][j] = 0;
                }
                g_device_queue.appear_time[i] = 0;
                g_device_queue.used[i] = 0;
            }
        }
    }
}

/*****************************************************************************
 * @fn      get_config
 *
 * @brief   read the config file and initialize parameter
 *
 * @thread_addr   filename - ame of config file
 *
 * @return  Config struct including filepath, coordinates, etc.
 */
Config get_config(char *filename)
{
    Config config;
    FILE *file = fopen(filename, "r");

    if (file != NULL)
    {
        char line[MAX_BUFFER];
        int i = 0;

        while (fgets(line, sizeof(line), file) != NULL)
        {
            char *config_message;
            config_message = strstr((char *)line, DELIMITER);
            config_message = config_message + strlen(DELIMITER);

            if (0 == i)
            {
                memcpy(config.filepath, config_message, strlen(config_message));
                config.filepath_len = strlen(config_message);
                // printf("%s",config.imgserver);
            }
            else if (1 == i)
            {
                memcpy(config.filename, config_message, strlen(config_message));
                config.filename_len = strlen(config_message);
                // printf("%s",config.ccserver);
            }
            else if (2 == i)
            {
                memcpy(config.coordinate_X, config_message, strlen(config_message));
                config.coordinate_X_len = strlen(config_message);
                // printf("%s",config.port);
            }
            else if (3 == i)
            {
                memcpy(config.coordinate_Y, config_message, strlen(config_message));
                config.coordinate_Y_len = strlen(config_message);
                // printf("%s",config.imagename);
            }
            else if (4 == i)
            {
                memcpy(config.level, config_message, strlen(config_message));
                config.level_len = strlen(config_message);
                // printf("%s",config.getcmd);
            }
            else if (5 == i)
            {
                memcpy(config.rssi_coverage, config_message, strlen(config_message));
                config.rssi_coverage_len = strlen(config_message);
                // printf("%s",config.coordinate_X);
            }
            i++;
        } // End while
        fclose(file);
    } // End if file

    return config;
}

/*****************************************************************************
 * STARTUP FUNCTION
 */
int main(int argc, char **argv)
{
    char cmd[100];
    char ble_buffer[100]; // HCI command for BLE beacon
    char hex_c[20];
    pthread_t Device_cleaner_id;
    int i;

    //*-----Initialize BLE--------
    sprintf(cmd, "hciconfig hci0 leadv 3");
    system(cmd);
    sprintf(cmd, "hciconfig hci0 noscan");
    system(cmd);
    sprintf(ble_buffer,
            "hcitool -i hci0 cmd 0x08 0x0008 1E 02 01 1A 1A FF 4C 00 02 15 E2 C5 "
            "6D B5 DF FB 48 D2 B0 60 D0 F5 11 11 11 11 00 00 00 00 C8 00");
    //*-----Initialize BLE--------

    //*-----Load config--------start
    g_config = get_config(CONFIG_FILENAME);
    g_filepath = malloc(g_config.filepath_len + g_config.filename_len);
    memcpy(g_filepath, g_config.filepath, g_config.filepath_len - 1);
    memcpy(g_filepath + g_config.filepath_len - 1, g_config.filename,
           g_config.filename_len - 1);
    coordinate_X.f = (float)atof(g_config.coordinate_X);
    coordinate_Y.f = (float)atof(g_config.coordinate_Y);
    printf("%s\n", hex_c);
    memcpy(ble_buffer + 98, hex_c, 11);
    printf("%s\n", hex_c);
    memcpy(ble_buffer + 110, hex_c, 11);
    system(ble_buffer);
    //*-----Load config--------end

    //  Device Cleaner
    pthread_create(&Device_cleaner_id, NULL, (void *)timeout_cleaner, NULL);

    for (i = 0; i < MAX_DEVICES; i++)
    {
        g_device_queue.used[i] = 0;
    }
    while (1)
    {
        start_scanning();
    }

    return 0;
}