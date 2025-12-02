#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700 // <--- ADD THIS LINE
#include <math.h>

#include <math.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h> // For M_PI definition, though often in math.h
#include "util.h"


// --- Utility Time Function ---

// Get current time in milliseconds
static long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// --- GPS Math ---

// Degrees to Radians (d2r)
static inline double d2r(double d){ return d * (M_PI/180.0); }

/**
 * @brief Calculates the great-circle distance between two points on a sphere 
 * using the Haversine formula.
 */
double haversine_km(double lat1, double lon1, double lat2, double lon2){
    const double R = 6371.0; // Earth radius in km
    double dlat = d2r(lat2-lat1), dlon = d2r(lon2-lon1);
    
    double a = sin(dlat/2)*sin(dlat/2) + 
               cos(d2r(lat1))*cos(d2r(lat2))*sin(dlon/2)*sin(dlon/2);
    double c = 2*atan2(sqrt(a), sqrt(1-a));
    return R*c;
}

/**
 * @brief Gets the current time in seconds since the Epoch.
 */
long now_sec(void){ 
    return (long)time(NULL); 
}

// --- Non-Blocking Utility ---

/**
 * @brief Sets the file descriptor (socket) to non-blocking mode.
 */
int set_nonblocking(int fd){
    int fl = fcntl(fd, F_GETFL, 0);
    if (fl < 0) return -1;
    return fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

// --- Robust Socket I/O ---

/**
 * @brief Reads a line (\n terminated) from a file descriptor with a total timeout.
 * @return >0 bytes read, 0 on timeout, -1 on error.
 */
ssize_t recv_line_timeout(int fd, char *buf, size_t n, int timeout_ms){
    size_t pos = 0; 
    buf[0] = '\0';
    long start_time = now_ms();
    long remaining_ms;

    while (pos + 1 < n) {
        // Calculate remaining time
        remaining_ms = timeout_ms - (now_ms() - start_time);
        if (remaining_ms <= 0) break; // Timeout reached

        fd_set rf; FD_ZERO(&rf); FD_SET(fd, &rf);
        struct timeval tv = { 
            .tv_sec = remaining_ms / 1000, 
            .tv_usec = (remaining_ms % 1000) * 1000 
        };
        
        int r = select(fd + 1, &rf, NULL, NULL, &tv);
        if (r <= 0) return r == 0 ? 0 : -1; // 0 on timeout, -1 on error

        char c; 
        ssize_t k = recv(fd, &c, 1, 0); // Read a single byte
        
        if (k <= 0) { 
            if (k < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
            return -1; // Error or EOF
        }
        
        buf[pos++] = c; 
        if (c == '\n') break;
    }
    
    buf[pos] = '\0';
    return (ssize_t)pos;
}


/**
 * @brief Sends all 'n' bytes of 'buf' with a total timeout.
 * @return >0 bytes sent, -1 on error or timeout before completion.
 */
ssize_t send_all_timeout(int fd, const char *buf, size_t n, int timeout_ms){
    size_t sent = 0; 
    long start_time = now_ms();
    long remaining_ms;

    while (sent < n) {
        // Calculate remaining time
        remaining_ms = timeout_ms - (now_ms() - start_time);
        if (remaining_ms <= 0) break; // Total timeout reached
        
        fd_set wf; FD_ZERO(&wf); FD_SET(fd, &wf);
        struct timeval tv = { 
            .tv_sec = remaining_ms / 1000, 
            .tv_usec = (remaining_ms % 1000) * 1000 
        };
        
        int r = select(fd + 1, NULL, &wf, NULL, &tv);
        if (r <= 0) return -1; // 0 on select timeout (shouldn't happen with correct remaining_ms check), or -1 on error

        ssize_t k = send(fd, buf + sent, n - sent, 0);
        
        if (k < 0) { 
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue; 
            return -1; // Fatal error
        }
        
        sent += (size_t)k;
    }
    
    return (ssize_t)sent;
}

// The following functions (gps_init, gps_step) are assumed to be defined 
// in this file or a linked file, but their definitions were not provided.
// Their prototypes MUST be in util.h.
/*
void gps_init(double lat, double lon, double max_dist_km);
void gps_step(double *lat, double *lon);
*/