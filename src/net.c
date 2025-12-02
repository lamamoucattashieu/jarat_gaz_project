#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <errno.h>
#include "net.h"
#include "util.h"


int udp_mc_sender(const char *group, uint16_t port, int *sock_out, struct sockaddr_in *addr_out){
int s = socket(AF_INET, SOCK_DGRAM, 0); if (s<0) return -1;
int ttl=1; setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
struct sockaddr_in addr={0}; addr.sin_family=AF_INET; addr.sin_port=htons(port);
inet_aton(group, &addr.sin_addr);
*sock_out=s; if (addr_out) *addr_out=addr; return 0;
}


int udp_mc_receiver(const char *group, uint16_t port, int *sock_out){
int s = socket(AF_INET, SOCK_DGRAM, 0); if (s<0) return -1;
int reuse=1; setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
struct sockaddr_in addr={0}; addr.sin_family=AF_INET; addr.sin_port=htons(port); addr.sin_addr.s_addr=htonl(INADDR_ANY);
if (bind(s, (struct sockaddr*)&addr, sizeof(addr))<0){ close(s); return -1; }
struct ip_mreq mreq; mreq.imr_multiaddr.s_addr=inet_addr(group); mreq.imr_interface.s_addr=htonl(INADDR_ANY);
if (setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq))<0){ close(s); return -1; }
*sock_out=s; return 0;
}


int tcp_listen(uint16_t port, int backlog, int *sock_out){
int s=socket(AF_INET, SOCK_STREAM, 0); if (s<0) return -1;
int reuse=1; setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
struct sockaddr_in addr={0}; addr.sin_family=AF_INET; addr.sin_port=htons(port); addr.sin_addr.s_addr=htonl(INADDR_ANY);
if (bind(s,(struct sockaddr*)&addr,sizeof(addr))<0){ close(s); return -1; }
if (listen(s, backlog)<0){ close(s); return -1; }
*sock_out=s; return 0;
}


int tcp_connect_timeout_addr(struct in_addr ip, uint16_t port, int timeout_ms){
int s=socket(AF_INET, SOCK_STREAM, 0); if (s<0) return -1;
set_nonblocking(s);
struct sockaddr_in addr={0}; addr.sin_family=AF_INET; addr.sin_port=htons(port); addr.sin_addr=ip;
int r=connect(s,(struct sockaddr*)&addr,sizeof(addr));
if (r<0 && errno!=EINPROGRESS){ close(s); return -1; }
fd_set wf; FD_ZERO(&wf); FD_SET(s,&wf);
struct timeval tv = { .tv_sec = timeout_ms/1000, .tv_usec = (timeout_ms%1000)*1000 };
r = select(s+1,NULL,&wf,NULL,&tv);
if (r<=0){ close(s); return -1; }
int err=0; socklen_t len=sizeof(err); getsockopt(s,SOL_SOCKET,SO_ERROR,&err,&len);
if (err){ close(s); return -1; }
return s;
}