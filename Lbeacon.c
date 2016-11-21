/*gcc blueObex.c -g -o blueObex -I libxbee3/include/ -L libxbee3/lib/ -lxbee -lrt -lpthread -lbluetooth -lobexftp*/
#include "Lbeacon.h"
/*********************************************************************
 * Parser FUNCTIONS
 */
 void Packet2Uchar(struct Packet packet,unsigned char pkt[])
 {
    memcpy(pkt,&packet,sizeof(struct Packet));
    return;
 }
 struct Packet Uchar2Packet(unsigned char *pkt)
 {
   struct Packet packet;
   memcpy(&packet,pkt,74);
   return packet;
 }
 int CoordinatePare(float X,float Y)
 {
    int ret;

    return ret;
 }
void Parse_package(struct Packet packet)
{
    char *buf;
    char buff[sizeof(struct Packet)];
    struct Packet myPacket;
    switch (packet.CMD)
    {
        case 's':

        break;
        case 'c':

        break;
        case 'r':
            myPacket.CMD = 'v';
            buf = malloc(configstruct.coordinate_X_len);
            memcpy(buf,configstruct.coordinate_X,configstruct.coordinate_X_len);
            myPacket.coordinate_X = atof(buf);
            buf = malloc(configstruct.coordinate_Y_len);
            memcpy(buf,configstruct.coordinate_Y,configstruct.coordinate_Y_len);
            myPacket.coordinate_Y = atof(buf);
            myPacket.level = '1';
            myPacket.packet_no = 1;
            myPacket.data_len = 0;
            memcpy(buff,&myPacket,sizeof(struct Packet));
            xbee_conTx(con, NULL,buff);
        break;
        case 'a':

        break;

    };
    free(buf);
return;
}
/*********************************************************************
 * InterNet FUNCTIONS
 */
 void error(char *msg)
{
    perror(msg);
    exit(0);
}
 void myCB(struct xbee *xbee, struct xbee_con *con, struct xbee_pkt **pkt, void **data) {
    if ((*pkt)->dataLen > 0) {
        printf("rx: [%s]\n", (*pkt)->data);
        Parse_package(Uchar2Packet((*pkt)->data));
        //xbee_conCallbackSet(con, NULL, NULL);
    }
   
}

void *ZigBee_Receiver(void)
{
    int n;
    int len;
    unsigned char buffer[16];
    xbee_err ret;
    if ((ret = xbee_setup(&xbee, "xbee2", "/dev/ttyUSB0", 9600)) != XBEE_ENONE) {
        printf("ret: %d (%s)\n", ret, xbee_errorToStr(ret));
     return;
    }

    memset(&address, 0, sizeof(address));
    address.addr64_enabled = 1;
    address.addr64[0] = 0x00;
    address.addr64[1] = 0x00;
    address.addr64[2] = 0x00;
    address.addr64[3] = 0x00;
    address.addr64[4] = 0x00;
    address.addr64[5] = 0x00;
    address.addr64[6] = 0xFF;
    address.addr64[7] = 0xFF;
    if ((ret = xbee_conNew(xbee, &con, "Data", &address)) != XBEE_ENONE) {
        xbee_log(xbee, -1, "xbee_conNew() returned: %d (%s)", ret, xbee_errorToStr(ret));
       return;
    }
   
    if ((ret = xbee_conCallbackSet(con, myCB, NULL)) != XBEE_ENONE) {
        xbee_log(xbee, -1, "xbee_conCallbackSet() returned: %d", ret);
       return;
    }
    
    /* getting an ACK for a broadcast message is kinda pointless... */
    xbee_conSettings(con, NULL, &settings);
    settings.disableAck = 1;
    xbee_conSettings(con, &settings, NULL);

    while(1)
    {
        usleep(200000);
    }
   xbee_shutdown(xbee);
    return;
}

long long getSystemTime() {
    struct timeb t;
    ftime(&t);
    return 1000 * t.time + t.millitm;
}
int UsedAddrCheck(char addr[])
{
int IfUsed=0;
int i=0,j=0;
for(i=0;i<MAX_OF_DEVICE;i++)
{
   if(compare_strings(addr,UsedDeviceQueue.DeviceAppearAddr[i]) == 0)
   {
    IfUsed = 1;
    return IfUsed;

   }

}
for(i=0;i<MAX_OF_DEVICE;i++)
{
    if(UsedDeviceQueue.DeviceUsed[i] == 0){
        for(j=0;j<18;j++){
          UsedDeviceQueue.DeviceAppearAddr[i][j]=addr[j];
      }
      UsedDeviceQueue.DeviceUsed[i]=1;
      UsedDeviceQueue.DeviceAppearTime[i]=getSystemTime();
      return IfUsed;
}

}
return IfUsed;
}

int compare_strings(char a[], char b[])
{
    int c = 0;

    while (a[c] == b[c]) {
        if (a[c] == '\0' || b[c] == '\0')
            break;
        c++;
    }

    if (a[c] == '\0' && b[c] == '\0')
        return 0;
    else
        return -1;
}
void ifTequal(pthread_t tid) {
    int i;
    for (i = 0; i < NTHREADS; i++) {
        if (pthread_equal(tid, thread_id[i]))
            thread_id[i] = 0;

    }

}
static void print_result(bdaddr_t *bdaddr, char has_rssi, int rssi)
{
    char addr[18];

    ba2str(bdaddr, addr);

    printf("%17s", addr);
    if (has_rssi)
        printf(" RSSI:%d", rssi);
    else
        printf(" RSSI:n/a");
    printf("\n");
    fflush(NULL);
}
Threadaddr Taddr[30];
static void sendToPushDongle(bdaddr_t *bdaddr, char has_rssi, int rssi)
{
    int i = 0, j = 0, idle = -1;
    char addr[18];

    void * status;
    ba2str(bdaddr, addr);
    for (i = 0; i < PUSHDONGLES; i++)
        for (j = 0; j < NUMBER_OF_DEVICE_IN_EACH_PUSHDONGLE; j++) {
            if (compare_strings(addr, addrbufferlist[i * j + j]) == 0)
            {
                goto out;

            }
            if (IdleHandler[i * j + j] != 1 && idle == -1)
            {
                idle = i * j + j;


            }

        }
    if (idle != -1 && UsedAddrCheck(addr)==0)
    {
        Threadaddr *param = &Taddr[idle];
        IdleHandler[idle] = 1;
        printf("%d\n",sizeof(addr)/sizeof(addr[0]));
        for (i = 0; i < sizeof(addr)/sizeof(addr[0]); i++)
        {
            addrbufferlist[idle][i] = addr[i];
            param->addr[i] = addr[i];
        }
        param->threadId = idle;
        pthread_create( &Taddr[idle].t, NULL, send_file, &Taddr[idle]);
    }
out: return;

}
static void scanner_start()
{
    int dev_id, sock = 0;
    struct hci_filter flt;
    inquiry_cp cp;
    unsigned char buf[HCI_MAX_EVENT_SIZE], *ptr;
    hci_event_hdr *hdr;
    char canceled = 0;
    inquiry_info_with_rssi *info_rssi;
    inquiry_info *info;
    int results, i, len;
    struct pollfd p;

    //dev_id = hci_get_route(NULL);
    dev_id = 0;
    printf("%d",dev_id);
    sock = hci_open_dev( dev_id );
    if (dev_id < 0 || sock < 0) {
        perror("Can't open socket");
        return;
    }

    hci_filter_clear(&flt);
    hci_filter_set_ptype(HCI_EVENT_PKT, &flt);
    hci_filter_set_event(EVT_INQUIRY_RESULT, &flt);
    hci_filter_set_event(EVT_INQUIRY_RESULT_WITH_RSSI, &flt);
    hci_filter_set_event(EVT_INQUIRY_COMPLETE, &flt);
    if (setsockopt(sock, SOL_HCI, HCI_FILTER, &flt, sizeof(flt)) < 0) {
        perror("Can't set HCI filter");
        return;
    }
    hci_write_inquiry_mode(sock, 0x01, 10);
    if (hci_send_cmd(sock, OGF_HOST_CTL, OCF_WRITE_INQUIRY_MODE, WRITE_INQUIRY_MODE_RP_SIZE, &cp) < 0) {
      perror("Can't set inquiry mode");
     return;
    }

    memset (&cp, 0, sizeof(cp));
    cp.lap[2] = 0x9e;
    cp.lap[1] = 0x8b;
    cp.lap[0] = 0x33;
    cp.num_rsp = 0;
    cp.length = 0x30;

    printf("Starting inquiry with RSSI...\n");

    if (hci_send_cmd (sock, OGF_LINK_CTL, OCF_INQUIRY, INQUIRY_CP_SIZE, &cp) < 0) {
        perror("Can't start inquiry");
        return;
    }

    p.fd = sock;
    p.events = POLLIN | POLLERR | POLLHUP;

    while (!canceled) {
        p.revents = 0;

        /* poll the BT device for an event */
        if (poll(&p, 1, -1) > 0) {
            len = read(sock, buf, sizeof(buf));

            if (len < 0)
                continue;
            else if (len == 0)
                break;

            hdr = (void *) (buf + 1);
            ptr = buf + (1 + HCI_EVENT_HDR_SIZE);

            results = ptr[0];

            switch (hdr->evt) {
            case EVT_INQUIRY_RESULT:
                for (i = 0; i < results; i++) {
                    info = (void *)ptr + (sizeof(*info) * i) + 1;
                    print_result(&info->bdaddr, 0, 0);
                }
                break;

            case EVT_INQUIRY_RESULT_WITH_RSSI:
                for (i = 0; i < results; i++) {
                    info_rssi = (void *)ptr + (sizeof(*info_rssi) * i) + 1;
                    print_result(&info_rssi->bdaddr, 1, info_rssi->rssi);
                    if(info_rssi->rssi>RSSI_RANGE)
                    sendToPushDongle(&info_rssi->bdaddr, 1, info_rssi->rssi);
                    
                }
                break;

            case EVT_INQUIRY_COMPLETE:
                canceled = 1;
    
                break;
            }

        }

    }
    printf("Scaning done\n");
    close(sock);
}
//file sending function, used in new threads
void *send_file(void *ptr)
{
    int i = 0;
    Threadaddr *Pigs = (Threadaddr*)ptr;
    struct timeval start, end;
    char *address = NULL;
    int dev_id,sock;
    int channel = -1;
    //char *filepath = "/home/pi/smsb1.txt";
    char *filename;
    obexftp_client_t *cli = NULL; /*!!!*/
    int ret;
    pthread_t tid = pthread_self();
    if (Pigs->threadId >= BLOCKOFDONGLE)
    {
        dev_id = 2;
    }
    else
    {
        dev_id = 1;
    }
    sock = hci_open_dev(dev_id);
    if (dev_id < 0 || sock < 0) {
        perror("opening socket");
        pthread_exit(NULL);
    }
    printf("Thread number %d\n", Pigs->threadId);
    //pthread_exit(0);
    gettimeofday( &start, NULL );
    long long start1 = getSystemTime();
    address = (char *)Pigs->addr;
    channel = obexftp_browse_bt_push(address); /*!!!*/
    /* Extract basename from file path */
    filename = strrchr(filepath, '/');
    if (!filename)
        filename = filepath;
    else
        filename++;
    printf("Sending file %s to %s\n", filename, address);
    /* Open connection */
    cli = obexftp_open(OBEX_TRANS_BLUETOOTH, NULL, NULL, NULL); /*!!!*/
    long long end1 = getSystemTime();

    printf("time: %lld ms\n", end1 - start1);
    if (cli == NULL) {
        fprintf(stderr, "Error opening obexftp client\n");
        IdleHandler[Pigs->threadId] = 0;
        for (i = 0; i < 18; i++)
        {

            addrbufferlist[Pigs->threadId][i] = 0;
        }

        close( sock );
        pthread_exit(NULL);
    }
    /* Connect to device */
    ret = obexftp_connect_push(cli, address, channel); /*!!!*/

    if (ret < 0) {

        fprintf(stderr, "Error connecting to obexftp device\n");
        obexftp_close(cli);
        cli = NULL;
        IdleHandler[Pigs->threadId] = 0;
        for (i = 0; i < 18; i++)
        {

            addrbufferlist[Pigs->threadId][i] = 0;
        }
        close( sock );
        pthread_exit(NULL);
    }

    /* Push file */
    ret = obexftp_put_file(cli, filepath, filename); /*!!!*/
    if (ret < 0) {
        fprintf(stderr, "Error putting file\n");
    }

    /* Disconnect */
    ret = obexftp_disconnect(cli); /*!!!*/
    if (ret < 0) {
        fprintf(stderr, "Error disconnecting the client\n");
    }
    /* Close */
    obexftp_close(cli); /*!!!*/
    cli = NULL;
    IdleHandler[Pigs->threadId] = 0;
    for (i = 0; i < 18; i++)
    {

        addrbufferlist[Pigs->threadId][i] = 0;
    }
    close( sock );
    pthread_exit(0);
}
void *DeviceCleaner(void)
{
    while(1){
int i=0,j;

for(i=0;i<MAX_OF_DEVICE;i++)
    if(getSystemTime()-UsedDeviceQueue.DeviceAppearTime[i]>Timeout&&UsedDeviceQueue.DeviceUsed[i]==1)
    {
        printf("Cleanertime: %lld ms\n", getSystemTime()-UsedDeviceQueue.DeviceAppearTime[i]);
        for(j=0;j<18;j++)
        {
           UsedDeviceQueue.DeviceAppearAddr[i][j]=0;

        }
       UsedDeviceQueue.DeviceAppearTime[i]=0;
       UsedDeviceQueue.DeviceUsed[i]=0;

    }

}



}
int lengthOfU(unsigned char * str)
{
    int i = 0;

    while(*(str++)){
        i++;
        if(i == INT_MAX)
            return -1;
    }

    return i;
}
struct config get_config(char *filename) 
{
        struct config configstruct;
        FILE *file = fopen (filename, "r");

        if (file != NULL)
        { 
                char line[MAXBUF];
                int i = 0;

                while(fgets(line, sizeof(line), file) != NULL)
                {
                        char *cfline;
                        cfline = strstr((char *)line,DELIM);
                        cfline = cfline + strlen(DELIM);
    
                        if (i == 0){
                                memcpy(configstruct.my_zigbee_address,cfline,strlen(cfline));
                                configstruct.my_zigbee_address_len = strlen(cfline);
                                //printf("%s",configstruct.imgserver);
                        } else if (i == 1){
                                memcpy(configstruct.port,cfline,strlen(cfline));
                                configstruct.port_len = strlen(cfline);
                                //printf("%s",configstruct.ccserver);
                        } else if (i == 2){
                                memcpy(configstruct.hostname,cfline,strlen(cfline));
                                configstruct.hostname_len = strlen(cfline);
                                //printf("%s",configstruct.port);
                        } else if (i == 3){
                                memcpy(configstruct.filepath_head,cfline,strlen(cfline));
                                configstruct.filepath_head_len = strlen(cfline);
                                //printf("%s",configstruct.imagename);
                        } else if (i == 4){
                                memcpy(configstruct.filename,cfline,strlen(cfline));
                                configstruct.filename_len = strlen(cfline);
                                //printf("%s",configstruct.getcmd);
                        } else if (i == 5){
                                memcpy(configstruct.coordinate_X,cfline,strlen(cfline));
                                configstruct.coordinate_X_len = strlen(cfline);
                                //printf("%s",configstruct.coordinate_X);
                        } else if (i == 6){
                                memcpy(configstruct.coordinate_Y,cfline,strlen(cfline));
                                configstruct.coordinate_Y_len = strlen(cfline);
                                //printf("%s",configstruct.getcmd);
                        } else if (i == 7){
                                memcpy(configstruct.level,cfline,strlen(cfline));
                                configstruct.level_len = strlen(cfline);
                                //printf("%s",configstruct.getcmd);
                        }
                        
                        i++;
                } // End while
                fclose(file);
        } // End if file
        
                
        
        return configstruct;
}
/*
char *test;
test = malloc(configstruct.my_zigbee_address_len);
memcpy(test,configstruct.my_zigbee_address,configstruct.my_zigbee_address_len);
*/
/*********************************************************************
 * STARTUP FUNCTION
 */
int main(int argc, char **argv)
{
    int i=0;
    pthread_t Device_cleaner_id,ZigBee_id;
    configstruct = get_config(CONFIG_FILENAME);
    //*****Device Cleaner
    pthread_create(&Device_cleaner_id,NULL,(void *) DeviceCleaner,NULL);
    //*****ZigBee Receiver
    pthread_create(&ZigBee_id,NULL,(void *) ZigBee_Receiver,NULL);
    for(i=0;i<MAX_OF_DEVICE;i++)
    UsedDeviceQueue.DeviceUsed[i]= 0;
    while (1) {scanner_start();}

    return 0;
}
