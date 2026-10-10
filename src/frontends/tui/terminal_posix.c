/* SPDX-License-Identifier: MIT */
#include "internal.h"
#include "terminal.h"
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <poll.h>
struct inv_terminal { struct termios saved; struct sigaction old_suspend; };
static volatile sig_atomic_t suspend_requested;
static void suspend_handler(int signal) { (void)signal; suspend_requested=1; }
static int enter(inv_terminal *t) {
    struct termios raw=t->saved;
    raw.c_lflag&=(tcflag_t)~(ICANON|ECHO|IEXTEN);
    raw.c_iflag&=(tcflag_t)~(IXON|ICRNL);
    raw.c_cc[VMIN]=0; raw.c_cc[VTIME]=0;
    if(tcsetattr(STDIN_FILENO,TCSAFLUSH,&raw))return 0;
    fputs("\033[?1049h\033[?25l\033[2J",stdout); fflush(stdout); return 1;
}
static void leave(inv_terminal *t) {
    fputs("\033[0m\033[?25h\033[?1049l",stdout); fflush(stdout);
    (void)tcsetattr(STDIN_FILENO,TCSAFLUSH,&t->saved);
}
int inv_terminal_open(inv_terminal **out,char *error,size_t cap) {
    struct sigaction action;
    inv_terminal *t;
    const char *term=getenv("TERM");
    *out=NULL;
    if((term&&!strcmp(term,"dumb")) || !isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        snprintf(error,cap,"TUI requires an interactive ANSI terminal; use --mode file for pipes"); return 0;
    }
    t=(inv_terminal *)calloc(1,sizeof(*t));
    if(!t) { snprintf(error,cap,"TUI: out of memory"); return 0; }
    if(tcgetattr(STDIN_FILENO,&t->saved) || !enter(t)) {
        snprintf(error,cap,"TUI: cannot configure terminal"); free(t); return 0;
    }
    memset(&action,0,sizeof(action)); action.sa_handler=suspend_handler; sigemptyset(&action.sa_mask);
    if(sigaction(SIGTSTP,&action,&t->old_suspend)) { leave(t); free(t); snprintf(error,cap,"TUI: cannot install suspend handler"); return 0; }
    *out=t; return 1;
}
void inv_terminal_close(inv_terminal *t) {
    if(!t)return;
    leave(t); (void)sigaction(SIGTSTP,&t->old_suspend,NULL); free(t);
}
void inv_terminal_size(inv_terminal *t,int *cols,int *rows) {
    struct winsize ws;
    (void)t; *cols=100; *rows=24;
    if(!ioctl(STDOUT_FILENO,TIOCGWINSZ,&ws)) { if(ws.ws_col)*cols=ws.ws_col; if(ws.ws_row)*rows=ws.ws_row; }
}
void inv_terminal_begin(inv_terminal *t) { (void)t; fputs("\033[H",stdout); }
void inv_terminal_line(inv_terminal *t,const char *text,int width) {
    int n=0;
    const unsigned char *p=(const unsigned char *)text;
    (void)t; fputs("\033[2K",stdout);
    /* Deterministic ASCII cell geometry on POSIX; file/GUI preserve UTF-8. */
    while(*p&&n<width-1) {
        unsigned ch=*p++;
        if(ch>=32&&ch<127)fputc((int)ch,stdout);
        else if(ch>=192) { while((*p&192)==128)p++; fputc('?',stdout); }
        else fputc(' ',stdout);
        n++;
    }
    fputs("\r\n",stdout);
}
int inv_terminal_present(inv_terminal *t) { (void)t; fputs("\033[J",stdout); return fflush(stdout)==0 && !ferror(stdout); }
static int byte(unsigned timeout,unsigned char *out) {
    struct pollfd fd;
    int status;
    ssize_t n;
    fd.fd=STDIN_FILENO; fd.events=POLLIN; fd.revents=0;
    status=poll(&fd,1,(int)timeout);
    if(status<0)return errno==EINTR?0:-1;
    if(!status)return 0;
    if(fd.revents&(POLLERR|POLLHUP|POLLNVAL))return -1;
    n=read(STDIN_FILENO,out,1);
    return n>0?1:n<0&&errno!=EINTR?-1:0;
}
int inv_terminal_key(inv_terminal *t,uint32_t *key,unsigned timeout) {
    unsigned char ch,next;
    int status;
    if(suspend_requested) {
        struct sigaction action;
        leave(t); memset(&action,0,sizeof(action)); action.sa_handler=SIG_DFL; sigemptyset(&action.sa_mask);
        (void)sigaction(SIGTSTP,&action,NULL); raise(SIGTSTP); suspend_requested=0;
        action.sa_handler=suspend_handler; (void)sigaction(SIGTSTP,&action,NULL);
        if(!enter(t))return -1;
    }
    status=byte(timeout,&ch); if(status!=1)return status;
    *key=ch;
    if(ch==27) {
        if(byte(20,&next)!=1)return 1;
        if(next!='[' && next!='O')return 1;
        if(byte(20,&next)!=1)return 1;
        switch(next) {
        case 'A':*key=INV_KEY_UP;break; case 'B':*key=INV_KEY_DOWN;break;
        case 'C':*key=INV_KEY_RIGHT;break;case 'D':*key=INV_KEY_LEFT;break;
        case 'H':*key=INV_KEY_HOME;break;case 'F':*key=INV_KEY_END;break;
        case '5':case '6': { unsigned char tail; *key=next=='5'?INV_KEY_PAGE_UP:INV_KEY_PAGE_DOWN; (void)byte(20,&tail); break; }
        default:return 0;
        }
    } else if(ch>=0xc2 && ch<=0xf4) {
        unsigned count=ch<0xe0?2:ch<0xf0?3:4,i;
        uint32_t code=ch&(count==2?31u:count==3?15u:7u);
        uint32_t minimum=count==2?0x80u:count==3?0x800u:0x10000u;
        for(i=1;i<count;i++) {
            if(byte(20,&next)!=1 || (next&0xc0)!=0x80) { *key='?'; return 1; }
            code=(code<<6)|(next&63u);
        }
        *key=code<minimum || code>0x10ffff || (code>=0xd800&&code<=0xdfff)?'?':code;
    }
    return 1;
}
