#pragma once
#include "common.h"
#include <time.h>


int logger_open(const char *path);
void logger_close(void);
void logger_log_ping(time_t ts, const PingMsg *p, double truck_lat, double truck_lon);