/* SPDX-License-Identifier: MIT */
#include "internal.h"
#include "inv/view.h"
#include "raster.h"
#include <math.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#endif
static unsigned checks,failures;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);failures++;}}while(0)
static void test_query(void) {
    inv_plan *p=(inv_plan *)malloc(sizeof(*p));
    char e[512];
    uint64_t ns;
    inv_status s;
    CHECK(p!=NULL);
    if(!p)return;
    CHECK(inv_parse_duration("250ms",&ns)==INV_OK&&ns==250*INV_NS_MS);
    CHECK(inv_parse_duration("0.5s",&ns)==INV_OK&&ns==500*INV_NS_MS);
    CHECK(inv_parse_duration("5m",&ns)==INV_OK&&ns==300*INV_NS_S);
    CHECK(inv_parse_duration("NaNs",&ns)!=INV_OK);
    CHECK(inv_parse_duration("-1s",&ns)!=INV_OK);
    CHECK(inv_parse_duration("1",&ns)!=INV_OK);
    CHECK(inv_parse_duration("999999h",&ns)!=INV_OK);
    s=inv_query_parse("# comment\nversion 1; mode file; format csv; every 250ms; count 3; select os.uptime as plot(min=0,title=\"Boot time\") every 1s;",p,e,sizeof(e));
    CHECK(s==INV_OK);
    CHECK(p->select_count==1&&p->count==3&&!p->once&&p->selects[0].period_ns==INV_NS_S);
    CHECK(inv_plan_validate(p,e,sizeof(e))==INV_OK);
    {
        const char *bad[]= {
            "mode foo; select os.name;","mode file; mode tui; select os.name;","select os.name", "select os.name as robot;", "select os.name as value(title=\"unterminated);","select os.name as value(precision=0.1);","select os.name{x=\"a\",x=\"b\"};", "select rate(rate(os.uptime));","select os.name as value(min=nan);","select os.name as value(title=\"\\q\");","version 2; select os.name;"
        };
        size_t i;
        for(i=0; i<INV_ARRAY_SIZE(bad); i++)CHECK(inv_query_parse(bad[i],p,e,sizeof(e))!=INV_OK);
    }
    CHECK(inv_query_parse("select os.name as plot;",p,e,sizeof(e))==INV_OK);
    CHECK(inv_plan_validate(p,e,sizeof(e))==INV_BAD_QUERY);
    CHECK(inv_query_parse("select imaginary.metric;",p,e,sizeof(e))==INV_OK);
    CHECK(inv_plan_validate(p,e,sizeof(e))==INV_BAD_QUERY);
    CHECK(inv_query_parse("select optional imaginary.metric;",p,e,sizeof(e))==INV_OK);
    CHECK(inv_plan_validate(p,e,sizeof(e))==INV_OK);
    CHECK(inv_query_parse("once; for 2s; select os.name;",p,e,sizeof(e))==INV_OK);
    CHECK(inv_plan_validate(p,e,sizeof(e))==INV_BAD_QUERY);
    CHECK(inv_query_parse("select os.name{ifname=\"a\"};",p,e,sizeof(e))==INV_OK);
    CHECK(inv_plan_validate(p,e,sizeof(e))==INV_BAD_QUERY);
    CHECK(inv_query_parse("select os.uptime as value(unit=\"GiB\");",p,e,sizeof(e))==INV_OK);
    CHECK(inv_plan_validate(p,e,sizeof(e))==INV_BAD_QUERY);
    CHECK(inv_glob_match("cpu.*","cpu.core.utilization"));
    CHECK(inv_glob_match("e?p*","enp3s0"));
    CHECK(!inv_glob_match("e?p*","eth0"));
    CHECK(inv_glob_match("a**b?c","axyzbxc"));
    CHECK(!inv_glob_match("a*b?c","abc"));
    /* Deterministic parser smoke-fuzzing, including high bytes and unterminated strings. */ {
        unsigned seed=0x1a2b3c4d;
        size_t i,j;
        char input[256];
        const char alphabet[]="abcdefghijklmnopqrstuvwxyz0123456789.;{}()=,\"\\ \n#*/_-";
        for(i=0; i<3000; i++) {
            size_t len=(seed>>16)%255;
            for(j=0; j<len; j++) {
                seed=1664525u*seed+1013904223u;
                input[j]=alphabet[seed%(sizeof(alphabet)-1)];
            }
            input[len]=0;
            s=inv_query_parse(input,p,e,sizeof(e));
            CHECK(s==INV_OK||s==INV_BAD_QUERY||s==INV_LIMIT);
            seed=1664525u*seed+1013904223u;
        }
    }
    free(p);
}
static void test_values(void) {
    inv_value v;
    inv_view_spec view;
    char b[256];
    double x;
    memset(&v,0,sizeof(v));
    memset(&view,0,sizeof(view));
    view.precision=2;
    v.type=INV_U64;
    v.as.u64=UINT64_MAX;
    inv_format_value(b,sizeof(b),&v,"B",NULL,1);
    CHECK(!strcmp(b,"18446744073709551615"));
    v.type=INV_F64;
    v.as.f64=0.25;
    inv_format_value(b,sizeof(b),&v,"1",&view,0);
    CHECK(!strcmp(b,"25.00 %"));
    CHECK(inv_unit_scale("B/s","MiB/s",&x)&&fabs(x-1.0/1048576)<1e-12);
    CHECK(!inv_unit_scale("degC","B",&x));
    CHECK(!inv_parse_u64("-1",&v.as.u64));
    CHECK(!inv_parse_u64("18446744073709551616",&v.as.u64));
    CHECK(inv_parse_u64("42\n",&v.as.u64)&&v.as.u64==42);
    inv_safe_text(b,sizeof(b),"A\033[31m\nB");
    CHECK(strchr(b,'\033')==NULL&&strchr(b,'\n')==NULL);
}
static void test_counters(void) {
    inv_sample a,b;
    inv_value v;
    uint64_t window;
    memset(&a,0,sizeof(a));
    memset(&b,0,sizeof(b));
    a.status=b.status=INV_OK;
    a.value.type=b.value.type=INV_U64;
    a.counter_epoch=b.counter_epoch=7;
    a.value.as.u64=UINT64_MAX-100;
    b.value.as.u64=UINT64_MAX-99;
    a.measured_at_ns=INV_NS_S;
    b.measured_at_ns=a.measured_at_ns+100*INV_NS_MS;
    CHECK(inv_counter_derive(INV_RATE,&a,&b,&v,&window)==INV_OK);
    CHECK(v.type==INV_F64&&fabs(v.as.f64-10)<1e-9&&window==100*INV_NS_MS);
    CHECK(inv_counter_derive(INV_DELTA,&a,&b,&v,&window)==INV_OK&&v.type==INV_U64&&v.as.u64==1);
    b.value.as.u64=0;
    CHECK(inv_counter_derive(INV_RATE,&a,&b,&v,&window)==INV_WARMUP);
    b.value.as.u64=a.value.as.u64+1;
    b.counter_epoch=8;
    CHECK(inv_counter_derive(INV_RATE,&a,&b,&v,&window)==INV_WARMUP);
    b.counter_epoch=7;
    b.measured_at_ns=a.measured_at_ns;
    CHECK(inv_counter_derive(INV_RATE,&a,&b,&v,&window)==INV_WARMUP);
    b.status=INV_PERMISSION_DENIED;
    CHECK(inv_counter_derive(INV_RATE,&a,&b,&v,&window)==INV_PERMISSION_DENIED);
}
static void test_normalizers(void) {
#if INV_WITH_MEMORY && !defined(_WIN32)
    inv_mem_data m;
    CHECK(inv_parse_meminfo("MemTotal: 1000 kB\nMemAvailable: 250 kB\nMemFree: 10 kB\nSwapTotal: 20 kB\nSwapFree: 5 kB\n",&m)==INV_OK);
    CHECK(m.total==1024000&&m.available==256000&&m.found==31);
    CHECK(inv_parse_meminfo("MemTotal: 100 kB\nMemFree: 10 kB\n",&m)==INV_OK&&!(m.found&2));
    CHECK(inv_parse_meminfo("MemTotal: 100 kB\nMemTotal: 100 kB\n",&m)==INV_INVALID_DATA);
    CHECK(inv_parse_meminfo("MemTotal: 100 kB\nMemAvailable: 101 kB\n",&m)==INV_INVALID_DATA);
    CHECK(inv_parse_meminfo("MemTotal: 18446744073709551615 kB\n",&m)==INV_INVALID_DATA);
    CHECK(inv_parse_meminfo("MemTotal: 100 MB\n",&m)==INV_INVALID_DATA);
    CHECK(inv_parse_meminfo("Unknown: 0\n",&m)==INV_NO_DATA);
#endif
#if INV_WITH_CPU && !defined(_WIN32)
    {
        uint64_t p[8]= {
            100,0,50,800,50,0,0,0
        },
        n[8]= {
            120,0,60,850,70,0,0,0
        };
        double v;
        CHECK(inv_cpu_fraction(p,n,&v)==INV_OK&&fabs(v-.3)<1e-9);
        n[4]=49;
        CHECK(inv_cpu_fraction(p,n,&v)==INV_WARMUP);
        CHECK(inv_cpu_fraction(p,p,&v)==INV_WARMUP);
    }
#endif
}
static void test_model(void) {
    inv_plan *plan=(inv_plan *)malloc(sizeof(*plan));
    inv_model m,copy;
    inv_sink sink;
    inv_series series;
    inv_sample sample;
    inv_item *item;
    CHECK(plan!=NULL);
    if(!plan)return;
    inv_plan_init(plan);
    plan->history_ns=2*INV_NS_S;
    inv_model_init(&m,plan);
    memset(&copy,0,sizeof(copy));
    sink=inv_model_sink(&m);
    memset(&series,0,sizeof(series));
    series.id=1;
    series.source_id=2;
    series.metric=inv_metric_find("os.uptime");
    series.type=INV_F64;
    series.effective_period_ns=INV_NS_S;
    series.view.widget=INV_WIDGET_PLOT;
    inv_copy(series.key,sizeof(series.key),"os.uptime");
    inv_copy(series.unit,sizeof(series.unit),"s");
    CHECK(sink.upsert_series(sink.context,&series)==INV_OK);
    memset(&sample,0,sizeof(sample));
    sample.series_id=1;
    sample.sequence=1;
    sample.status=INV_OK;
    sample.value.type=INV_F64;
    sample.value.as.f64=5;
    sample.measured_at_ns=INV_NS_S;
    CHECK(sink.emit_sample(sink.context,&sample)==INV_OK);
    CHECK(sink.emit_sample(sink.context,&sample)==INV_OK);
    item=inv_model_find(&m,1);
    CHECK(item&&item->history_count==1&&m.samples==1);
    CHECK(inv_model_clone(&copy,&m)==INV_OK);
    CHECK(copy.items[0].history!=m.items[0].history);
    sample.sequence=2;
    sample.status=INV_IO_ERROR;
    sample.measured_at_ns+=INV_NS_S;
    CHECK(sink.emit_sample(sink.context,&sample)==INV_OK);
    CHECK(item->good.value.as.f64==5&&item->last.status==INV_IO_ERROR&&item->history_count==2);
    CHECK(copy.items[0].last.status==INV_OK);
    sample.sequence=3;
    sample.status=INV_OK;
    sample.value.as.f64=9;
    sample.measured_at_ns+=3*INV_NS_S;
    CHECK(sink.emit_sample(sink.context,&sample)==INV_OK);
    CHECK(item->history_count==1&&inv_history_at(item,0)->value==9);
    CHECK(sink.remove_series(sink.context,1)==INV_OK&&m.history_bytes==0&&!inv_model_find(&m,1));
    inv_model_destroy(&copy);
    inv_model_destroy(&m);
    free(plan);
}
static void test_raster(void) {
    uint32_t pixels[64];
    inv_surface s= {
        pixels,8,8,8
    };
    inv_clip clip= {
        0,0,8,8
    };
    inv_vertex a[3]= {
        {
            0,0,0,0,0x800000ff
        },
        {
            8,0,0,0,0x800000ff
        },
        {
            8,8,0,0,0x800000ff
        }
    };
    inv_vertex b[3]= {
        {
            0,0,0,0,0x800000ff
        },
        {
            8,8,0,0,0x800000ff
        },
        {
            0,8,0,0,0x800000ff
        }
    };
    size_t i;
    inv_raster_clear(&s,0);
    inv_raster_triangle(&s,&clip,a,NULL);
    inv_raster_triangle(&s,&clip,b,NULL);
    for(i=0; i<64; i++)CHECK(pixels[i]==0x800000);
    /* Shared diagonal blended exactly once. */ inv_raster_clear(&s,0);
    clip.right=4;
    inv_raster_triangle(&s,&clip,a,NULL);
    inv_raster_triangle(&s,&clip,b,NULL);
    for(i=0; i<64; i++)CHECK(pixels[i]==(i%8<4?0x800000u:0));
    a[0].x=INFINITY;
    inv_raster_triangle(&s,&clip,a,NULL);
    CHECK(pixels[0]==0x800000);
    {
        uint8_t alpha=0;
        inv_texture t= {
            &alpha,1,1
        };
        inv_raster_clear(&s,0);
        inv_raster_triangle(&s,&clip,b,&t);
        for(i=0; i<64; i++)CHECK(pixels[i]==0);
    }
}
#if INV_WITH_MEMORY && !defined(_WIN32)
static void write_file(const char *p,const char *s) {
    FILE *f=fopen(p,"w");
    CHECK(f!=NULL);
    if(f) {
        CHECK(fputs(s,f)>=0);
        CHECK(!fclose(f));
    }
}
typedef struct capture {
    unsigned series,samples,ended;
    uint64_t source[4];
    inv_status result;
    inv_value values[4];
} capture;
static inv_status capture_series(void *ctx,const inv_series *s) {
    capture *c=(capture *)ctx;
    if(c->series<4)c->source[c->series]=s->source_id;
    c->series++;
    return INV_OK;
}
static inv_status capture_sample(void *ctx,const inv_sample *s) {
    capture *c=(capture *)ctx;
    if(c->samples<4)c->values[c->samples]=s->value;
    c->samples++;
    CHECK(s->status==INV_OK);
    return INV_OK;
}
static inv_status capture_end(void *ctx,inv_status s) {
    capture *c=(capture *)ctx;
    c->result=s;
    c->ended++;
    return INV_OK;
}
static void test_engine_fixture(void) {
    char dir[]="/tmp/inv-unit-XXXXXX",path[1024],error[512];
    inv_paths paths;
    inv_plan *p=(inv_plan *)malloc(sizeof(*p));
    inv_engine *e=NULL;
    inv_sink sink;
    capture cap;
    CHECK(p!=NULL);
    if(!p)return;
    CHECK(mkdtemp(dir)!=NULL);
    snprintf(path,sizeof(path),"%s/meminfo",dir);
    write_file(path,"MemTotal: 1000 kB\nMemAvailable: 250 kB\n");
    paths.proc_root=dir;
    paths.sys_root=dir;
    paths.etc_root=dir;
    CHECK(inv_query_parse("once; select os.memory.used_bytes; select os.memory.used_bytes as plot; select os.memory.utilization;",p,error,sizeof(error))==INV_OK);
    CHECK(inv_plan_validate(p,error,sizeof(error))==INV_OK);
    CHECK(inv_engine_create(p,&paths,&e,error,sizeof(error))==INV_OK);
    memset(&cap,0,sizeof(cap));
    memset(&sink,0,sizeof(sink));
    sink.struct_size=sizeof(sink);
    sink.api_version=1;
    sink.context=&cap;
    sink.upsert_series=capture_series;
    sink.emit_sample=capture_sample;
    sink.end_session=capture_end;
    if(e) {
        CHECK(inv_engine_run(e,&sink)==INV_OK);
        CHECK(cap.series==3&&cap.samples==3&&cap.ended==1);
        CHECK(cap.source[0]==cap.source[1]&&cap.source[0]!=cap.source[2]);
        CHECK(cap.values[0].type==INV_U64&&cap.values[0].as.u64==750*1024);
        CHECK(cap.values[2].type==INV_F64&&fabs(cap.values[2].as.f64-.75)<1e-9);
        CHECK(inv_engine_provider_refreshes(e,"linux.meminfo")==1);
        inv_engine_destroy(e);
    }
    unlink(path);
    rmdir(dir);
    free(p);
}
#endif
int main(void) {
    char err[512];
    CHECK(inv_registry_validate(err,sizeof(err))==INV_OK);
    test_query();
    test_values();
    test_counters();
    test_normalizers();
    test_model();
    test_raster();
#if INV_WITH_MEMORY && !defined(_WIN32)
    test_engine_fixture();
#endif
    printf("%u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
