// protocol.h
#pragma once

#include <stdint.h>
#include <time.h>



// Maximum length for Truck ID, User ID (plus one for the null terminator)
#define MAX_ID_LEN 16 

// Maximum length for the full message buffer (Heartbeat, Ping, or ACK)
// This should be large enough to hold the longest formatted message.
#define MAX_MSG_LEN 256 


// --- Data Structures ---

/**
 * @brief Structure for Truck Heartbeat/Location Information.
 * Used for UDP Multicast broadcasts (HB message).
 */
typedef struct {
    char id[MAX_ID_LEN]; // Truck identifier (e.g., "T101")
    double lat;          // Latitude (e.g., 32.000000)
    double lon;          // Longitude (e.g., 35.900000)
    int tcp_port;        // TCP port where the truck listens for PINGs (e.g., 8080)
} TruckInfo;


/**
 * @brief Structure for Ping Message (Service Request).
 * Used for TCP communication (PING message).
 */
typedef struct {
    char truck_id[MAX_ID_LEN]; // Target Truck ID
    char user_id[MAX_ID_LEN];  // Resident User ID (e.g., "U5555")
    char addr[128];            // Resident's street address or location (e.g., "Amman St 24")
    char note[64];             // Optional note (e.g., "Need 2 cylinders large")
} PingMsg;


// --- Function Prototypes (Serialization / Deserialization) ---

// Heartbeat (HB) - Truck to Client
int format_hb(char *out, size_t n,
              const char *truck_id, double lat, double lon, int tcp_port, time_t ts);

int parse_hb(const char *line, TruckInfo *out, time_t *ts);

// Ping (PING) - Client to Truck
int format_ping(char *out, size_t n, const PingMsg *p);

int parse_ping(const char *line, PingMsg *out);

// Acknowledgment (ACK) - Truck to Client
int format_ack(char *out, size_t n, const char *truck_id, int eta_min, int queued);