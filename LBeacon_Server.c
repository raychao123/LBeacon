/* A simple server in the internet domain using TCP
   The port number is passed as an argument */
#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
int sockfd, newsockfd, portno, clilen;
unsigned char buffer[256];
struct xbee_conAddress {
    unsigned char broadcast; /* if set, this address just sent us a broadcast message */

    unsigned char addr16_enabled;
    unsigned char addr16[2];
    
    unsigned char addr64_enabled;
    unsigned char addr64[8];
    
    unsigned char endpoints_enabled;
    unsigned char endpoint_local;
    unsigned char endpoint_remote;
    
    unsigned char profile_enabled;
    unsigned short profile_id;
    
    unsigned char cluster_enabled;
    unsigned short cluster_id;
};
typedef struct 
{
    float coordinate[3];
    char GatewayIP[14];
    /* SH = 0xXX XX XX XX SL = 0xXX XX XX XX*/
    struct xbee_conAddress address;
    int TxPower;
}Beacon_IP_table;
Beacon_IP_table IPtable[255];
Beacon_IP_table IPtableBuffer;
void error(char *msg)
{
    perror(msg);
    exit(1);
}
void COMM_Parser(unsigned char COMM)
{
    
    int n,len;
switch (COMM)
    {
        case 0x30:
        //rcv IP table
        printf("%02X\n",COMM );
        n = read(newsockfd,&len,sizeof(len));
        if (n < 0) error("ERROR reading from socket");
        printf("%d\n",len );
        n = read(newsockfd,IPtable,len*sizeof(IPtableBuffer));
        if (n < 0) error("ERROR reading from socket");
        break;
           
    };

return;
}
void TCPinit()
{
 
     struct sockaddr_in serv_addr, cli_addr;
     int n;
    
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0) 
        error("ERROR opening socket");
     bzero((char *) &serv_addr, sizeof(serv_addr));
     portno = atoi("3333");
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              error("ERROR on binding");
     listen(sockfd,5);
     clilen = sizeof(cli_addr);
     newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
     if (newsockfd < 0) 
          error("ERROR on accept");


}

int main(int argc, char *argv[])
{
     int n;
     unsigned char COMM;
     bzero(buffer,256);
     printf("%d\n",sizeof(IPtableBuffer) );
     TCPinit();
     n = read(newsockfd,&COMM,1);

     //printf("Node:0x%02X%02X%02X%02X 0x%02X%02X%02X%02X\n",buffer[0],buffer[1],buffer[2],buffer[3],buffer[4],buffer[5],buffer[6],buffer[7]);
     COMM_Parser(COMM);
     if (n < 0) error("ERROR reading from socket");
     printf("Here is the message: %s\n",buffer);
     n = write(newsockfd,"I got your message",18);
     if (n < 0) error("ERROR writing to socket");
     return 0; 
}
