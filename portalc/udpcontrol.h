
#ifndef _UDPCONTROL_H 
#define _UDPCONTROL_H 

void udpcontrol_setup(void);
int udp_send_state( int *state, int * offset);
int udp_receive_state(int * source,int * state,int * offset);

#endif