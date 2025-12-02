#define _POSIX_C_SOURCE 200809L
rows.push_back({d,t});
}
std::sort(rows.begin(), rows.end(), [](auto &a, auto &b){return a.first<b.first;});
pthread_mutex_unlock(&mu);


printf("\ntruck_id distance_km last_seen_s tcp_port ip\n");
for (auto &r: rows){
printf("%-14s %10.3f %11d %8d %s\n", r.second.id, r.first,
(int)(now_sec()-r.second.last_seen), r.second.tcp_port, inet_ntoa(r.second.last_ip));
if (r.first < near_km) { printf("\a>> %s is nearby!\n", r.second.id); }
}
fflush(stdout); sleep(1);
}
}


static int do_ping(){
// find truck
pthread_mutex_lock(&mu); TruckInfo t={0}; bool found=false;
for (auto &x: trucks){ if (strncmp(x.id,want_truck,MAX_ID_LEN)==0){ t=x; found=true; break; }}
pthread_mutex_unlock(&mu);
if (!found){ fprintf(stderr, "truck %s not seen yet. Run list mode first.\n", want_truck); return 1; }


int s = tcp_connect_timeout_addr(t.last_ip, t.tcp_port, 2000);
if (s<0){ perror("connect"); return 1; }
PingMsg p={0}; strncpy(p.truck_id,want_truck,MAX_ID_LEN); strncpy(p.user_id,user_id,MAX_ID_LEN);
strncpy(p.addr,addr,sizeof(p.addr)); strncpy(p.note,note,sizeof(p.note));
char line[MAX_LINE]; format_ping(line,sizeof(line),&p);
send_all_timeout(s,line,strlen(line),2000);
char resp[MAX_LINE]; ssize_t n = recv_line_timeout(s,resp,sizeof(resp),2000);
if (n>0){ char id[16]; int eta,q; if (parse_ack(resp,id,&eta,&q)){
printf("ACK from %s: eta=%d min queued=%d\n", id, eta, q);
} else { printf("bad ACK: %s\n", resp); }
}
close(s); return 0;
}


int main(int argc, char **argv){
bool ping_mode=false;
for(int i=1;i<argc;i++){
if(!strcmp(argv[i],"--user-lat") && i+1<argc) u_lat=atof(argv[++i]);
else if(!strcmp(argv[i],"--user-lon") && i+1<argc) u_lon=atof(argv[++i]);
else if(!strcmp(argv[i],"--near") && i+1<argc) near_km=atof(argv[++i]);
else if(!strcmp(argv[i],"--truck") && i+1<argc){ strncpy(want_truck,argv[++i],MAX_ID_LEN); ping_mode=true; }
else if(!strcmp(argv[i],"--addr") && i+1<argc) strncpy(addr,argv[++i],sizeof(addr));
else if(!strcmp(argv[i],"--note") && i+1<argc) strncpy(note,argv[++i],sizeof(note));
else if(!strcmp(argv[i],"--user") && i+1<argc) strncpy(user_id,argv[++i],MAX_ID_LEN);
}
if (udp_mc_receiver(MC_GROUP, MC_PORT, &mc_fd)<0){ perror("udp_mc_receiver"); return 1; }
pthread_t tm; pthread_create(&tm,NULL,th_mc,NULL);


if (ping_mode){ sleep(1); return do_ping(); }
list_loop();
return 0;
}