#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "logger.h"


static FILE *gfp=NULL; static pthread_mutex_t gmu=PTHREAD_MUTEX_INITIALIZER;


static void sanitize(char *s){ for(char *p=s; *p; ++p){ if(*p=='\n' || *p=='\r' || *p==',') *p=' '; }}


int logger_open(const char *path){
gfp=fopen(path, "a"); if(!gfp) return -1;
fseek(gfp,0,SEEK_END); long sz=ftell(gfp);
if (sz==0) fprintf(gfp, "ts,truck_id,user_id,lat,lon,addr,note\n");
fflush(gfp); return 0;
}


void logger_close(void){ if(gfp){ fclose(gfp); gfp=NULL; }}


void logger_log_ping(time_t ts, const PingMsg *p, double truck_lat, double truck_lon){
if (!gfp) return; pthread_mutex_lock(&gmu);
char addr[128], note[64]; strncpy(addr,p->addr,sizeof(addr)); strncpy(note,p->note,sizeof(note));
addr[127]='\0'; note[63]='\0'; sanitize(addr); sanitize(note);
fprintf(gfp, "%ld,%s,%s,%.6f,%.6f,%s,%s\n", (long)ts, p->truck_id, p->user_id,
truck_lat, truck_lon, addr, note);
fflush(gfp); pthread_mutex_unlock(&gmu);
}