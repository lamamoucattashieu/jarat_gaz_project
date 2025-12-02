#pragma once
#include <stddef.h>
#include <time.h>
#include <sys/types.h>


double haversine_km(double lat1, double lon1, double lat2, double lon2);
long now_sec(void);
int set_nonblocking(int fd);
ssize_t recv_line_timeout(int fd, char *buf, size_t n, int timeout_ms);
ssize_t send_all_timeout(int fd, const char *buf, size_t n, int timeout_ms);