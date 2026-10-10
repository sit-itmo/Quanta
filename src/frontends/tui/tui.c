/* SPDX-License-Identifier: MIT */
#include "internal.h"
#include "inv/view.h"
#include "terminal.h"
#include <math.h>
static int display_error;
static inv_terminal *terminal; /* One active frontend per process. */
static const inv_model *sort_model;
static int sort_by_value;
static int compare_items(const void *a,const void *b) {
    size_t ia=*(const size_t *)a,ib=*(const size_t *)b;
    char na[1024],nb[1024];
    const inv_item *x=&sort_model->items[ia],*y=&sort_model->items[ib];
    if(sort_by_value) {
        double vx=0,vy=0;
        int ox=x->has_last&&x->last.status==INV_OK&&inv_value_number(&x->last.value,&vx);
        int oy=y->has_last&&y->last.status==INV_OK&&inv_value_number(&y->last.value,&vy);
        if(ox!=oy)return oy-ox;
        if(ox&&vx!=vy)return vx>vy?-1:1;
    }
    inv_series_name(na,sizeof(na),&x->meta);
    inv_series_name(nb,sizeof(nb),&y->meta);
    return strcmp(na,nb);
}
static void line(const char *text,int width) {
    inv_terminal_line(terminal,text,width);
}
static void bar(char *out,size_t cap,const inv_item *p,int width) {
    double v,min=p->meta.view.has_min?p->meta.view.min:0,max=p->meta.view.has_max?p->meta.view.max:1;
    int i,filled;
    if(!p->has_last||p->last.status!=INV_OK||!inv_value_number(&p->last.value,&v)||max<=min) {
        inv_copy(out,cap,"[ no current measurement ]");
        return;
    }
    width=INV_MAX(4,INV_MIN(width,(int)cap-4));
    v=(v-min)/(max-min);
    if(v<0)v=0;
    if(v>1)v=1;
    filled=(int)(v*width+0.5);
    out[0]='[';
    for(i=0; i<width; i++)out[i+1]=i<filled?'#':'-';
    out[width+1]=']';
    out[width+2]=0;
}
static void spark(char *out,size_t cap,const inv_item *p,int width,uint64_t now) {
    static const char levels[]=" .:-=+*#%@";
    double lo=0,hi=1;
    uint64_t first;
    size_t i;
    int found=0;
    width=INV_MAX(4,INV_MIN(width,(int)cap-1));
    memset(out,' ',(size_t)width);
    out[width]=0;
    if(!p->history_count)return;
    first=inv_history_at(p,0)->time_ns;
    for(i=0; i<p->history_count; i++) {
        const inv_point *t=inv_history_at(p,i);
        if(t->status!=INV_OK)continue;
        if(!found) {
            lo=hi=t->value;
            found=1;
        }
        else {
            if(t->value<lo)lo=t->value;
            if(t->value>hi)hi=t->value;
        }
    }
    if(p->meta.view.has_min)lo=p->meta.view.min;
    if(p->meta.view.has_max)hi=p->meta.view.max;
    if(hi<=lo)hi=lo+1;
    if(now<=first)now=first+1;
    for(i=0; i<p->history_count; i++) {
        const inv_point *t=inv_history_at(p,i);
        int x;
        double v;
        if(t->time_ns>now)continue;
        x=(int)((double)(t->time_ns-first)/(double)(now-first)*(width-1));
        x=INV_MAX(0,INV_MIN(width-1,x));
        if(t->status!=INV_OK) {
            out[x]='!';
            continue;
        }
        v=(t->value-lo)/(hi-lo);
        v=INV_MAX(0,INV_MIN(1,v));
        out[x]=levels[(int)(v*9+0.5)];
    }
}
static size_t render(const inv_model *m,const char *filter,size_t offset,int paused,int sorting,int filtering,int help) {
    int rows=24,cols=100,row=0;
    char b[4096],utc[64];
    size_t *indices,n=0,i;
    inv_terminal_size(terminal,&cols,&rows);
    cols=INV_MIN(cols,500);
    rows=INV_MIN(rows,300);
    inv_terminal_begin(terminal);
    if(rows<8||cols<40) {
        line("INV: resize the terminal to at least 40x8; q quits",cols);
        if(!inv_terminal_present(terminal))display_error=1;
        return offset;
    }
    inv_utc_string(m->frame.realtime_ns,utc,sizeof(utc));
    snprintf(b,sizeof(b),"INV %s | %s | %s | frames=%" PRIu64 " samples=%" PRIu64,INV_VERSION,paused?"DISPLAY PAUSED (collector running)":"LIVE",utc,m->frames,m->samples);
    line(b,cols);
    row++;
    line("q quit | p pause display | r refresh | / filter | j/k arrows scroll | s sort | ? help",cols);
    row++;
    snprintf(b,sizeof(b),"Filter: %s%s  Sort: %s",filter,filtering?"_":"",sorting?"value descending":"name");
    line(b,cols);
    row++;
    indices=(size_t *)malloc((m->count?m->count:1)*sizeof(*indices));
    if(!indices) {
        line("Out of memory",cols);
        if(!inv_terminal_present(terminal))display_error=1;
        return offset;
    }
    for(i=0; i<m->count; i++)if(m->items[i].active) {
        char name[2048];
        inv_series_name(name,sizeof(name),&m->items[i].meta);
        if(!*filter||strstr(name,filter)||strstr(m->items[i].meta.metric->title,filter))indices[n++]=i;
    }
    sort_model=m;
    sort_by_value=sorting;
    qsort(indices,n,sizeof(*indices),compare_items);
    if(n&&offset>=n)offset=n-1;
    if(!n)offset=0;
    if(help) {
        const char *lines[]= {
            "INVQ chooses both metric selection and widgets.","Static properties are cached. r rechecks them and enumerates devices.", "p freezes a copy of the display, not the acquisition thread.","Charts use actual measurement times; ! denotes a failed measurement.", "Errors do not become zero. (last good) is explicitly old data.","Filters match the metric key, labels and title (case-sensitive).", "Windows uses a Unicode console buffer; Linux uses ASCII-safe cells.","Use inv describe KEY for semantics, source and polling limits.","Press ? to close this help."
        };
        for(i=0; i<INV_ARRAY_SIZE(lines)&&row<rows-2; i++,row++)line(lines[i],cols);
    }
    else {
        for(i=offset; i<n&&row<rows-3; i++) {
            const inv_item *p=&m->items[indices[i]];
            char name[2048],value[1024],status[128],graph[512];
            inv_series_name(name,sizeof(name),&p->meta);
            inv_item_value(value,sizeof(value),p,m->redact);
            inv_item_status(status,sizeof(status),p,inv_now_ns());
            snprintf(b,sizeof(b),"%s%s",i==offset?"> ":"  ",p->meta.view.title[0]?p->meta.view.title:name);
            line(b,cols);
            row++;
            if(row>=rows-2)break;
            snprintf(b,sizeof(b),"    %s   [%s]",value,status);
            line(b,cols);
            row++;
            if(row<rows-2&&(p->meta.view.widget==INV_WIDGET_BAR||p->meta.view.widget==INV_WIDGET_GAUGE)) {
                bar(graph,sizeof(graph),p,cols-8);
                snprintf(b,sizeof(b),"    %s",graph);
                line(b,cols);
                row++;
            }
            else if(row<rows-2&&p->meta.view.widget==INV_WIDGET_PLOT) {
                spark(graph,sizeof(graph),p,cols-8,m->frame.monotonic_ns);
                snprintf(b,sizeof(b),"    %s",graph);
                line(b,cols);
                row++;
            }
        }
    }
    while(row<rows-2) {
        line("",cols);
        row++;
    }
    snprintf(b,sizeof(b),"%zu matching series; offset %zu; %s",n,offset,m->diagnostic);
    line(b,cols);
    if(!inv_terminal_present(terminal))display_error=1;
    free(indices);
    return offset;
}

static void filter_append(char *filter,size_t cap,uint32_t cp) {
    char bytes[4];size_t count=0,len=strlen(filter);
    if(cp<32||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))return;
    if(cp<0x80)bytes[count++]=(char)cp;
    else if(cp<0x800) {
        bytes[count++]=(char)(0xc0|(cp>>6));bytes[count++]=(char)(0x80|(cp&63));
    } else if(cp<0x10000) {
        bytes[count++]=(char)(0xe0|(cp>>12));bytes[count++]=(char)(0x80|((cp>>6)&63));bytes[count++]=(char)(0x80|(cp&63));
    } else {
        bytes[count++]=(char)(0xf0|(cp>>18));bytes[count++]=(char)(0x80|((cp>>12)&63));
        bytes[count++]=(char)(0x80|((cp>>6)&63));bytes[count++]=(char)(0x80|(cp&63));
    }
    if(len+count<cap){memcpy(filter+len,bytes,count);filter[len+count]=0;}
}
int inv_run_tui(inv_engine *engine,const inv_plan *plan) {
    inv_model model,frozen;
    inv_sink sink;
    inv_live *live=NULL;
    inv_status st=INV_OK;
    uint64_t next_draw=0;
    size_t offset=0;
    char filter[128]={0},error[256]={0};
    int paused=0,filtering=0,sorting=0,help=0,quit=0,rc=0;
    if(!inv_terminal_open(&terminal,error,sizeof(error))) {
        fprintf(stderr,"inv: %s; use --mode file for pipes\n",error);
        return 2;
    }
    display_error=0;
    inv_model_init(&model,plan);
    memset(&frozen,0,sizeof(frozen));
    sink=inv_model_sink(&model);
    st=inv_live_start(engine,plan,&live);
    if(st!=INV_OK){rc=1;goto finish;}
    while(!quit&&!inv_interrupted()) {
        uint64_t now;
        uint32_t ch=0;
        int key_result;
        st=inv_live_pump(live,&sink);
        if(st!=INV_OK){rc=1;break;}
        now=inv_now_ns();
        if(now>=next_draw) {
            offset=render(paused?&frozen:&model,filter,offset,paused,sorting,filtering,help);
            next_draw=now+plan->redraw_ns;
        }
        if(display_error){st=INV_IO_ERROR;rc=1;break;}
        if(inv_live_done(live))break;
        key_result=inv_terminal_key(terminal,&ch,20);
        if(key_result<0){st=INV_IO_ERROR;rc=1;break;}
        if(!key_result)continue;
        next_draw=0;
        if(ch==27){filtering=0;filter[0]=0;offset=0;continue;}
        if(filtering) {
            size_t len=strlen(filter);
            if(ch=='\r'||ch=='\n'){filtering=0;continue;}
            if(ch==127||ch==8) {
                if(len){do{len--;}while(len&&((unsigned char)filter[len]&0xc0)==0x80);filter[len]=0;}
            } else filter_append(filter,sizeof(filter),ch);
            offset=0;
            continue;
        }
        if(ch=='q'||ch==3)quit=1;
        else if(ch=='p') {
            if(!paused) {
                st=inv_model_clone(&frozen,&model);
                if(st!=INV_OK){rc=1;quit=1;}else paused=1;
            }else{inv_model_destroy(&frozen);paused=0;}
        }
        else if(ch=='r')inv_engine_refresh(engine);
        else if(ch=='/')filtering=1;
        else if(ch=='s'){sorting=!sorting;offset=0;}
        else if(ch=='j'||ch==INV_KEY_DOWN)offset++;
        else if(ch=='k'||ch==INV_KEY_UP){if(offset)offset--;}
        else if(ch==INV_KEY_PAGE_DOWN)offset+=8;
        else if(ch==INV_KEY_PAGE_UP)offset=offset>8?offset-8:0;
        else if(ch=='g'||ch==INV_KEY_HOME)offset=0;
        else if(ch==INV_KEY_END)offset=model.count;
        else if(ch=='?')help=!help;
    }
    if(live) {
        inv_status result=inv_live_finish(live,1);
        if(result!=INV_OK&&result!=INV_CANCELLED){st=result;rc=1;}
    }
finish:
    inv_model_destroy(&frozen);
    inv_model_destroy(&model);
    inv_terminal_close(terminal);terminal=NULL;
    if(rc)fprintf(stderr,"inv: TUI session failed: %s\n",inv_status_name(st));
    return rc;
}
