#pragma once
void gps_init(double lat0, double lon0, double max_step_m);
void gps_step(double *lat, double *lon);