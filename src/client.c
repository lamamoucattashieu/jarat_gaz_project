#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#include "common.h"
#include "net.h"
#include "protocol.h"
#include "util.h"

static double u_lat = 31.956;
static double u_lon = 35.945;
static double near_km = 0.5;

static char want_truck[MAX_ID_LEN] = "";
static char user_id[MAX_ID_LEN] = "USR1";
static char addr[128] = "";
static char note[64] = "";

static int mc_fd = -1;
static pthread_mutex_t trucks_mu = PTHREAD_MUTEX_INITIALIZER;

static TruckInfo *trucks = NULL;
static size_t trucks_count = 0;
static size_t trucks_cap = 0;

static long now_s(void) { return now_sec(); }

static void upsert_truck(const TruckInfo *ti) {
    pthread_mutex_lock(&trucks_mu);

    // look for existing
    for (size_t i = 0; i < trucks_count; ++i) {
        if (strncmp(trucks[i].id, ti->id, MAX_ID_LEN) == 0) {
            trucks[i] = *ti;
            pthread_mutex_unlock(&trucks_mu);
            return;
        }
    }

    // grow array if needed
    if (trucks_count == trucks_cap) {
        size_t new_cap = (trucks_cap == 0) ? 4 : trucks_cap * 2;
        TruckInfo *tmp = realloc(trucks, new_cap * sizeof(TruckInfo));
        if (!tmp) {
            pthread_mutex_unlock(&trucks_mu);
            return;
        }
        trucks = tmp;
        trucks_cap = new_cap;
    }

    trucks[trucks_count++] = *ti;
    pthread_mutex_unlock(&trucks_mu);
}

static void prune_stale(void) {
    long now = now_s();
    size_t w = 0;
    for (size_t r = 0; r < trucks_count; ++r) {
        if ((now - trucks[r].last_seen) <= DROP_AGE_SEC) {
            if (w != r) {
                trucks[w] = trucks[r];
            }
            ++w;
        }
    }
    trucks_count = w;
}

struct Row {
    double dist;
    TruckInfo *t;
};

static int cmp_row(const void *a, const void *b) {
    const struct Row *ra = (const struct Row *)a;
    const struct Row *rb = (const struct Row *)b;
    if (ra->dist < rb->dist) return -1;
    if (ra->dist > rb->dist) return 1;
    return 0;
}

static void *th_mc(void *arg) {
    (void)arg;
    char buf[MAX_LINE];

    while (1) {
        struct sockaddr_in src;
        socklen_t sl = sizeof(src);
        ssize_t n = recvfrom(mc_fd, buf, sizeof(buf) - 1, 0,
                             (struct sockaddr *)&src, &sl);
        if (n <= 0) {
            usleep(20 * 1000);
            continue;
        }
        buf[n] = '\0';

        TruckInfo ti;
        memset(&ti, 0, sizeof(ti));
        time_t ts = 0;
        if (parse_hb(buf, &ti, &ts)) {
            ti.last_seen = now_s();
            ti.last_ip = src.sin_addr;
            upsert_truck(&ti);
        }
    }
    return NULL;
}

static void list_loop(void) {
    while (1) {
        pthread_mutex_lock(&trucks_mu);
        prune_stale();

        size_t n = trucks_count;
        struct Row *rows = NULL;
        if (n > 0) {
            rows = (struct Row *)malloc(n * sizeof(struct Row));
        }

        for (size_t i = 0; i < n; ++i) {
            rows[i].t = &trucks[i];
            rows[i].dist = haversine_km(u_lat, u_lon,
                                        trucks[i].lat, trucks[i].lon);
        }

        pthread_mutex_unlock(&trucks_mu);

        if (n > 0) {
            qsort(rows, n, sizeof(struct Row), cmp_row);
        }

        printf("\ntruck_id       distance_km last_seen_s tcp_port ip\n");
        long now = now_s();
        for (size_t i = 0; i < n; ++i) {
            TruckInfo *t = rows[i].t;
            char ipbuf[INET_ADDRSTRLEN];
            snprintf(ipbuf, sizeof(ipbuf), "%s", inet_ntoa(t->last_ip));
            printf("%-14s %10.3f %11ld %8d %s\n",
                   t->id,
                   rows[i].dist,
                   now - t->last_seen,
                   t->tcp_port,
                   ipbuf);
            if (rows[i].dist < near_km) {
                printf("\a>> %s is nearby!\n", t->id);
            }
        }
        fflush(stdout);
        free(rows);
        sleep(1);
    }
}

static int do_ping(void) {
    // find truck by ID
    TruckInfo chosen;
    int found = 0;

    pthread_mutex_lock(&trucks_mu);
    for (size_t i = 0; i < trucks_count; ++i) {
        if (strncmp(trucks[i].id, want_truck, MAX_ID_LEN) == 0) {
            chosen = trucks[i];
            found = 1;
            break;
        }
    }
    pthread_mutex_unlock(&trucks_mu);

    if (!found) {
        fprintf(stderr,
                "truck %s not seen yet. Run client in list mode first.\n",
                want_truck);
        return 1;
    }

    int s = tcp_connect_timeout_addr(chosen.last_ip,
                                     chosen.tcp_port,
                                     2000);
    if (s < 0) {
        perror("connect");
        return 1;
    }

    PingMsg p;
    memset(&p, 0, sizeof(p));
    strncpy(p.truck_id, want_truck, MAX_ID_LEN);
    strncpy(p.user_id, user_id, MAX_ID_LEN);
    strncpy(p.addr, addr, sizeof(p.addr) - 1);
    strncpy(p.note, note, sizeof(p.note) - 1);

    char line[MAX_LINE];
    format_ping(line, sizeof(line), &p);
    send_all_timeout(s, line, strlen(line), 2000);

    char resp[MAX_LINE];
    ssize_t n = recv_line_timeout(s, resp, sizeof(resp), 2000);
    if (n > 0) {
        char id[16];
        int eta, q;
        if (parse_ack(resp, id, &eta, &q)) {
            printf("ACK from %s: eta=%d min queued=%d\n", id, eta, q);
        } else {
            printf("bad ACK: %s\n", resp);
        }
    }

    close(s);
    return 0;
}

int main(int argc, char **argv) {
    int ping_mode = 0;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--user-lat") && i + 1 < argc) {
            u_lat = atof(argv[++i]);
        } else if (!strcmp(argv[i], "--user-lon") && i + 1 < argc) {
            u_lon = atof(argv[++i]);
        } else if (!strcmp(argv[i], "--near") && i + 1 < argc) {
            near_km = atof(argv[++i]);
        } else if (!strcmp(argv[i], "--truck") && i + 1 < argc) {
            strncpy(want_truck, argv[++i], MAX_ID_LEN - 1);
            ping_mode = 1;
        } else if (!strcmp(argv[i], "--addr") && i + 1 < argc) {
            strncpy(addr, argv[++i], sizeof(addr) - 1);
        } else if (!strcmp(argv[i], "--note") && i + 1 < argc) {
            strncpy(note, argv[++i], sizeof(note) - 1);
        } else if (!strcmp(argv[i], "--user") && i + 1 < argc) {
            strncpy(user_id, argv[++i], MAX_ID_LEN - 1);
        }
    }

    if (udp_mc_receiver(MC_GROUP, MC_PORT, &mc_fd) < 0) {
        perror("udp_mc_receiver");
        return 1;
    }

    pthread_t tm;
    pthread_create(&tm, NULL, th_mc, NULL);

    if (ping_mode) {
        // give some time to receive at least one heartbeat
        sleep(1);
        return do_ping();
    }

    list_loop();
    return 0;
}
