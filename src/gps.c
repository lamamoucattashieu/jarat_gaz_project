#include <stdlib.h>
#include <math.h>
#include <time.h>
static double g_lat0, g_lon0, g_step_m=3.0; // default ~3m/step


static double urand(){ return (double)rand() / (double)RAND_MAX; }


void gps_init(double lat0, double lon0, double max_step_m){
g_lat0=lat0; g_lon0=lon0; g_step_m = max_step_m>0?max_step_m:3.0; srand((unsigned)time(NULL));
}


void gps_step(double *lat, double *lon){
double meters_lat = (urand()-0.5)*2.0*g_step_m;
double meters_lon = (urand()-0.5)*2.0*g_step_m;
double dlat = meters_lat / 111320.0; // deg per meter
double dlon = meters_lon / (111320.0 * cos((*lat)*M_PI/180.0));
*lat += dlat; *lon += dlon;
}