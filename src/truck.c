#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "protocol.h" 
#include "util.h"    
#include "net.h"   
#include "logger.h" 
#ifndef MAX_LINE
#define MAX_LINE 256
#endif
#ifndef HB_INTERVAL_MS
#define HB_INTERVAL_MS 1000 // 1 second heartbeat interval
#endif
#ifndef MC_GROUP
#define MC_GROUP "225.0.0.37" // Default Multicast Group IP
#endif
#ifndef MC_PORT
#define MC_PORT 12345         // Default Multicast Port
#endif


// --- GLOBAL STATE ---
static volatile int running = 1;

// Truck Status
static double g_lat = 31.956, g_lon = 35.945;
static volatile int g_queue_len = 0; // MUST be volatile for atomic operations
static char g_truck_id[MAX_ID_LEN] = "TRK01"; 
static int g_tcp_port = 6012;

// Network File Descriptors and Address
static int mc_fd = -1, listen_fd = -1; 
static struct sockaddr_in mc_addr;


// --- SIGNAL HANDLER ---
static void on_sig(int s) { 
    (void)s; 
    fprintf(stderr, "\nSignal received. Shutting down...\n");
    running = 0; 
}


// --- GPS SIMULATION THREAD ---
static void* th_gps(void* _) { 
    (void)_; 
    while (running) { 
        // Assumed to be in util.c
        gps_step(&g_lat, &g_lon); 
        usleep(300 * 1000); // Update every 300ms
    } 
    return NULL; 
}


// --- HEARTBEAT BROADCAST THREAD ---
static void* th_hb(void* _) { 
    (void)_; 
    char line[MAX_LINE];
    while (running) {
        // 1. Format the Heartbeat message (HB)
        format_hb(line, sizeof(line), g_truck_id, g_lat, g_lon, g_tcp_port, time(NULL));
        
        // 2. Send the message via UDP Multicast (mc_fd is set up in main)
        sendto(mc_fd, line, strlen(line), 0, (struct sockaddr*)&mc_addr, sizeof(mc_addr));
        
        // 3. Wait for the interval
        usleep(HB_INTERVAL_MS * 1000);
    } 
    return NULL; 
}


// --- PING WORKER THREAD (Handles one TCP connection) ---
static void* th_worker(void *arg) { 
    // FIX: Safely retrieve socket FD and free the allocated memory
    int *sock_ptr = (int *)arg;
    int sock = *sock_ptr;
    free(sock_ptr); 

    char buf[MAX_LINE];
    
    // Attempt to read the PING message from the client with a 2-second timeout
    ssize_t n = recv_line_timeout(sock, buf, sizeof(buf), 2000);

    if (n > 0) { 
        PingMsg p = {0}; 
        if (parse_ping(buf, &p)) {
            
            // ATOMIC FIX: Safely calculate the ETA based on the current queue length
            // 1. Increment queue length and read the *new* length (N+1) atomically
            int current_queue = __sync_add_and_fetch(&g_queue_len, 1);
            
            // 2. Calculate ETA (5 mins base + position in queue)
            // If current_queue is 1 (first person), eta = 5 + 0.
            int eta = 5 + (current_queue - 1); 

            // 3. Log the ping
            logger_log_ping(time(NULL), &p, g_lat, g_lon);
            
            // 4. Send the ACK back to the client
            char out[MAX_LINE]; 
            format_ack(out, sizeof(out), g_truck_id, eta, current_queue);
            send_all_timeout(sock, out, strlen(out), 2000);
            
            // 5. Decrement queue length atomically
            __sync_sub_and_fetch(&g_queue_len, 1);
        } else {
            fprintf(stderr, "Worker: Failed to parse PING message: %s\n", buf);
        }
    } else if (n == 0) {
        // fprintf(stderr, "Worker: Client disconnected before sending data.\n");
    } else {
        perror("Worker: recv_line_timeout error");
    }
    
    close(sock); 
    return NULL; 
}


// --- MAIN ENTRY POINT ---
int main(int argc, char **argv) {
    // 1. Argument Parsing (Unchanged, looks correct)
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--id") && i + 1 < argc) strncpy(g_truck_id, argv[++i], MAX_ID_LEN - 1);
        else if (!strcmp(argv[i], "--tcp") && i + 1 < argc) g_tcp_port = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--start-lat") && i + 1 < argc) g_lat = atof(argv[++i]);
        else if (!strcmp(argv[i], "--start-lon") && i + 1 < argc) g_lon = atof(argv[++i]);
    }
    g_truck_id[MAX_ID_LEN - 1] = '\0'; // Ensure termination safety

    // 2. Setup Signal Handlers
    signal(SIGINT, on_sig); 
    signal(SIGTERM, on_sig);

    // 3. Initialize GPS and Networking
    gps_init(g_lat, g_lon, 4.0);
    
    // Setup Multicast Sender for Heartbeats (HB)
    if (udp_mc_sender(MC_GROUP, MC_PORT, &mc_fd, &mc_addr) < 0) { 
        perror("udp_mc_sender failed"); 
        return 1; 
    }
    // Setup TCP Listener for Ping Requests (PING)
    if (tcp_listen(g_tcp_port, 64, &listen_fd) < 0) { 
        perror("tcp_listen failed"); 
        return 1; 
    }
    
    // 4. Setup Logging
    system("mkdir -p logs"); 
    if (logger_open("logs/pings.csv") < 0) { 
        perror("logger_open failed"); 
    }

    // 5. Start Background Threads
    pthread_t tg, th; 
    pthread_create(&tg, NULL, th_gps, NULL); 
    pthread_create(&th, NULL, th_hb, NULL);

    fprintf(stderr, "🚚 Truck %s running: TCP port=%d, Multicast=%s:%d\n", 
            g_truck_id, g_tcp_port, MC_GROUP, MC_PORT);

    // 6. Main TCP Listener Loop
    while (running) {
        struct sockaddr_in ca; 
        socklen_t cl = sizeof(ca);
        
        // This call blocks until a client connects
        int s = accept(listen_fd, (struct sockaddr*)&ca, &cl);
        
        if (s < 0) { 
            // If accept fails (e.g., interrupted by signal), wait briefly and continue
            if (running) { // Only print error if server is meant to be running
                // perror("accept");
                usleep(20 * 1000); 
            }
            continue; 
        }

        // FIX: Allocate memory for the socket FD to pass to the new thread
        int *sock_ptr = malloc(sizeof(int));
        if (!sock_ptr) {
            fprintf(stderr, "Failed to allocate memory for thread argument. Closing client socket.\n");
            close(s);
            continue;
        }
        *sock_ptr = s;
        
        // Create a new worker thread to handle the ping
        pthread_t tw; 
        pthread_create(&tw, NULL, th_worker, sock_ptr); 
        pthread_detach(tw); // The thread cleans up its own resources when finished
    }


    // 7. Cleanup and Exit
    fprintf(stderr, "Shutting down threads and resources...\n");
    
    // Threads will exit gracefully because 'running' is false

    logger_close(); 
    close(listen_fd); 
    close(mc_fd);
    
    // Note: It's good practice to pthread_join tg and th here, but we rely on a quick exit for simplicity.

    return 0;
}
