/* SPDX-License-Identifier: MIT -- native Windows tests, not a mock WinAPI. */
#include "windows_internal.h"
#include "inv/view.h"
static unsigned checks,failures;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);failures++;}}while(0)
static void *thread_worker(void *p){InterlockedIncrement((volatile LONG *)p);return NULL;}
static void files_and_clock(void) {
    wchar_t tmp[MAX_PATH],path[MAX_PATH],*wide;
    char utf8[2048],text[128];FILE *f;
    volatile LONG seen=0;inv_thread thread;inv_mutex mutex;
    uint64_t t=inv_now_ns();
    CHECK(t>0);inv_sleep_ns(20*INV_NS_MS);CHECK(inv_now_ns()>t);
    CHECK(inv_realtime_ns()>UINT64_C(1500000000000000000));
    CHECK(inv_mutex_init(&mutex)==0);inv_mutex_lock(&mutex);inv_mutex_unlock(&mutex);inv_mutex_destroy(&mutex);
    CHECK(inv_thread_start(&thread,thread_worker,(void *)&seen)==0);inv_thread_join(&thread);CHECK(seen==1);
    CHECK(GetTempPathW(MAX_PATH,tmp)>0);
    swprintf(path,MAX_PATH,L"%sinv-%lu-\x0442\x0435\x0441\x0442.txt",tmp,(unsigned long)GetCurrentProcessId());
    CHECK(inv_win_utf8(utf8,sizeof(utf8),path));
    DeleteFileW(path);
    f=inv_create_output(utf8);CHECK(f!=NULL);
    if(f){CHECK(fputs("UTF-8 fixture\n",f)>=0);CHECK(fclose(f)==0);}
    f=inv_create_output(utf8);CHECK(f==NULL);if(f)fclose(f);
    CHECK(inv_read_text(utf8,text,sizeof(text))==INV_OK&&!strcmp(text,"UTF-8 fixture\n"));
    wide=inv_win_widen(utf8);CHECK(wide!=NULL&&wcscmp(wide,path)==0);free(wide);
    CHECK(DeleteFileW(path));
    CHECK(!inv_win_widen("\xc0\x80"));
}
static void live_session(void) {
    inv_plan *p=(inv_plan *)malloc(sizeof(*p));inv_engine *engine=NULL;inv_live *live=NULL;
    inv_model model;inv_sink sink;char error[512];inv_status st;uint64_t limit;
    CHECK(p!=NULL);if(!p)return;
    CHECK(inv_query_parse("mode file; every 250ms; count 3; select os.uptime;",p,error,sizeof(error))==INV_OK);
    CHECK(inv_plan_validate(p,error,sizeof(error))==INV_OK);
    st=inv_engine_create(p,NULL,&engine,error,sizeof(error));CHECK(st==INV_OK);
    if(st!=INV_OK){free(p);return;}
    inv_model_init(&model,p);sink=inv_model_sink(&model);
    st=inv_live_start(engine,p,&live);CHECK(st==INV_OK);
    if(st==INV_OK) {
        limit=inv_now_ns()+10*INV_NS_S;
        while(!inv_live_done(live)&&inv_now_ns()<limit){CHECK(inv_live_pump(live,&sink)==INV_OK);inv_sleep_ns(10*INV_NS_MS);}
        CHECK(inv_live_done(live));CHECK(inv_live_pump(live,&sink)==INV_OK);
        st=inv_live_finish(live,1);CHECK(st==INV_OK);CHECK(model.done);CHECK(model.end_status==INV_OK);CHECK(model.frames==3);CHECK(model.samples>=1&&model.samples<=model.frames);
    }
    inv_model_destroy(&model);inv_engine_destroy(engine);free(p);
}
int main(void) {
    CHECK(inv_install_signal_handlers());files_and_clock();live_session();
    printf("Native Windows: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
