#include <stdio.h>
#include <string.h>
#include "../src/protocol.h"
int main(void){
char buf[256]; TruckInfo t; time_t ts;
format_hb(buf,sizeof(buf),"TRK12",31.0,35.0,6012,123);
if(!parse_hb(buf,&t,&ts)) return 1;


PingMsg p = {0};
strcpy(p.truck_id,"TRK12"); strcpy(p.user_id,"USR1");
strcpy(p.addr,"12 St"); strcpy(p.note,"2 cyl");
format_ping(buf,sizeof(buf),&p);
PingMsg p2; if(!parse_ping(buf,&p2)) return 2;


char id[16]; int eta, q; format_ack(buf,sizeof(buf),"TRK12",7,3);
if(!parse_ack(buf,id,&eta,&q)) return 3;
printf("ok\n"); return 0;
}