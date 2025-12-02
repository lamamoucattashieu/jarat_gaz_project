#pragma once

#include <stdint.h>
#include <time.h>
#include "common.h" // FIX: Include common.h to get TruckInfo and PingMsg definitions

// Maximum length for Truck ID, User ID (plus one for the null terminator)
// #define MAX_ID_LEN 16 // MAX_ID_LEN is already defined in common.h

// Maximum length for the full message buffer (Heartbeat, Ping, or ACK)
// This should be large enough to hold the longest formatted message.
#define MAX_MSG_LEN 256 


// --- Data Structures ---
// FIX: Removed duplicate struct definitions (TruckInfo and PingMsg)
// They are now pulled from common.h via the include above.


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