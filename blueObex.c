#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <pthread.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <obexftp/client.h> /*!!!*/
#define NTHREADS 10    //max number of threads used in multithreading
void *send_file(void *address); //prototype for the file sending function used in the new thread
//searching for devices code is taken from some bluez examples
int main(int argc, char **argv)
{
    while(1){
    inquiry_info *ii = NULL;
    int max_rsp, num_rsp;
    int dev_id, sock, len, flags;
    int i,j;
    char addr[10][19] = { 0 };

    char name[248] = { 0 };
    pthread_t thread_id[NTHREADS];
    dev_id = hci_get_route(NULL);
    sock = hci_open_dev( dev_id );
    if (dev_id < 0 || sock < 0) {
        perror("opening socket");
        exit(1);
    }
    len  = 8;
    max_rsp = 255;
    flags = IREQ_CACHE_FLUSH;
    ii = (inquiry_info*)malloc(max_rsp * sizeof(inquiry_info));
    num_rsp = hci_inquiry(dev_id, len, max_rsp, NULL, &ii, flags);
    if( num_rsp < 0 ) perror("hci_inquiry");
    printf ("%i devices discovered!\n",num_rsp);
    for (i = 0; i < num_rsp; i++) {
        ba2str(&(ii+i)->bdaddr, addr[i]);
        memset(name, 0, sizeof(name));
        if (hci_read_remote_name(sock, &(ii+i)->bdaddr, sizeof(name),
            name, 0) < 0)
        strcpy(name, "[unknown]");
        //start multithreading
        /* Create independent threads each of which will execute function */
        printf ("Starting thread %i...\n",i);
        pthread_create( &thread_id[i], NULL, send_file, (void*) addr[i]);
        //pthread_join( thread_id[i], NULL);   //wait for the thread to finish its execution; not sure if this is good here
    }
    for(i=0;i< num_rsp;i++)
    pthread_join( thread_id[i], NULL);

    //free the resources
    free( ii );
    close( sock );
}
    return 0;
}
//file sending function, used in new threads
void *send_file(void *ptr)
{
        char *address = NULL;
        int channel = -1;
        char *filepath = "/home/pi/smsb4.txt";
        char *filename;
        obexftp_client_t *cli = NULL; /*!!!*/
        int ret;
        printf("Thread number %ld\n", pthread_self());
        //pthread_exit(0);
        address = (char *)ptr;
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
        if (cli == NULL) {
                fprintf(stderr, "Error opening obexftp client\n");
                pthread_exit(NULL);
        }
        /* Connect to device */
        ret = obexftp_connect_push(cli, address, channel); /*!!!*/
        if (ret < 0) {
                fprintf(stderr, "Error connecting to obexftp device\n");
                obexftp_close(cli);
                cli = NULL;
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
        pthread_exit(0);
}