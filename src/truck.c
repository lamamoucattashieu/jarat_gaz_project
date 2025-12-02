#define _POSIX_C_SOURCE 200809L
#include "protocol.h"
#include "util.h"
#include "logger.h"


static volatile int running=1;
static double g_lat=31.956, g_lon=35.945;
static int g_queue_len=0; static char g_truck_id[MAX_ID_LEN]="TRK01"; static int g_tcp_port=6012;
static int mc_fd=-1, listen_fd=-1; static struct sockaddr_in mc_addr;


static void on_sig(int s){ (void)s; running=0; }


static void* th_gps(void* _){ (void)_; while(running){ gps_step(&g_lat,&g_lon); usleep(300*1000); } return NULL; }


static void* th_hb(void* _){ (void)_; char line[MAX_LINE];
while(running){
format_hb(line,sizeof(line), g_truck_id, g_lat, g_lon, g_tcp_port, time(NULL));
sendto(mc_fd, line, strlen(line), 0, (struct sockaddr*)&mc_addr, sizeof(mc_addr));
usleep(HB_INTERVAL_MS*1000);
} return NULL; }


static void* th_worker(void *arg){ int sock = (int)(intptr_t)arg; char buf[MAX_LINE];
ssize_t n = recv_line_timeout(sock, buf, sizeof(buf), 2000);
if (n>0){ PingMsg p={0}; if (parse_ping(buf,&p)){
__sync_add_and_fetch(&g_queue_len,1);
int eta = 5 + __sync_fetch_and_add(&g_queue_len,0);
logger_log_ping(time(NULL), &p, g_lat, g_lon);
char out[MAX_LINE]; format_ack(out,sizeof(out),g_truck_id,eta,g_queue_len);
send_all_timeout(sock,out,strlen(out),2000);
__sync_sub_and_fetch(&g_queue_len,1);
}
}
close(sock); return NULL; }


int main(int argc, char **argv){
for(int i=1;i<argc;i++){
if (!strcmp(argv[i],"--id") && i+1<argc) strncpy(g_truck_id,argv[++i],MAX_ID_LEN);
else if(!strcmp(argv[i],"--tcp") && i+1<argc) g_tcp_port=atoi(argv[++i]);
else if(!strcmp(argv[i],"--start-lat") && i+1<argc) g_lat=atof(argv[++i]);
else if(!strcmp(argv[i],"--start-lon") && i+1<argc) g_lon=atof(argv[++i]);
}
signal(SIGINT,on_sig); signal(SIGTERM,on_sig);


gps_init(g_lat,g_lon,4.0);
if (udp_mc_sender(MC_GROUP, MC_PORT, &mc_fd, &mc_addr)<0){ perror("udp_mc_sender"); return 1; }
if (tcp_listen(g_tcp_port, 64, &listen_fd)<0){ perror("tcp_listen"); return 1; }
system("mkdir -p logs"); if (logger_open("logs/pings.csv")<0){ perror("logger_open"); }


pthread_t tg, th; pthread_create(&tg,NULL,th_gps,NULL); pthread_create(&th,NULL,th_hb,NULL);


fprintf(stderr, "truck %s up: tcp=%d, mc=%s:%d\n", g_truck_id, g_tcp_port, MC_GROUP, MC_PORT);


while(running){
struct sockaddr_in ca; socklen_t cl=sizeof(ca);
int s = accept(listen_fd, (struct sockaddr*)&ca, &cl);
if (s<0){ usleep(20*1000); continue; }
pthread_t tw; pthread_create(&tw,NULL,th_worker,(void*)(intptr_t)s); pthread_detach(tw);
}


logger_close(); close(listen_fd); close(mc_fd);
return 0;
}