#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include "util.h"


static inline double d2r(double d){ return d * (M_PI/180.0); }


double haversine_km(double lat1, double lon1, double lat2, double lon2){
const double R = 6371.0; // km
double dlat = d2r(lat2-lat1), dlon = d2r(lon2-lon1);
double a = sin(dlat/2)*sin(dlat/2) + cos(d2r(lat1))*cos(d2r(lat2))*sin(dlon/2)*sin(dlon/2);
double c = 2*atan2(sqrt(a), sqrt(1-a));
return R*c;
}


long now_sec(void){ return (long)time(NULL); }


int set_nonblocking(int fd){
int fl = fcntl(fd, F_GETFL, 0);
if (fl < 0) return -1;
return fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}


ssize_t recv_line_timeout(int fd, char *buf, size_t n, int timeout_ms){
size_t pos = 0; buf[0] = '\0';
while (pos+1 < n){
fd_set rf; FD_ZERO(&rf); FD_SET(fd, &rf);
struct timeval tv = { .tv_sec = timeout_ms/1000, .tv_usec = (timeout_ms%1000)*1000 };
int r = select(fd+1, &rf, NULL, NULL, &tv);
if (r <= 0) return r==0 ? 0 : -1;
char c; ssize_t k = recv(fd, &c, 1, 0);
if (k <= 0){ if (errno==EINTR) continue; return -1; }
buf[pos++] = c; if (c == '\n') break;
}
buf[pos] = '\0';
return (ssize_t)pos;
}


ssize_t send_all_timeout(int fd, const char *buf, size_t n, int timeout_ms){
size_t sent = 0; long start = now_sec();
while (sent < n){
fd_set wf; FD_ZERO(&wf); FD_SET(fd, &wf);
struct timeval tv = { .tv_sec = timeout_ms/1000, .tv_usec = (timeout_ms%1000)*1000 };
int r = select(fd+1, NULL, &wf, NULL, &tv);
if (r <= 0) return -1;
ssize_t k = send(fd, buf+sent, n-sent, 0);
if (k < 0){ if (errno==EINTR) continue; return -1; }
sent += (size_t)k;
if ((now_sec()-start) > (timeout_ms/1000 + 1)) break;
}
return (ssize_t)sent;
}