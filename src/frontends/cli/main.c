/* SPDX-License-Identifier: MIT */
#include "internal.h"
#include "inv/view.h"
#include <locale.h>
static void usage(FILE *f) {
    fputs( "inv " INV_VERSION " -- portable inventory and monitoring\n\n" "  inv list [GLOB] [--compiled | --probe]\n" "  inv describe KEY\n" "  inv get SELECTOR... [options]\n" "  inv run --query 'INVQ...' [options]\n" "  inv run --query-file dashboard.invq [options]\n" "  inv explain --query-file dashboard.invq\n\n" "Options:\n" "  -q, --query TEXT      Inline INVQ query\n" "  --query-file PATH     Read INVQ, '-' reads standard input\n" "  --mode file|tui|gui   Override the query frontend\n" "  --format human|table|csv|md\n" "  --every DURATION     Global requested period (e.g. 250ms, 1s)\n" "  --duration DURATION  Stop after this elapsed time\n" "  --count N            Stop after N output frames (including warmup)\n" "  --once               Single snapshot\n" "  --warmup DURATION    Prime rate/CPU baselines before a single snapshot\n" "  --output PATH        Create a NEW output file, '-' is stdout\n" "  --redact             Mask sensitive values in the active frontend\n" "  --strict             Reject periods below metric minimums\n" "  --fail-on-error      Exit 3 on measurement errors (not WARMUP)\n" "  --csv-unsafe-strings Disable spreadsheet formula neutralization\n" "  --font PATH          GUI: load a local TTF font (e.g. for Cyrillic)\n" "  --version, --help\n\n" "TUI keys: q quit, p pause display, r refresh properties/discovery,\n" "          / filter, Esc clear filter, j/k or arrows scroll, s sort.\n" "All collectors are read-only. No elevation, external commands or telemetry.\n",f);
}
static int append(char *s,size_t cap,const char *v) {
    size_t n=strlen(s),m=strlen(v);
    if(m>=cap-n)return 0;
    memcpy(s+n,v,m+1);
    return 1;
}
static int load_query(const char *path,char *s,size_t cap) {
    FILE *f;
    size_t n;
    int extra;
    f=!strcmp(path,"-")?stdin:inv_fopen_utf8(path,"rb");
    if(!f) {
        fprintf(stderr,"inv: query file: %s\n",strerror(errno));
        return 0;
    }
    n=fread(s,1,cap-1,f);
    extra=fgetc(f);
    if(ferror(f)||extra!=EOF) {
        if(f!=stdin)fclose(f);
        fprintf(stderr,"inv: query read failed or query too large\n");
        return 0;
    }
    if(memchr(s,0,n)) {
        if(f!=stdin)fclose(f);
        fprintf(stderr,"inv: NUL is not permitted in a query\n");
        return 0;
    }
    s[n]=0;
    /* PowerShell UTF-8 files may contain a BOM. UTF-16 remains rejected. */
    if(n>=3 && (unsigned char)s[0]==0xef && (unsigned char)s[1]==0xbb && (unsigned char)s[2]==0xbf)
        memmove(s,s+3,n-2);
    if(f!=stdin)fclose(f);
    return 1;
}
static int parse_mode(const char *s,inv_mode *m) {
    if(!strcmp(s,"gui"))*m=INV_MODE_GUI;
    else if(!strcmp(s,"tui"))*m=INV_MODE_TUI;
    else if(!strcmp(s,"file"))*m=INV_MODE_FILE;
    else return 0;
    return 1;
}
static int parse_format(const char *s,inv_format *f) {
    if(!strcmp(s,"human"))*f=INV_FORMAT_HUMAN;
    else if(!strcmp(s,"table"))*f=INV_FORMAT_TABLE;
    else if(!strcmp(s,"csv"))*f=INV_FORMAT_CSV;
    else if(!strcmp(s,"md"))*f=INV_FORMAT_MD;
    else return 0;
    return 1;
}
static void list_compiled(const char *pattern) {
    size_t i,n;
    const inv_metric_desc *const *r=inv_registry(&n);
    printf("%-36s %-7s %-9s %-9s %-8s %s\n","KEY","TYPE","KIND","UNIT","MIN(s)","DESCRIPTION");
    for(i=0; i<n; i++)if(inv_glob_match(pattern,r[i]->key))printf("%-36s %-7s %-9s %-9s %-8.3f %s\n",r[i]->key,inv_type_name(r[i]->type),inv_kind_name(r[i]->kind),r[i]->unit,(double)r[i]->min_poll_ns/1e9,r[i]->description);
}
static int describe(const char *key) {
    const inv_metric_desc *m=inv_metric_find(key);
    const char *doc;
    if(!m) {
        fprintf(stderr,"inv: metric is not compiled: %s\n",key);
        return 2;
    }
    printf("%s -- %s\n\n%s\n\nType: %s\nKind: %s\nUnit: %s\nSemantic version: %u\nProvider: %s\nLabels: %s\nMinimum poll interval: %.3fs\nSensitive: %s\nDocumentation: %s\n", m->key,m->title,m->description,inv_type_name(m->type),inv_kind_name(m->kind),m->unit,m->semantic_version,inv_metric_source(m),m->label_schema,(double)m->min_poll_ns/1e9,m->flags&INV_SENSITIVE?"yes":"no",m->doc_path);
    doc=inv_metric_documentation(key);
    if(doc)printf("\n---\n%s\n",doc);
    return 0;
}
int inv_cli_main(int argc,char **argv) {
    inv_plan *plan;
    inv_engine *engine=NULL;
    char *query;
    char err[512];
    const char *command="run",*pattern="*",*key=NULL,*query_arg=NULL,*query_file=NULL,*output=NULL,*font=NULL;
    int i=1,mode_set=0,format_set=0,probe=0,get_count=0,once=0,redact=0,strict=0,fail_error=0,csv_safe=1,rc=0;
    inv_mode mode=INV_MODE_FILE;
    inv_format format=INV_FORMAT_HUMAN;
    uint64_t period=0,duration_ns=0,count=0,warmup=0;
    inv_status st;
    setlocale(LC_NUMERIC,"C");
    if(!inv_install_signal_handlers()) {
        fputs("inv: cannot install signal handlers\n",stderr);
        return 1;
    }
    if(argc==1) {
        usage(stdout);
        return 0;
    }
    if(!strcmp(argv[1],"--version")) {
        size_t n;
        inv_registry(&n);
        printf("inv %s | C99 | metrics=%zu | TUI=%d GUI=%d static=%d | ImGui adapter=1.91.9b\n",INV_VERSION,n,INV_WITH_TUI,INV_WITH_GUI,INV_STRICT_STATIC);
        return 0;
    }
    if(!strcmp(argv[1],"--help")||!strcmp(argv[1],"-h")) {
        usage(stdout);
        return 0;
    }
    if(argv[1][0]!='-') {
        command=argv[1];
        i++;
    }
    if(strcmp(command,"run")&&strcmp(command,"get")&&strcmp(command,"explain")&&strcmp(command,"list")&&strcmp(command,"describe")) {
        fprintf(stderr,"inv: unknown command '%s'\n",command);
        return 2;
    }
    query=(char *)calloc(INV_QUERY_CAP+1,1);
    plan=(inv_plan *)malloc(sizeof(*plan));
    if(!query||!plan) {
        free(query);
        free(plan);
        return 1;
    }
    for(; i<argc; i++) {
        const char *a=argv[i],*v=NULL;
        if(!strcmp(a,"--help")) {
            usage(stdout);
            goto done;
        }
        if(!strcmp(a,"--probe")) {
            probe=1;
            continue;
        }
        if(!strcmp(a,"--compiled"))continue;
        if(!strcmp(a,"--once")) {
            once=1;
            continue;
        }
        if(!strcmp(a,"--redact")) {
            redact=1;
            continue;
        }
        if(!strcmp(a,"--strict")) {
            strict=1;
            continue;
        }
        if(!strcmp(a,"--fail-on-error")) {
            fail_error=1;
            continue;
        }
        if(!strcmp(a,"--csv-unsafe-strings")) {
            csv_safe=0;
            continue;
        }
        if(a[0]!='-') {
            if(!strcmp(command,"list")) {
                if(strcmp(pattern,"*")) {
                    snprintf(err,sizeof(err),"only one list pattern is allowed");
                    goto bad;
                }
                pattern=a;
            }
            else if(!strcmp(command,"describe")) {
                if(key) {
                    snprintf(err,sizeof(err),"describe takes exactly one metric key");
                    goto bad;
                }
                key=a;
            }
            else if(!strcmp(command,"get")) {
                if(!append(query,INV_QUERY_CAP+1,"select ")||!append(query,INV_QUERY_CAP+1,a)||!append(query,INV_QUERY_CAP+1,";\n")) {
                    snprintf(err,sizeof(err),"selectors too long");
                    goto bad;
                }
                get_count++;
            }
            else {
                snprintf(err,sizeof(err),"unexpected argument '%s'",a);
                goto bad;
            }
            continue;
        }
        if(i+1>=argc) {
            snprintf(err,sizeof(err),"option %s requires a value",a);
            goto bad;
        }
        v=argv[++i];
        if(!strcmp(a,"--query")||!strcmp(a,"-q")) {
            if(query_arg||query_file) {
                snprintf(err,sizeof(err),"use one query source");
                goto bad;
            }
            query_arg=v;
        }
        else if(!strcmp(a,"--query-file")) {
            if(query_arg||query_file) {
                snprintf(err,sizeof(err),"use one query source");
                goto bad;
            }
            query_file=v;
        }
        else if(!strcmp(a,"--mode")) {
            if(!parse_mode(v,&mode)) {
                snprintf(err,sizeof(err),"invalid mode: %s",v);
                goto bad;
            }
            mode_set=1;
        }
        else if(!strcmp(a,"--format")) {
            if(!parse_format(v,&format)) {
                snprintf(err,sizeof(err),"invalid format: %s",v);
                goto bad;
            }
            format_set=1;
        }
        else if(!strcmp(a,"--output"))output=v;
        else if(!strcmp(a,"--font"))font=v;
        else if(!strcmp(a,"--every")) {
            if(inv_parse_duration(v,&period)!=INV_OK) {
                snprintf(err,sizeof(err),"invalid period: %s",v);
                goto bad;
            }
        }
        else if(!strcmp(a,"--duration")) {
            if(inv_parse_duration(v,&duration_ns)!=INV_OK) {
                snprintf(err,sizeof(err),"invalid duration: %s",v);
                goto bad;
            }
        }
        else if(!strcmp(a,"--warmup")) {
            if(inv_parse_duration(v,&warmup)!=INV_OK) {
                snprintf(err,sizeof(err),"invalid warmup: %s",v);
                goto bad;
            }
        }
        else if(!strcmp(a,"--count")) {
            if(!inv_parse_u64(v,&count)||!count) {
                snprintf(err,sizeof(err),"invalid count: %s",v);
                goto bad;
            }
        }
        else {
            snprintf(err,sizeof(err),"unknown option: %s",a);
            goto bad;
        }
    }
    st=inv_registry_validate(err,sizeof(err));
    if(st!=INV_OK)goto bad;
    if(!strcmp(command,"describe")) {
        if(!key) {
            snprintf(err,sizeof(err),"describe requires a key");
            goto bad;
        }
        rc=describe(key);
        goto done;
    }
    if(!strcmp(command,"list")&&!probe) {
        list_compiled(pattern);
        goto done;
    }
    if(get_count&&(query_arg||query_file)) {
        snprintf(err,sizeof(err),"get selectors cannot be combined with a query");
        goto bad;
    }
    if(query_arg) {
        if(!inv_copy(query,INV_QUERY_CAP+1,query_arg)) {
            snprintf(err,sizeof(err),"query too long");
            goto bad;
        }
    }
    else if(query_file) {
        if(!load_query(query_file,query,INV_QUERY_CAP+1)) {
            rc=2;
            goto done;
        }
    }
    else if(!strcmp(command,"list")&&probe) {
        if(snprintf(query,INV_QUERY_CAP+1,"mode file; format table; once; select optional %s;",pattern)>INV_QUERY_CAP) {
            snprintf(err,sizeof(err),"pattern too long");
            goto bad;
        }
    }
    else if(!query[0]) {
        if(!strcmp(command,"get")) {
            snprintf(err,sizeof(err),"get requires at least one selector");
            goto bad;
        }
        inv_copy(query,INV_QUERY_CAP+1,"select os.name; select optional os.memory.utilization as bar(min=0,max=1); select optional cpu.utilization as plot(min=0,max=1); select optional sensor.temperature as value; select optional rate(device.net.rx_bytes_total) as plot; ");
    }
    st=inv_query_parse(query,plan,err,sizeof(err));
    if(st!=INV_OK)goto bad;
    if(mode_set)plan->mode=mode;
    if(format_set)plan->format=format;
    if(period) {
        size_t j;
        for(j=0; j<plan->select_count; j++)if(!plan->selects[j].period_ns)plan->selects[j].period_ns=period;
        plan->period_ns=period;
        plan->once=0;
    }
    if(duration_ns) {
        plan->duration_ns=duration_ns;
        plan->once=0;
    }
    if(count) {
        plan->count=count;
        plan->once=0;
    }
    if(once) {
        plan->once=1;
        plan->once_explicit=1;
    }
    if(output&&!inv_copy(plan->output,sizeof(plan->output),output)) {
        snprintf(err,sizeof(err),"output path too long");
        goto bad;
    }
    if(font&&!inv_copy(plan->font_path,sizeof(plan->font_path),font)) {
        snprintf(err,sizeof(err),"font path too long");
        goto bad;
    }
    plan->redact=redact;
    plan->strict_poll|=strict;
    plan->fail_on_error=fail_error;
    plan->csv_safe=csv_safe;
    plan->warmup_ns=warmup;
    if(warmup&&!plan->once) {
        snprintf(err,sizeof(err),"--warmup requires a one-shot file report");
        goto bad;
    }
    st=inv_plan_validate(plan,err,sizeof(err));
    if(st!=INV_OK)goto bad;
    st=inv_engine_create(plan,NULL,&engine,err,sizeof(err));
    if(st!=INV_OK) {
        fprintf(stderr,"inv: %s\n",err);
        rc=st==INV_NOT_PRESENT?3:1;
        goto done;
    }
    if(!strcmp(command,"explain")) {
        inv_engine_explain(engine,stdout);
        goto done;
    }
    if(plan->mode==INV_MODE_FILE)rc=inv_run_file(engine,plan);
#if INV_WITH_TUI
    else if(plan->mode==INV_MODE_TUI)rc=inv_run_tui(engine,plan);
#endif
#if INV_WITH_GUI
    else if(plan->mode==INV_MODE_GUI)rc=inv_run_gui(engine,plan);
#endif
    else rc=2;
    if(!rc&&plan->fail_on_error&&inv_engine_has_sample_errors(engine))rc=3;
    if(inv_interrupted())rc=128+inv_interrupted();
    goto done;
    bad: fprintf(stderr,"inv: %s\n",err);
    rc=2;
    done: inv_engine_destroy(engine);
    free(query);
    free(plan);
    return rc;
}

#ifndef _WIN32
int main(int argc,char **argv) { return inv_cli_main(argc,argv); }
#endif
