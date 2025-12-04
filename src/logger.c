#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdint.h> 

// Network headers for structs and functions used externally
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>


#include "common.h"
#include "protocol.h"
#include "logger.h"

// Internal State Definition (Fixes original struct in_addr error)
struct TruckLogState {
    char id[MAX_ID_LEN];
    double lat;
    double lon;
    time_t last_hb_ts;
    uint32_t last_ip; 
};

// Global state and mutex
static struct TruckLogState log_state = {0};
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static FILE *log_file = NULL;

// Helper to get formatted time
static void get_timestamp_string(char *buf, size_t n, time_t ts) {
    struct tm *tm = localtime(&ts);
    strftime(buf, n, "%Y-%m-%d %H:%M:%S", tm);
}

int logger_open(const char *path) {
    pthread_mutex_lock(&log_mutex);
    if (log_file) fclose(log_file);
    log_file = fopen(path, "a");
    int success = (log_file != NULL);
    if (!success) {
        perror("Failed to open log file");
    }
    pthread_mutex_unlock(&log_mutex);
    return success;
}


void logger_close(void) {
    pthread_mutex_lock(&log_mutex);
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
    pthread_mutex_unlock(&log_mutex);
}

// Implementation for logger_log_hb (assumed signature)
void logger_log_hb(const char *truck_id, double lat, double lon, const struct in_addr ip_addr, time_t ts) {
    pthread_mutex_lock(&log_mutex);
    
    char time_str[30];
    get_timestamp_string(time_str, sizeof(time_str), ts);
    
    struct in_addr temp_ip = ip_addr;
    char *ip_str = inet_ntoa(temp_ip);

    if (log_file) {
        fprintf(log_file, "[%s] HB | ID: %s | Loc: %.6f, %.6f | IP: %s\n", 
                time_str, truck_id, lat, lon, ip_str);
        fflush(log_file);
    }
    
    // Update internal state
    if (strcmp(log_state.id, truck_id) == 0 || log_state.id[0] == 0) {
        strncpy(log_state.id, truck_id, MAX_ID_LEN);
        log_state.lat = lat;
        log_state.lon = lon;
        log_state.last_hb_ts = ts;
        log_state.last_ip = ip_addr.s_addr; // Store the raw integer
    }

    pthread_mutex_unlock(&log_mutex);
}

// MATCHES logger.h: void logger_log_ping(time_t ts, const PingMsg *p, double truck_lat, double truck_lon);
void logger_log_ping(time_t ts, const PingMsg *p, double truck_lat, double truck_lon) {
    pthread_mutex_lock(&log_mutex);
    
    char time_str[30];
    get_timestamp_string(time_str, sizeof(time_str), ts);

    if (log_file) {
        fprintf(log_file, "[%s] PING | Truck: %s (%.6f, %.6f) | User: %s | Note: \"%s\"\n", 
                time_str, p->truck_id, truck_lat, truck_lon, p->user_id, p->note);
        fflush(log_file);
    }

    pthread_mutex_unlock(&log_mutex);
}

// Implementation for logger_log_ack (assumed signature)
void logger_log_ack(const char *truck_id, int eta_min, int queued) {
    pthread_mutex_lock(&log_mutex);
    
    time_t now = time(NULL);
    char time_str[30];
    get_timestamp_string(time_str, sizeof(time_str), now);

    if (log_file) {
        fprintf(log_file, "[%s] ACK | Truck: %s | ETA: %d min | Queued: %d\n", 
                time_str, truck_id, eta_min, queued);
        fflush(log_file);
    }

    pthread_mutex_unlock(&log_mutex);
}

// Function to get the latest known state (e.g., for truck server to respond)
void logger_get_latest_state(TruckInfo *info, struct in_addr *ip_addr) {
    pthread_mutex_lock(&log_mutex);
    strncpy(info->id, log_state.id, MAX_ID_LEN);
    info->lat = log_state.lat;
    info->lon = log_state.lon;
    info->tcp_port = 0; 

    // Reconstruct the struct in_addr from the stored uint32_t
    ip_addr->s_addr = log_state.last_ip; 
    
    pthread_mutex_unlock(&log_mutex);
}
