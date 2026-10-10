/* SPDX-License-Identifier: MIT */
#include "windows_math.h"
#include <limits.h>
#include <math.h>
#include <string.h>
inv_status inv_win_memory_used(uint64_t total,uint64_t available,uint64_t *used,double *fraction) {
    if(!total || available>total)return INV_INVALID_DATA;
    *used=total-available; *fraction=(double)*used/(double)total;
    return INV_OK;
}
inv_status inv_win_cpu_fraction(uint64_t pt,uint64_t pi,uint64_t total,uint64_t idle,double *out) {
    uint64_t dt,di;
    if(idle>total || pi>pt)return INV_INVALID_DATA;
    if(total<=pt || idle<pi)return INV_WARMUP;
    dt=total-pt; di=idle-pi;
    if(di>dt)return INV_WARMUP;
    *out=(double)(dt-di)/(double)dt;
    return INV_OK;
}
inv_status inv_win_pages_bytes(uint64_t pages,uint64_t size,uint64_t *out) {
    if(!size || pages>UINT64_MAX/size)return INV_INVALID_DATA;
    *out=pages*size; return INV_OK;
}
inv_status inv_win_kelvin_celsius(double kelvin,double *out) {
    /* A zero/negative firmware reading is usually an unsupported channel.
       No fabricated CPU package association and no averaging of zones. */
    if(!isfinite(kelvin) || kelvin<=0 || kelvin>2000)return INV_INVALID_DATA;
    *out=kelvin-273.15; return INV_OK;
}
static int number(const char **s,unsigned *v) {
    unsigned n=0;
    if(**s<'0'||**s>'9')return 0;
    do {
        unsigned digit=(unsigned)(**s-'0');
        if(n>(UINT_MAX-digit)/10)return 0;
        n=n*10+digit; (*s)++;
    } while(**s>='0'&&**s<='9');
    *v=n; return 1;
}
int inv_win_cpu_instance(const char *s,unsigned *group,unsigned *cpu) {
    unsigned first,second;
    if(!strcmp(s,"_Total"))return 0;
    if(!number(&s,&first))return -1;
    if(!*s) { *group=0; *cpu=first; return 1; }
    if(*s++!=',' || !number(&s,&second) || *s || first>65535 || second>63)return -1;
    *group=first; *cpu=second; return 1;
}
