/*gcc blueObex.c -g -o blueObex -I libxbee3/include/ -L libxbee3/lib/ -lxbee -lrt -lpthread -lbluetooth -lobexftp*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <pthread.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <signal.h>
#include <obexftp/client.h> /*!!!*/
#include <string.h>
#include <ctype.h>
#include <semaphore.h>
#include <errno.h>
#include <time.h>
#include <xbee.h>
#define MAX_OF_DEVICE 30 //max number of device to storage
#define NTHREADS 30    //max number of threads used in multithreading
#define PUSHDONGLES 2
#define NUMBER_OF_DEVICE_IN_EACH_PUSHDONGLE 9
#define BLOCKOFDONGLE 5
#define IDLE         -1
const long long Timeout = 20000; // TimeOut in 20s
char addr[30][18] = { 0 };
char addrbufferlist[PUSHDONGLES * NUMBER_OF_DEVICE_IN_EACH_PUSHDONGLE][18] = { 0 };
int IdleHandler[PUSHDONGLES * NUMBER_OF_DEVICE_IN_EACH_PUSHDONGLE] = {0};
pthread_t thread_id[NTHREADS] = {0};
void *send_file(void *address); //prototype for the file sending function used in the new thread
void *DeviceCleaner(void); //prototype for clean the device by time
//searching for devices code is taken from some bluez examples
//struct xbee_pkt **Global_pkt;
int xbee_pkt_count = 0;
sem_t ndComplete;
//TCP/IP parameters
int sockfd, portno;
/* IP table: |Location|GatewayIpv4|Beacon address|Tx Power| */

/*********************************************************************
 * TYPEDEFS
 */
typedef struct {
    char addr[18];
    int threadId;
    pthread_t t;


} Threadaddr;

typedef struct {
    long long DeviceAppearTime[MAX_OF_DEVICE];
    char DeviceAppearAddr[MAX_OF_DEVICE][18];
    char DeviceUsed[MAX_OF_DEVICE];

} DeviceQueue;
DeviceQueue UsedDeviceQueue;
/*-----------------------IP table-----------------------*/
typedef struct 
{
    int coordinate[3];
    char GatewayIP[4];
    /* SH = 0xXX XX XX XX SL = 0xXX XX XX XX*/
    struct xbee_conAddress address;
    int TxPower;
}Beacon_IP_table;
Beacon_IP_table IpTable[255];//storage is max to 255's Beacons address
int ZigBee_addr_Scan_count=0;

/*********************************************************************
 * Parser FUNCTIONS
 */
void COMM_Parser(char COMM)
{
    switch COMM
    {
        case 0x11:
            //function of send file
            break;
        case 0x12:
            //emergency state change
            break;
        case 0x13:
            //coordinate change
            break;
        case 0x14:
            //Timing
            break;
        case 0x15:
            //Fetch IP table(gateway)
            break; 
        case 0x16:
            //live or not rsp
            break;




    }


}
/*********************************************************************
 * InterNet FUNCTIONS
 */
 void error(char *msg)
{
    perror(msg);
    exit(0);
}
void TCPinit(char port[],char hostname[])
{


    struct sockaddr_in serv_addr;
    struct hostent *server;
    //char buffer[256];
    portno = atoi(port);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");
    //140.109.17.72
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) 
        error("ERROR connecting");
    //printf("Please enter the message: ");
    //bzero(buffer,256);
    //fgets(buffer,255,stdin);
    //n = write(sockfd,buffer,strlen(buffer));
    //if (n < 0) 
    //     error("ERROR writing to socket");
    //bzero(buffer,256);
    //n = read(sockfd,buffer,255);
    //if (n < 0) 
    //     error("ERROR reading from socket");
    //printf("%s\n",buffer);
    return;

    
}
void *T_TCP_Receiver(void)
{
    int n;
    char buffer[256];
    char COMM_TYPE[2];
    while(true)
    {
        bezero(COMM_TYPE,2);
        n = read(sockfd,COMM_TYPE,1);
        if (n<0)
            error("ERROR reading from socket");
        COMM_Parser(COMM_TYPE[0]);

    }





    return;
}


/*********************************************************************
 * ZigBee FUNCTIONS
 */

 /*********************************************************************
 * ZigBee: Node Detect Callback
 */
void nodeCB(struct xbee *xbee, struct xbee_con *con, struct xbee_pkt **pkt, void **data) {
    int i;
    int j;
    if (strncasecmp((*pkt)->atCommand, "ND", 2)) return;
    if ((*pkt)->dataLen == 0) {
        printf("Scan complete!\n");
        sem_post(&ndComplete);
        return;
    }

    if ((*pkt)->dataLen < 11) {
        printf("Received small packet...\n");
        return;
    }

    /*                     v   v       v   v   v   v      v   v   v   v       */
    printf("Node: %-20s  0x%02X%02X  0x%02X%02X%02X%02X 0x%02X%02X%02X%02X\n",
           &((*pkt)->data[10]),

           (*pkt)->data[0],
           (*pkt)->data[1],

           (*pkt)->data[2],
           (*pkt)->data[3],
           (*pkt)->data[4],
           (*pkt)->data[5],
           (*pkt)->data[6],
           (*pkt)->data[7],
           (*pkt)->data[8],
           (*pkt)->data[9]);
    for(j=0;j<8;j++)
    {
      
     IpTable[ZigBee_addr_Scan_count].address.addr64[j]=(*pkt)->data[j+2];

    }
    ZigBee_addr_Scan_count++;
}
/*********************************************************************
 * ZigBee: Node Detect function
 */
int ZigBee_Node_dectet()
{
void *d;
    struct xbee *xbee;
    struct xbee_con *con;
    xbee_err ret;
    unsigned char txRet;
    struct timespec to;

    if (sem_init(&ndComplete, 0, 0) != 0) {
        printf("sem_init() returned an error: %d - %s\n", errno, strerror(errno));
        return -1;
    }
    
    if ((ret = xbee_setup(&xbee, "xbee2", "/dev/ttyUSB1", 9600)) != XBEE_ENONE) {
        printf("ret: %d (%s)\n", ret, xbee_errorToStr(ret));
        return ret;
    }

    if ((ret = xbee_conNew(xbee, &con, "Local AT", NULL)) != XBEE_ENONE) {
        xbee_log(xbee, -1, "xbee_conNew() returned: %d (%s)", ret, xbee_errorToStr(ret));
        return ret;
    }

    if ((ret = xbee_conCallbackSet(con, nodeCB, NULL)) != XBEE_ENONE) {
        xbee_log(xbee, -1, "xbee_conCallbackSet() returned: %d", ret);
        return ret;
    }

    if ((ret = xbee_conTx(con, &txRet, "ND")) != XBEE_ENONE && ret != XBEE_ETIMEOUT) {
        xbee_log(xbee, -1, "xbee_conTx() returned: %d-%d", ret, txRet);
        return ret;
    }

    printf("ND Sent!... waiting for completion\n");

    clock_gettime(CLOCK_REALTIME, &to);
    to.tv_sec  += 30;
    if (sem_timedwait(&ndComplete, &to) != 0) {
        if (errno == ETIMEDOUT) {
            printf("Timeout while waiting for ND command to complete...\n");
        } else {
            printf("Error calling sem_timedwait()... sleeping for 30 seconds instead\n");
            usleep(30000000);
        }
    }

    if ((ret = xbee_conEnd(con)) != XBEE_ENONE) {
        xbee_log(xbee, -1, "xbee_conEnd() returned: %d", ret);
        return ret;
    }

    xbee_shutdown(xbee);

    return 0;
}

/*********************************************************************
 * ZigBee: Send Data function
 */
int ZigBeeSendData(struct xbee_conAddress address)
{
void *d;
struct xbee *xbee;
struct xbee_con *con;
unsigned char txRet;
xbee_err ret;

    if ((ret = xbee_setup(&xbee, "xbee2", "/dev/ttyUSB1", 9600)) != XBEE_ENONE) {
        printf("ret: %d (%s)\n", ret, xbee_errorToStr(ret));
        return ret;
    }
    if ((ret = xbee_conNew(xbee, &con, "Data", &address)) != XBEE_ENONE) {
        xbee_log(xbee, -1, "xbee_conNew() returned: %d (%s)", ret, xbee_errorToStr(ret));
        return ret;
    }
     xbee_conTx(con, NULL, "Hi!");//"Hi" need to fix!
    
    if ((ret = xbee_conEnd(con)) != XBEE_ENONE) {
        xbee_log(xbee, -1, "xbee_conEnd() returned: %d", ret);
        return ret;
    }

    xbee_shutdown(xbee);

    return 0;

}
/*********************************************************************
 * ZigBee: Receive Data callback
 */
void RcvCB(struct xbee *xbee, struct xbee_con *con, struct xbee_pkt **pkt, void **data) {
   if ((*pkt)->dataLen > 0) {
        printf("rx: [%s]\n", (*pkt)->data);
        
    }
}
/*********************************************************************
 * ZigBee: Send Data function
 */
 int ZigBeeReceiver(){
    void *d;
    struct xbee *xbee;
    struct xbee_con *con;
    struct xbee_conAddress address;
    xbee_err ret;

    if ((ret = xbee_setup(&xbee, "xbee2", "/dev/ttyUSB1", 9600)) != XBEE_ENONE) {
        printf("ret: %d (%s)\n", ret, xbee_errorToStr(ret));
        return ret;
    }
if ((ret = xbee_conNew(xbee, &con, "Data", &address)) != XBEE_ENONE) {
        xbee_log(xbee, -1, "xbee_conNew() returned: %d (%s)", ret, xbee_errorToStr(ret));
        return ret;
    }

    if ((ret = xbee_conDataSet(con, xbee, NULL)) != XBEE_ENONE) {
        xbee_log(xbee, -1, "xbee_conDataSet() returned: %d", ret);
        return ret;
    }

    if ((ret = xbee_conCallbackSet(con, RcvCB, NULL)) != XBEE_ENONE) {
        xbee_log(xbee, -1, "xbee_conCallbackSet() returned: %d", ret);
        return ret;
    }


    for (;;) {
        void *p;

        if ((ret = xbee_conCallbackGet(con, (xbee_t_conCallback*)&p)) != XBEE_ENONE) {
            xbee_log(xbee, -1, "xbee_conCallbackGet() returned: %d", ret);
            return ret;
        }

        if (p == NULL) break;

        usleep(1000000);
    }
    if ((ret = xbee_conEnd(con)) != XBEE_ENONE) {
        xbee_log(xbee, -1, "xbee_conEnd() returned: %d", ret);
        return ret;
    }

    xbee_shutdown(xbee);

    return 0;


 }


void *ZigBeeNetwork(void)
{



}
//gcc main.c -g -o main -I ../../../include/ -L ../../../lib -lxbee -lpthread -lrt
/*********************************************************************
 * Bluetooth OBEX FUNCTIONS
 */
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
    dev_id = hci_devid("00:1A:7D:DA:71:11");
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
    //if (hci_send_cmd(sock, OGF_HOST_CTL, OCF_WRITE_INQUIRY_MODE, WRITE_INQUIRY_MODE_RP_SIZE, &cp) < 0) {
    //  perror("Can't set inquiry mode");
    // return;
    //}

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
                    // print_result(&info_rssi->bdaddr, 1, info_rssi->rssi);
                    sendToPushDongle(&info_rssi->bdaddr, 1, info_rssi->rssi);
                    //printf("aa\n" );
                }
                break;

            case EVT_INQUIRY_COMPLETE:
                canceled = 1;
    
                break;
            }

        }

    }
    printf("ac\n");
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
    char *filepath = "/home/pi/smsb1.txt";
    char *filename;
    obexftp_client_t *cli = NULL; /*!!!*/
    int ret;
    pthread_t tid = pthread_self();
    if (Pigs->threadId >= BLOCKOFDONGLE)
    {
        dev_id = hci_devid("00:1A:7D:DA:71:03");
    }
    else
    {
        dev_id = hci_devid("00:1A:7D:DA:71:01");
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

/*********************************************************************
 * STARTUP FUNCTION
 */
int main(int argc, char **argv)
{
    int i=0;
    pthread_t Device_cleaner_id,ZigBee_id;
    ZigBee_Node_dectet();
    for(i=0;i<ZigBee_addr_Scan_count;i++)
    {
printf("Node:0x%02X%02X%02X%02X 0x%02X%02X%02X%02X\n",IpTable[i].address.addr64[0],IpTable[i].address.addr64[1],IpTable[i].address.addr64[2],IpTable[i].address.addr64[3],IpTable[i].address.addr64[4],IpTable[i].address.addr64[5],IpTable[i].address.addr64[6],IpTable[i].address.addr64[7] );

    }
    pthread_create(&Device_cleaner_id,NULL,(void *) DeviceCleaner,NULL);
    pthread_create(&ZigBee_id,NULL,(void *) ZigBeeNetwork,NULL);
    for(i=0;i<MAX_OF_DEVICE;i++)
    UsedDeviceQueue.DeviceUsed[i]= 0;
    while (1) {scanner_start();}

    return 0;
}
