#pragma once
#include <stdint.h>
#include <netinet/in.h>


int udp_mc_sender(const char *group, uint16_t port, int *sock_out, struct sockaddr_in *addr_out);
int udp_mc_receiver(const char *group, uint16_t port, int *sock_out);
int tcp_listen(uint16_t port, int backlog, int *sock_out);
int tcp_connect_timeout_addr(struct in_addr ip, uint16_t port, int timeout_ms);