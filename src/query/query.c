/* SPDX-License-Identifier: MIT */
#include "internal.h"
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
typedef struct parser {
    const char *src;
    size_t pos;
    unsigned line,col,tline,tcol;
    int kind,failed;
    char text[INV_TEXT_CAP];
    char *error;
    size_t error_cap;
} parser;
enum {
    TOK_END=0, TOK_WORD=256, TOK_STRING
};
#if defined(__GNUC__) || defined(__clang__)
static int fail(parser *,const char *,...) __attribute__((format(printf,2,3)));
#endif
static int fail(parser *p,const char *fmt,...) {
    va_list a;
    char msg[256];
    if(p->failed)return 0;
    p->failed=1;
    va_start(a,fmt);
    vsnprintf(msg,sizeof(msg),fmt,a);
    va_end(a);
    snprintf(p->error,p->error_cap,"line %u, column %u: %s",p->tline,p->tcol,msg);
    return 0;
}
static char take(parser *p) {
    char c=p->src[p->pos++];
    if(c=='\n') {
        p->line++;
        p->col=1;
    }
    else p->col++;
    return c;
}
static int wordchar(unsigned char c) {
    return isalnum(c)||c=='_'||c=='.'||c=='*'||c=='?'||c=='-'||c=='/'||c=='%';
}
static int next(parser *p) {
    size_t n=0;
    char c;
    p->text[0]=0;
    for(; ; ) {
        while(isspace((unsigned char)p->src[p->pos]))take(p);
        if(p->src[p->pos]=='#'||(p->src[p->pos]=='/'&&p->src[p->pos+1]=='/')) {
            while(p->src[p->pos]&&p->src[p->pos]!='\n')take(p);
            continue;
        }
        break;
    }
    p->tline=p->line;
    p->tcol=p->col;
    c=p->src[p->pos];
    if(!c) {
        p->kind=TOK_END;
        return 1;
    }
    if(c=='"') {
        take(p);
        while((c=p->src[p->pos])&&c!='"') {
            c=take(p);
            if(c=='\\') {
                c=p->src[p->pos];
                if(!c)return fail(p,"unterminated escape");
                take(p);
                if(c=='n')c='\n';
                else if(c=='r')c='\r';
                else if(c=='t')c='\t';
                else if(c!='\\'&&c!='"')return fail(p,"unsupported string escape");
            }
            if(n+1>=sizeof(p->text))return fail(p,"token is too long");
            p->text[n++]=c;
        }
        if(!c)return fail(p,"unterminated string");
        take(p);
        p->kind=TOK_STRING;
    }
    else if(wordchar((unsigned char)c)) {
        while(wordchar((unsigned char)p->src[p->pos])) {
            if(n+1>=sizeof(p->text))return fail(p,"token is too long");
            p->text[n++]=take(p);
        }
        p->kind=TOK_WORD;
    }
    else {
        p->kind=(unsigned char)take(p);
        p->text[n++]=c;
    }
    p->text[n]=0;
    return 1;
}
static int isword(parser *p,const char *s) {
    return p->kind==TOK_WORD&&!strcmp(p->text,s);
}
static int expect(parser *p,int c) {
    if(p->kind!=c)return fail(p,"expected '%c', got '%s'",c,p->text);
    return next(p);
}
static int copytok(parser *p,char *dst,size_t cap) {
    if(p->kind!=TOK_WORD&&p->kind!=TOK_STRING)return fail(p,"expected a name or string");
    if(!inv_copy(dst,cap,p->text))return fail(p,"value is too long");
    return next(p);
}
void inv_plan_init(inv_plan *q) {
    memset(q,0,sizeof(*q));
    q->mode=INV_MODE_FILE;
    q->format=INV_FORMAT_HUMAN;
    q->once=1;
    q->period_ns=INV_NS_S;
    q->redraw_ns=200*INV_NS_MS;
    q->history_ns=300*INV_NS_S;
    q->columns=2;
    q->csv_safe=1;
}
inv_status inv_parse_duration(const char *s,uint64_t *ns) {
    char *end;
    double d,m;
    errno=0;
    d=strtod(s,&end);
    if(errno||end==s||!isfinite(d)||d<=0)return INV_BAD_QUERY;
    if(!strcmp(end,"ns"))m=1;
    else if(!strcmp(end,"us"))m=1000;
    else if(!strcmp(end,"ms"))m=1000000;
    else if(!strcmp(end,"s"))m=1e9;
    else if(!strcmp(end,"m"))m=60e9;
    else if(!strcmp(end,"h"))m=3600e9;
    else return INV_BAD_QUERY;
    if(d*m<1||d*m>604800e9)return INV_BAD_QUERY;
    *ns=(uint64_t)(d*m+0.5);
    return INV_OK;
}
static int duration(parser *p,uint64_t *v) {
    if(p->kind!=TOK_WORD||inv_parse_duration(p->text,v)!=INV_OK) return fail(p,"invalid duration; use e.g. 250ms, 1s, 5m (max 7d)");
    return next(p);
}
static int number(parser *p,double *v) {
    char *end;
    errno=0;
    *v=strtod(p->text,&end);
    if(p->kind!=TOK_WORD||errno||*end||end==p->text||!isfinite(*v))return fail(p,"expected a finite number");
    return next(p);
}
static int widget(parser *p,inv_view_spec *v) {
    const char *names[]= {
        "value","text","bar","gauge","plot","table"
    };
    size_t i;
    unsigned seen=0;
    for(i=0; i<INV_ARRAY_SIZE(names); i++)if(isword(p,names[i]))break;
    if(i==INV_ARRAY_SIZE(names))return fail(p,"unknown widget '%s'",p->text);
    v->widget=(inv_widget)i;
    if(!next(p))return 0;
    if(p->kind!='(')return 1;
    if(!next(p))return 0;
    while(p->kind!=')') {
        char k[32];
        unsigned bit=0;
        if(!copytok(p,k,sizeof(k))||!expect(p,'='))return 0;
        if(!strcmp(k,"title")) {
            bit=1;
            if(!copytok(p,v->title,sizeof(v->title)))return 0;
        }
        else if(!strcmp(k,"unit")) {
            bit=2;
            if(!copytok(p,v->unit,sizeof(v->unit)))return 0;
        }
        else if(!strcmp(k,"min")) {
            bit=4;
            v->has_min=1;
            if(!number(p,&v->min))return 0;
        }
        else if(!strcmp(k,"max")) {
            bit=8;
            v->has_max=1;
            if(!number(p,&v->max))return 0;
        }
        else if(!strcmp(k,"precision")) {
            double n;
            bit=16;
            if(!number(p,&n))return 0;
            if(n<0||n>9||n!=floor(n))return fail(p,"precision must be an integer in 0..9");
            v->precision=(int)n;
        }
        else return fail(p,"unknown widget option '%s'",k);
        if(seen&bit)return fail(p,"duplicate widget option '%s'",k);
        seen|=bit;
        if(p->kind==')')break;
        if(!expect(p,','))return 0;
        if(p->kind==')')return fail(p,"trailing comma is not allowed");
    }
    return expect(p,')');
}
static int selection(parser *p,inv_plan *q) {
    inv_select *s;
    int wrapped=0;
    if(q->select_count==INV_MAX_SELECT)return fail(p,"too many selections (max %d)",INV_MAX_SELECT);
    s=&q->selects[q->select_count++];
    memset(s,0,sizeof(*s));
    s->view.precision=2;
    if(isword(p,"optional")) {
        s->optional=1;
        if(!next(p))return 0;
    }
    if(isword(p,"rate")||isword(p,"delta")) {
        wrapped=1;
        s->transform=isword(p,"rate")?INV_RATE:INV_DELTA;
        if(!next(p)||!expect(p,'('))return 0;
    }
    if(p->kind!=TOK_WORD)return fail(p,"expected metric selector");
    if(!copytok(p,s->pattern,sizeof(s->pattern)))return 0;
    if(p->kind=='{') {
        if(!next(p))return 0;
        while(p->kind!='}') {
            char key[INV_NAME_CAP],val[INV_TEXT_CAP];
            if(!copytok(p,key,sizeof(key))||!expect(p,'=')||!copytok(p,val,sizeof(val)))return 0;
            if(inv_label_get(s->labels,s->label_count,key))return fail(p,"duplicate label '%s'",key);
            if(!inv_label_set(s->labels,&s->label_count,key,val))return fail(p,"too many labels");
            if(p->kind=='}')break;
            if(!expect(p,','))return 0;
            if(p->kind=='}')return fail(p,"trailing comma is not allowed");
        }
        if(!expect(p,'}'))return 0;
    }
    if(wrapped&&!expect(p,')'))return 0;
    if(isword(p,"as")) {
        if(!next(p)||!widget(p,&s->view))return 0;
    }
    if(isword(p,"every")) {
        if(!next(p)||!duration(p,&s->period_ns))return 0;
    }
    return expect(p,';');
}
inv_status inv_query_parse(const char *src,inv_plan *q,char *err,size_t cap) {
    parser p;
    uint32_t seen=0;
    int has_every=0;
    memset(&p,0,sizeof(p));
    p.src=src;
    p.line=1;
    p.col=1;
    p.error=err;
    p.error_cap=cap;
    if(cap)err[0]=0;
    inv_plan_init(q);
    if(strlen(src)>INV_QUERY_CAP) {
        snprintf(err,cap,"query exceeds %d bytes",INV_QUERY_CAP);
        return INV_LIMIT;
    }
    if(!next(&p))return INV_BAD_QUERY;
    while(p.kind!=TOK_END&&!p.failed) {
        char directive[32];
        uint32_t bit=0;
        if(p.kind!=TOK_WORD) {
            fail(&p,"expected directive, got '%s'",p.text);
            break;
        }
        if(!copytok(&p,directive,sizeof(directive)))break;
        if(!strcmp(directive,"select")) {
            if(!selection(&p,q))break;
            continue;
        }
        if(!strcmp(directive,"version")) {
            bit=1;
            if(!isword(&p,"1")) {
                fail(&p,"only INVQ version 1 is supported");
                break;
            }
            next(&p);
        }
        else if(!strcmp(directive,"mode")) {
            bit=2;
            if(isword(&p,"file"))q->mode=INV_MODE_FILE;
            else if(isword(&p,"tui"))q->mode=INV_MODE_TUI;
            else if(isword(&p,"gui"))q->mode=INV_MODE_GUI;
            else {
                fail(&p,"mode must be file, tui or gui");
                break;
            }
            next(&p);
        }
        else if(!strcmp(directive,"format")) {
            bit=4;
            if(isword(&p,"human"))q->format=INV_FORMAT_HUMAN;
            else if(isword(&p,"table"))q->format=INV_FORMAT_TABLE;
            else if(isword(&p,"csv"))q->format=INV_FORMAT_CSV;
            else if(isword(&p,"md"))q->format=INV_FORMAT_MD;
            else {
                fail(&p,"format must be human, table, csv or md");
                break;
            }
            next(&p);
        }
        else if(!strcmp(directive,"once")) {
            bit=8;
            q->once_explicit=1;
        }
        else if(!strcmp(directive,"every")) {
            bit=16;
            has_every=1;
            if(!duration(&p,&q->period_ns))break;
        }
        else if(!strcmp(directive,"redraw")) {
            bit=32;
            if(!duration(&p,&q->redraw_ns))break;
        }
        else if(!strcmp(directive,"history")) {
            bit=64;
            if(!duration(&p,&q->history_ns))break;
        }
        else if(!strcmp(directive,"for")) {
            bit=128;
            if(!duration(&p,&q->duration_ns))break;
        }
        else if(!strcmp(directive,"output")) {
            bit=256;
            if(!copytok(&p,q->output,sizeof(q->output)))break;
        }
        else if(!strcmp(directive,"strict")) {
            bit=512;
            q->strict_poll=1;
        }
        else if(!strcmp(directive,"count")) {
            bit=1024;
            if(p.kind!=TOK_WORD||!inv_parse_u64(p.text,&q->count)||!q->count) {
                fail(&p,"count must be a positive integer");
                break;
            }
            next(&p);
        }
        else if(!strcmp(directive,"layout")) {
            uint64_t n;
            bit=2048;
            if(!isword(&p,"grid")||!next(&p)||!expect(&p,'(')||!isword(&p,"columns")||!next(&p)||!expect(&p,'=')) {
                fail(&p,"expected layout grid(columns=N)");
                break;
            }
            if(!inv_parse_u64(p.text,&n)||n<1||n>8) {
                fail(&p,"columns must be in 1..8");
                break;
            }
            q->columns=(unsigned)n;
            if(!next(&p)||!expect(&p,')'))break;
        }
        else {
            fail(&p,"unknown directive '%s'",directive);
            break;
        }
        if(seen&bit) {
            fail(&p,"duplicate directive '%s'",directive);
            break;
        }
        seen|=bit;
        if(!expect(&p,';'))break;
    }
    if(p.failed)return INV_BAD_QUERY;
    q->once=q->once_explicit||!(has_every||q->duration_ns||q->count||q->mode!=INV_MODE_FILE);
    return INV_OK;
}
static int schema_has(const char *schema,const char *label) {
    size_t n=strlen(label);
    const char *p=schema;
    while(*p) {
        const char *e=strchr(p,',');
        size_t len=e?(size_t)(e-p):strlen(p);
        if(len==n&&!memcmp(p,label,n))return 1;
        if(!e)break;
        p=e+1;
    }
    return 0;
}
inv_status inv_plan_validate(inv_plan *p,char *err,size_t cap) {
    size_t i,j,k,n;
    const inv_metric_desc *const *r=inv_registry(&n);
#define BAD(...) do {snprintf(err,cap,__VA_ARGS__);return INV_BAD_QUERY;}while(0)
    if(!p->select_count)BAD("query has no select statements");
    if(p->period_ns<10*INV_NS_MS||p->redraw_ns<10*INV_NS_MS)BAD("period/redraw must be at least 10ms");
    if(p->history_ns>86400*INV_NS_S)BAD("history is limited to 24h");
    if(p->count>1000000)BAD("count is limited to 1000000");
    if(p->once&&(p->count||p->duration_ns))BAD("once cannot be combined with count/for");
    if(p->mode!=INV_MODE_FILE&&p->once_explicit)BAD("once is only valid in file mode");
    if(p->mode!=INV_MODE_FILE)p->once=0;
    if(p->mode!=INV_MODE_FILE&&p->output[0])BAD("output is only valid in file mode");
    if(p->mode!=INV_MODE_GUI&&p->font_path[0])BAD("--font is only valid in GUI mode");
#if !INV_WITH_GUI
    if(p->mode==INV_MODE_GUI)BAD("GUI was disabled at compile time (INV_WITH_GUI=OFF)");
#endif
#if !INV_WITH_TUI
    if(p->mode==INV_MODE_TUI)BAD("TUI was disabled at compile time (INV_WITH_TUI=OFF)");
#endif
    for(i=0; i<p->select_count; i++) {
        inv_select *s=&p->selects[i];
        size_t matched=0,compatible=0;
        if(!s->period_ns)s->period_ns=p->period_ns;
        if(s->period_ns<10*INV_NS_MS)BAD("selection %zu: period must be at least 10ms",i+1);
        if(s->view.has_min&&s->view.has_max&&s->view.max<=s->view.min)BAD("selection %zu: max must exceed min",i+1);
        for(j=0; j<n; j++)if(inv_select_matches_metric(s,r[j])) {
            int labels_ok=1;
            double scale;
            char unit[32];
            inv_value_type type=r[j]->type;
            matched++;
            for(k=0; k<s->label_count; k++)if(!schema_has(r[j]->label_schema,s->labels[k].key))labels_ok=0;
            if(!labels_ok)continue;
            compatible++;
            if(s->transform!=INV_RAW&&(r[j]->kind!=INV_COUNTER||r[j]->type!=INV_U64))BAD("%s: rate/delta requires a u64 counter",r[j]->key);
            if(s->transform==INV_RATE)type=INV_F64;
            if((s->view.widget==INV_WIDGET_BAR||s->view.widget==INV_WIDGET_GAUGE||s->view.widget==INV_WIDGET_PLOT)&& (type==INV_STRING||type==INV_BYTES||type==INV_BOOL))BAD("%s: selected widget requires a numeric metric",r[j]->key);
            snprintf(unit,sizeof(unit),"%s%s",r[j]->unit,s->transform==INV_RATE?"/s":"");
            if(s->view.unit[0]&&!inv_unit_scale(unit,s->view.unit,&scale))BAD("%s: cannot convert %s to %s",r[j]->key,unit,s->view.unit);
            if(p->strict_poll&&r[j]->refresh!=INV_REFRESH_ONCE&&s->period_ns<r[j]->min_poll_ns) BAD("%s: requested period is below the minimum of %.3fs",r[j]->key,(double)r[j]->min_poll_ns/1e9);
        }
        if(!matched&&!s->optional)BAD("no compiled metric matches '%s' (use 'inv list')",s->pattern);
        if(matched&&!compatible)BAD("'%s': label filter is not supported by matching metric schemas",s->pattern);
    }
    return INV_OK;
#undef BAD
}
