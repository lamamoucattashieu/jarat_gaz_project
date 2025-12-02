#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "protocol.h"


static const char* skipsp(const char *s){ while(*s==' '||*s=='\t') ++s; return s; }
static int starts(const char *s, const char *p){ return strncmp(s,p,strlen(p))==0; }


int format_hb(char *out, size_t n,
const char *truck_id, double lat, double lon, int tcp_port, time_t ts){
return snprintf(out, n, "HB truck_id=%s lat=%.6f lon=%.6f ts=%ld tcp=%d\n",
truck_id, lat, lon, (long)ts, tcp_port);
}


int parse_hb(const char *line, TruckInfo *out, time_t *ts){
if (!starts(line, "HB ")) return 0; line += 3;
char id[MAX_ID_LEN]={0}; double lat=0,lon=0; long t=0; int tcp=0;
const char *s = line;
while (*s){
s = skipsp(s);
if (starts(s,"truck_id=")){ s+=9; sscanf(s, "%15s", id); }
else if (starts(s,"lat=")){ s+=4; sscanf(s, "%lf", &lat); }
else if (starts(s,"lon=")){ s+=4; sscanf(s, "%lf", &lon); }
else if (starts(s,"ts=")){ s+=3; sscanf(s, "%ld", &t); }
else if (starts(s,"tcp=")){ s+=4; sscanf(s, "%d", &tcp); }
while (*s && *s!=' ' && *s!='\n') ++s;
if (*s=='\n') break;
}
if (!*id || tcp<=0) return 0;
strncpy(out->id,id,MAX_ID_LEN);
out->lat=lat; out->lon=lon; out->tcp_port=tcp; if(ts) *ts=(time_t)t;
return 1;
}


int format_ping(char *out, size_t n, const PingMsg *p){
return snprintf(out,n,
"PING truck_id=%s user_id=%s addr=\"%s\" note=\"%s\"\n",
p->truck_id, p->user_id, p->addr, p->note);
}


static void scan_qstr(const char *s, char *dst, size_t n){
if (*s!='\"'){ *dst='\0'; return; }
s++; size_t i=0; while(*s && *s!='\"' && i+1<n){ dst[i++]=*s++; }
dst[i]='\0';
}


int parse_ping(const char *line, PingMsg *out){
if (!starts(line, "PING ")) return 0; line+=5;
const char *s=line; char id[MAX_ID_LEN]={0}, uid[MAX_ID_LEN]={0};
char addr[128]={0}, note[64]={0};
while(*s){
s=skipsp(s);
if (starts(s,"truck_id=")){ s+=9; sscanf(s, "%15s", id); }
else if (starts(s,"user_id=")){ s+=8; sscanf(s, "%15s", uid); }
else if (starts(s,"addr=")){ s+=5; scan_qstr(s, addr, sizeof(addr)); }
else if (starts(s,"note=")){ s+=5; scan_qstr(s, note, sizeof(note)); }
while(*s && *s!=' ' && *s!='\n') ++s;
if (*s=='\n') break;
}
if(!*id || !*uid) return 0;
strncpy(out->truck_id,id,MAX_ID_LEN);
strncpy(out->user_id,uid,MAX_ID_LEN);
strncpy(out->addr,addr,sizeof(out->addr));
strncpy(out->note,note,sizeof(out->note));
return 1;
}


int format_ack(char *out, size_t n, const char *truck_id, int eta_min, int queued){
return snprintf(out,n, "ACK truck_id=%s eta_min=%d queued=%d\n", truck_id, eta_min, queued);
}


}