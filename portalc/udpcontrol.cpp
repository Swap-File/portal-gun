
#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include  <sys/fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "udpcontrol.h"
#define BUFLEN 512
#define PORT 5005
#define IPADDRESS "192.168.1.255"

char localIP[20];
char buf[BUFLEN];
struct sockaddr_in sender_addr;
int sender_sockfd;
socklen_t slen=sizeof(sender_addr);
struct sockaddr_in receiver_addr, incoming_addr;
int receiver_sockfd;
socklen_t srlen=sizeof(incoming_addr);

void getIP(char *buffer);

void udpcontrol_setup(void){
	getIP(&localIP[0]);
	
	//printf("%s\n",localIP);
	//sending stuff
		
	if ((sender_sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1){
	printf("UDP_Control: Sender bind() failure\n");
	exit(1);
	}
	else
	printf("UDP_Control : Sender Socket() successful\n");
	
	fcntl(sender_sockfd, F_SETFL, FNDELAY); //Set to nonblocking mode so send to wont block on a full UDP buffer both SENDING and REC
	int yes=1;
	int ret=setsockopt(sender_sockfd, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
		
	bzero(&sender_addr, sizeof(sender_addr));
	sender_addr.sin_family = AF_INET;
	sender_addr.sin_port = htons(PORT);
	sender_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	//if (inet_aton(IPADDRESS, &sender_addr.sin_addr)==0)
	//{
	//	fprintf(stderr, "inet_aton() failed\n");
	//	exit(1);
	//}

	//receive stuff
	

	if ((receiver_sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1){
	printf("UDP_Control: receiver bind() failure\n");
	exit(1);
	}
	else
	printf("UDP_Control : Receiver Socket() successful\n");
	fcntl(receiver_sockfd, F_SETFL, FNDELAY);  //Set to nonblocking mode so send to wont block on a full UDP buffer both SENDING and REC
	bzero(&receiver_addr, sizeof(receiver_addr));
	receiver_addr.sin_family = AF_INET;
	receiver_addr.sin_port = htons(PORT);
	receiver_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	
	ret=setsockopt(receiver_sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) ;
	
	if (bind(receiver_sockfd, (struct sockaddr* ) &receiver_addr, sizeof(receiver_addr))==-1){
	printf("UDP_Receive : bind() failure\n");
	exit(1);
	}
	else
	printf("UDP_Control : bind() successful\n");
	}
	
int udp_send_state(int *state, int * offset){
		
		int n =sprintf (buf, "%d %d", *state, *offset);
		//n + 1 to include null terminator!
		return sendto(sender_sockfd, buf, n + 1, 0, (struct sockaddr*)&sender_addr, srlen);
}

int udp_receive_state(int * source,int * state, int * offset){
		int n = recvfrom(receiver_sockfd, buf, BUFLEN, 0, (struct sockaddr*)&incoming_addr, &slen);
		if (n > 0){
			//check of message is from self
			if ( strcmp (inet_ntoa(incoming_addr.sin_addr) , localIP) == 0){
				
				return 0;
			}
			buf[n] = '\0'; //make sure null terminator is installed
			int ipbytes[4];
			sscanf( inet_ntoa(incoming_addr.sin_addr), "%3u.%3u.%3u.%3u", &ipbytes[3], &ipbytes[2], &ipbytes[1], &ipbytes[0]);
			//printf("%d  %d %d %d\n", ipbytes[0] , ipbytes[1] , ipbytes[2], ipbytes[3]);
			sscanf( buf, "%d %d",state,offset);
			//*state = atoi(buf);
			//pack IP address into one 32bit unique int
			*source = ipbytes[0] | ipbytes[1] << 8 | ipbytes[2] << 16 | ipbytes[3] << 24;
		}
		return n;
	}

void getIP(char *buffer){
 int fd;
 struct ifreq ifr;

 fd = socket(AF_INET, SOCK_DGRAM, 0);

 /* I want to get an IPv4 IP address */
 ifr.ifr_addr.sa_family = AF_INET;

 /* I want IP address attached to "eth0" */
 strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);

 ioctl(fd, SIOCGIFADDR, &ifr);

 close(fd);

 /* display result */
 sprintf(buffer,"%s", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
 
 return ;
}
