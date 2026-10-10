/* SPDX-License-Identifier: MIT */
#include "internal.h"
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
static unsigned checks,errors;
#define REQUIRE(x) do { checks++; if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);errors++;} } while(0)
#if INV_WITH_NETWORK || INV_WITH_HWMON
static void put(const char *root,const char *name,const char *text) {
    char p[2048];FILE *f;REQUIRE(inv_path(p,sizeof(p),root,name));f=fopen(p,"w");REQUIRE(f!=NULL);
    if(f){REQUIRE(fputs(text,f)>=0);REQUIRE(fclose(f)==0);}
}
static void directory(const char *root,const char *name) {char p[2048];REQUIRE(inv_path(p,sizeof(p),root,name));REQUIRE(mkdir(p,0700)==0);}
static void erase(const char *root,const char *name,int dir) {char p[2048];inv_path(p,sizeof(p),root,name);REQUIRE((dir?rmdir(p):unlink(p))==0);}
typedef struct captured { inv_engine *engine; const char *root; unsigned added,removed,samples,frames,warm;
    uint64_t first_id,last_id,period;inv_status status;double number;int replace; } captured;
static inv_status upsert(void *ctx,const inv_series *s) {
    captured *c=ctx;c->added++;if(!c->first_id)c->first_id=s->id;c->last_id=s->id;c->period=s->effective_period_ns;return INV_OK;
}
static inv_status remove_series(void *ctx,uint64_t id) {captured *c=ctx;c->removed++;REQUIRE(id==c->first_id);return INV_OK;}
static inv_status sample(void *ctx,const inv_sample *s) {
    captured *c=ctx;c->samples++;c->status=s->status;
    if(s->status==INV_WARMUP)c->warm++;
    if(s->status==INV_OK)REQUIRE(inv_value_number(&s->value,&c->number));
    return INV_OK;
}
static inv_status frame(void *ctx) {
    captured *c=ctx;c->frames++;
    if(c->replace && c->frames==1) {
        put(c->root,"class/net/fake0/ifindex","2\n");
        put(c->root,"net/dev","Inter-| Receive | Transmit\n face | bytes\n fake0: 5 1 0 0 0 0 0 0 7 1 0 0 0 0 0 0\n");
        inv_engine_refresh(c->engine);
    }
    return INV_OK;
}
static inv_sink sink_for(captured *c) {inv_sink s;memset(&s,0,sizeof(s));s.api_version=1;s.struct_size=sizeof(s);s.context=c;
 s.upsert_series=upsert;s.remove_series=remove_series;s.emit_sample=sample;s.end_frame=frame;return s;}
#endif
#if INV_WITH_NETWORK
static void network_test(void) {
 char dir[]="/tmp/inv-hotplug-XXXXXX",error[512];inv_plan *p=malloc(sizeof(*p));inv_engine *e=NULL;inv_paths paths;captured c;inv_sink sink;
 REQUIRE(p!=NULL);if(!p)return;REQUIRE(mkdtemp(dir)!=NULL);
 directory(dir,"class");directory(dir,"class/net");directory(dir,"class/net/fake0");directory(dir,"net");
 put(dir,"class/net/fake0/ifindex","1\n");put(dir,"net/dev","Inter-| Receive | Transmit\n face | bytes\n fake0: 100 1 0 0 0 0 0 0 100 1 0 0 0 0 0 0\n");
 paths.proc_root=paths.sys_root=paths.etc_root=dir;
 REQUIRE(inv_query_parse("mode file; every 100ms; for 600ms; select rate(device.net.rx_bytes_total);",p,error,sizeof(error))==INV_OK);
 REQUIRE(inv_plan_validate(p,error,sizeof(error))==INV_OK);REQUIRE(inv_engine_create(p,&paths,&e,error,sizeof(error))==INV_OK);
 memset(&c,0,sizeof(c));c.engine=e;c.root=dir;c.replace=1;sink=sink_for(&c);
 if(e){REQUIRE(inv_engine_run(e,&sink)==INV_OK);REQUIRE(c.added==2 && c.removed==1);REQUIRE(c.first_id!=c.last_id);REQUIRE(c.warm==2);REQUIRE(c.samples>=3);REQUIRE(c.status==INV_OK && c.number>=0);inv_engine_destroy(e);}
 erase(dir,"class/net/fake0/ifindex",0);erase(dir,"class/net/fake0",1);erase(dir,"class/net",1);erase(dir,"class",1);erase(dir,"net/dev",0);erase(dir,"net",1);REQUIRE(rmdir(dir)==0);free(p);
}
#endif
#if INV_WITH_HWMON
static void hwmon_test(void) {
 char dir[]="/tmp/inv-hwmon-XXXXXX",error[512];inv_plan *p=malloc(sizeof(*p));inv_engine *e=NULL;inv_paths paths;captured c;inv_sink sink;int run;
 REQUIRE(p!=NULL);if(!p)return;REQUIRE(mkdtemp(dir)!=NULL);
 directory(dir,"class");directory(dir,"class/hwmon");directory(dir,"class/hwmon/hwmon0");
 put(dir,"class/hwmon/hwmon0/name","fixture_chip\n");put(dir,"class/hwmon/hwmon0/temp1_input","54321\n");
 put(dir,"class/hwmon/hwmon0/update_interval","2000\n");paths.proc_root=paths.sys_root=paths.etc_root=dir;
 REQUIRE(inv_query_parse("every 1s; strict; count 1; select sensor.temperature;",p,error,sizeof(error))==INV_OK);
 REQUIRE(inv_plan_validate(p,error,sizeof(error))==INV_OK);REQUIRE(inv_engine_create(p,&paths,&e,error,sizeof(error))==INV_BAD_QUERY && e==NULL);
 for(run=0;run<3;run++) {
  REQUIRE(inv_query_parse("once; select sensor.temperature;",p,error,sizeof(error))==INV_OK);REQUIRE(inv_plan_validate(p,error,sizeof(error))==INV_OK);
  if(run==1)put(dir,"class/hwmon/hwmon0/temp1_fault","1\n");
  if(run==2){put(dir,"class/hwmon/hwmon0/temp1_fault","0\n");put(dir,"class/hwmon/hwmon0/temp1_input","invalid\n");}
  REQUIRE(inv_engine_create(p,&paths,&e,error,sizeof(error))==INV_OK);memset(&c,0,sizeof(c));sink=sink_for(&c);
  if(e){REQUIRE(inv_engine_run(e,&sink)==INV_OK);REQUIRE(c.added==1 && c.samples==1 && c.period==2*INV_NS_S);
   REQUIRE(c.status==(run==0?INV_OK:run==1?INV_NO_DATA:INV_INVALID_DATA));if(!run)REQUIRE(c.number>54.32 && c.number<54.33);inv_engine_destroy(e);e=NULL;}
 }
 erase(dir,"class/hwmon/hwmon0/name",0);erase(dir,"class/hwmon/hwmon0/temp1_input",0);erase(dir,"class/hwmon/hwmon0/update_interval",0);erase(dir,"class/hwmon/hwmon0/temp1_fault",0);
 erase(dir,"class/hwmon/hwmon0",1);erase(dir,"class/hwmon",1);erase(dir,"class",1);REQUIRE(rmdir(dir)==0);free(p);
}
#endif
int main(void) {
#if INV_WITH_NETWORK
 network_test();
#endif
#if INV_WITH_HWMON
 hwmon_test();
#endif
 printf("device fixtures: %u checks, %u failures\n",checks,errors);return errors?1:0;
}
