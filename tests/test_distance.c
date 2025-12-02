#include <stdio.h>
#include "../src/util.h"
int main(void){
double d0 = haversine_km(0,0,0,0);
double d1 = haversine_km(31.956,35.945,31.956,35.945);
if (d0!=0 || d1>0.001) return 1;
double d2 = haversine_km(31.956,35.945,31.957,35.945);
if (d2<=0) return 2;
printf("ok\n"); return 0;
}