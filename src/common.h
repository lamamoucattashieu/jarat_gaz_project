#pragma once
#include <time.h>
#include <stdint.h>


#define MC_GROUP "239.255.0.1"
#define MC_PORT 5000
#define HB_INTERVAL_MS 1000
#define DROP_AGE_SEC 3
#define MAX_ID_LEN 16
#define MAX_LINE 512


#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif


typedef struct {
char id[MAX_ID_LEN];
double lat, lon;
int tcp_port;
time_t last_seen;
struct in_addr last_ip; 
} TruckInfo;


typedef struct {
char truck_id[MAX_ID_LEN];
char user_id[MAX_ID_LEN];
char addr[128];
char note[64];
} PingMsg;