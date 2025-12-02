#include <stdio.h>
#include "../src/gps.h"
int main(void){
double lat=31.956, lon=35.945; gps_init(lat,lon,5.0);
for(int i=0;i<1000;i++) gps_step(&lat,&lon);
if (lat==31.956 && lon==35.945) return 1;
printf("ok\n"); return 0;
}