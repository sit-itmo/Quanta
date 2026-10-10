/* SPDX-License-Identifier: MIT
 * Hardware primitives are tested only against owned RAM or temporary files.
 * No test reads /dev/mem, MSRs, real PCI config, or writes hardware. */
#include "quanta/hal.h"
#include "quanta/sensors.h"
#include "quanta/osal.h"
#include "quanta_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if QUANTA_OS_LINUX
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sched.h>
#endif
static unsigned checks, failures;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; fprintf(stderr,"%s:%d %s\n",__FILE__,__LINE__,#x); } } while (0)

typedef struct fixture
{
    unsigned reads, writes, closes;
    int fail, gpio;
    uint8_t *memory;
    size_t size;
} fixture;
static void close_mock(void *p) { ((fixture *)p)->closes++; }
static quanta_status read_mem(void *p, uint64_t address, void *out, size_t count)
{
    fixture *f=p;
    ++f->reads;
    if (address > f->size || count > f->size-address) return QUANTA_INVALID_DATA;
    memcpy(out,f->memory+(size_t)address,count);
    return f->fail ? QUANTA_IO_ERROR : QUANTA_OK;
}
static quanta_status write_mem(void *p, uint64_t address, const void *in, size_t count)
{
    fixture *f=p;
    ++f->writes;
    if (address > f->size || count > f->size-address) return QUANTA_INVALID_DATA;
    memcpy(f->memory+(size_t)address,in,count);
    return QUANTA_OK;
}
static quanta_status read_gpio(void *p, uint32_t controller, uint32_t pin, int *out)
{ fixture *f=p; (void)controller; (void)pin; *out=f->gpio; return QUANTA_OK; }
static quanta_status read_reg(void *p, quanta_register_kind kind, uint32_t index, uint32_t cpu, uint64_t *out)
{ fixture *f=p; (void)kind; (void)cpu; ++f->reads; *out=index; return f->fail ? QUANTA_IO_ERROR : QUANTA_OK; }
static quanta_status read_port(void *p, uint16_t port, unsigned width, uint32_t *out)
{ (void)p; (void)width; *out=width == 1 ? port & 255u : port; return QUANTA_OK; }
static quanta_status read_pci(void *p, quanta_pci_address address, unsigned width, uint32_t *out)
{ (void)p; (void)width; *out=width == 1 ? address.offset & 255u : address.offset; return QUANTA_OK; }
static quanta_hal *attach(fixture *f, const quanta_hal_options *options)
{
    quanta_hal_info info={0};
    quanta_hal_operations ops={0};
    quanta_hal *hal=NULL;
    strcpy(info.name,"ram-fixture"); info.maximum_physical_address=UINT64_MAX;
    info.maximum_transfer=8192;
    info.capabilities=UINT32_MAX; /* Must be recomputed, not trusted. */
    info.register_kinds=1u << QUANTA_REGISTER_MSR;
    ops.close=close_mock; ops.read_mem=read_mem; ops.write_mem=write_mem;
    ops.read_gpio=read_gpio; ops.read_reg=read_reg; ops.read_portio=read_port; ops.read_pci=read_pci;
    CHECK(quanta_hal_attach(&info,&ops,f,options,&hal)==QUANTA_OK);
    return hal;
}
static void test_contract(void)
{
    uint8_t ram[64]={0}, buffer[8];
    uint32_t scalar=999;
    uint64_t wide=999;
    int level=9;
    fixture f={0};
    quanta_hal_info info;
    quanta_hal_options options={0};
    quanta_physical_range range={8,16};
    quanta_pci_address pci={0};
    quanta_hal *hal;
    f.memory=ram; f.size=sizeof(ram);
    options.physical_ranges=&range; options.physical_range_count=1;
    hal=attach(&f,&options);
    range.base=32; /* The allowlist is copied, not borrowed. */
    CHECK(quanta_hal_get_info(hal,&info)==QUANTA_OK);
    CHECK(info.capabilities==(QUANTA_HAL_READ_REG|QUANTA_HAL_READ_MEM|QUANTA_HAL_WRITE_MEM|
                              QUANTA_HAL_READ_PORTIO|QUANTA_HAL_READ_PCI|QUANTA_HAL_READ_GPIO));
    CHECK(!info.writes_enabled);
    CHECK(quanta_hal_read_mem(hal,8,buffer,sizeof(buffer))==QUANTA_OK);
    CHECK(quanta_hal_read_mem(hal,20,buffer,sizeof(buffer))==QUANTA_PERMISSION_DENIED);
    CHECK(quanta_hal_read_mem(hal,UINT64_MAX-2,buffer,sizeof(buffer))==QUANTA_INVALID_DATA);
    CHECK(quanta_hal_read_mem(hal,8,buffer,0)==QUANTA_INVALID_DATA);
    CHECK(quanta_hal_read_mem(hal,8,NULL,8)==QUANTA_INVALID_DATA);
    CHECK(quanta_hal_read_mem(hal,8,buffer,8193)==QUANTA_LIMIT);
    CHECK(quanta_hal_write_mem(hal,8,buffer,8)==QUANTA_PERMISSION_DENIED && f.writes==0);
    CHECK(quanta_hal_read_reg(hal,QUANTA_REGISTER_MSR,123,0,&wide)==QUANTA_OK && wide==123);
    CHECK(quanta_hal_read_reg(hal,QUANTA_REGISTER_CONTROL,0,0,&wide)==QUANTA_UNSUPPORTED && wide==0);
    CHECK(quanta_hal_read_reg(hal,(quanta_register_kind)99,0,0,&wide)==QUANTA_INVALID_DATA);
    CHECK(quanta_hal_read_portio(hal,65535,2,&scalar)==QUANTA_INVALID_DATA && scalar==0);
    CHECK(quanta_hal_read_portio(hal,123,1,&scalar)==QUANTA_OK && scalar==123);
    for (unsigned width=0; width<16; ++width)
    {
        if (width==1 || width==2 || width==4) continue;
        CHECK(quanta_hal_read_portio(hal,0,width,&scalar)==QUANTA_INVALID_DATA);
        CHECK(quanta_hal_write_portio(hal,0,width,0)==QUANTA_INVALID_DATA);
        CHECK(quanta_hal_read_pci(hal,pci,width,&scalar)==QUANTA_INVALID_DATA);
    }
    pci.offset=4095;
    CHECK(quanta_hal_read_pci(hal,pci,1,&scalar)==QUANTA_OK && scalar==255);
    CHECK(quanta_hal_read_pci(hal,pci,2,&scalar)==QUANTA_INVALID_DATA);
    pci.offset=0; pci.device=32;
    CHECK(quanta_hal_read_pci(hal,pci,4,&scalar)==QUANTA_INVALID_DATA);
    f.gpio=2;
    CHECK(quanta_hal_read_gpio(hal,0,0,&level)==QUANTA_INVALID_DATA && level==0);
    f.gpio=1;
    CHECK(quanta_hal_read_gpio(hal,0,0,&level)==QUANTA_OK && level==1);
    f.fail=1; memset(ram,0xaa,sizeof(ram)); memset(buffer,0xbb,sizeof(buffer));
    CHECK(quanta_hal_read_mem(hal,8,buffer,sizeof(buffer))==QUANTA_IO_ERROR);
    CHECK(!memcmp(buffer,"\0\0\0\0\0\0\0\0",sizeof(buffer)));
    CHECK(quanta_hal_read_reg(hal,QUANTA_REGISTER_MSR,123,0,&wide)==QUANTA_IO_ERROR && wide==0);
    quanta_hal_close(hal); CHECK(f.closes==1);
    options.flags=QUANTA_HAL_ALLOW_WRITE;
    options.physical_range_count=0;
    hal=attach(&f,&options);
    CHECK(quanta_hal_write_mem(hal,8,buffer,8)==QUANTA_OK && f.writes==1);
    quanta_hal_close(hal); CHECK(f.closes==2);
    options.name="none"; options.flags=0;
    CHECK(quanta_hal_open(&options,&hal)==QUANTA_OK);
    CHECK(quanta_hal_get_info(hal,&info)==QUANTA_OK && info.capabilities==0);
    CHECK(quanta_hal_read_mem(hal,0,buffer,1)==QUANTA_UNSUPPORTED);
    CHECK(quanta_hal_read_reg(hal,QUANTA_REGISTER_MSR,0,0,&wide)==QUANTA_UNSUPPORTED);
    quanta_hal_close(hal);
    options.name="not-an-installed-hal";
    CHECK(quanta_hal_open(&options,&hal)!=QUANTA_OK && hal==NULL);
    CHECK(quanta_hal_open(NULL,NULL)==QUANTA_INVALID_DATA);
    CHECK(quanta_hal_attach(NULL,NULL,NULL,NULL,&hal)==QUANTA_INVALID_DATA && hal==NULL);
    quanta_hal_close(NULL);
}

#if QUANTA_WITH_DMI && QUANTA_ARCH_X86
static void little(uint8_t *out, uint64_t value, unsigned size)
{ for(unsigned i=0;i<size;++i) out[i]=(uint8_t)(value>>(8u*i)); }
static void make_checksum(uint8_t *bytes, size_t length, size_t field)
{ uint8_t sum=0; bytes[field]=0; for(size_t i=0;i<length;++i) sum=(uint8_t)(sum+bytes[i]); bytes[field]=(uint8_t)(0u-sum); }
static size_t make_table(uint8_t *table)
{
    /* Type 2 baseboard, vendor string #1, product #2, end-of-table. */
    const uint8_t bytes[]={2,8,0,0,1,2,0,0,'L','a','b',0,'T','e','s','t','B','o','a','r','d',0,0,127,4,1,0,0,0};
    memcpy(table,bytes,sizeof(bytes)); return sizeof(bytes);
}
static void test_physical_metric(void)
{
    fixture f={0};
    quanta_sample sample;
    const quanta_metric_desc *metric=quanta_metric_find("smbios.legacy_mem.board.model");
    quanta_hal *hal;
    uint8_t *entry;
    size_t table_size;
    CHECK(metric!=NULL);
    f.size=2u*1024u*1024u; f.memory=calloc(1,f.size);
    CHECK(f.memory!=NULL);
    if (!f.memory || !metric) {free(f.memory); return;}
    entry=f.memory+0xf0000;
    table_size=make_table(f.memory+0x100000);
    memcpy(entry,"_SM_",4); entry[5]=31; entry[6]=2; entry[7]=8;
    memcpy(entry+16,"_DMI_",5); little(entry+22,table_size,2); little(entry+24,0x100000,4);
    make_checksum(entry+16,15,5); make_checksum(entry,31,4);
    hal=attach(&f,NULL);
    CHECK(quanta_metric_read_once(metric,hal,NULL,&sample)==QUANTA_OK);
    CHECK(!strcmp(sample.value.as.string,"TestBoard") && f.reads>1 && f.writes==0);
    entry[4]++;
    CHECK(quanta_metric_read_once(metric,hal,NULL,&sample)==QUANTA_INVALID_DATA);
    memset(entry,0,32); memcpy(entry,"_SM3_",5); entry[6]=24; entry[7]=3; entry[8]=2;
    little(entry+12,table_size,4); little(entry+16,0x100000,8); make_checksum(entry,24,5);
    CHECK(quanta_metric_read_once(metric,hal,NULL,&sample)==QUANTA_OK);
    little(entry+12,2u*1024u*1024u,4); make_checksum(entry,24,5);
    CHECK(quanta_metric_read_once(metric,hal,NULL,&sample)==QUANTA_LIMIT);
    little(entry+12,table_size,4); little(entry+16,UINT64_MAX-1,8); make_checksum(entry,24,5);
    CHECK(quanta_metric_read_once(metric,hal,NULL,&sample)==QUANTA_INVALID_DATA);
    memset(entry,0,32);
    CHECK(quanta_metric_read_once(metric,hal,NULL,&sample)==QUANTA_NOT_PRESENT);
    CHECK(quanta_metric_read_once(metric,NULL,NULL,&sample)==QUANTA_UNSUPPORTED);
    CHECK(f.writes==0);
    quanta_hal_close(hal); CHECK(f.closes==1); free(f.memory);
}
#endif
#if QUANTA_OS_LINUX
static void test_linux_files(void)
{
    char root[]="/tmp/quanta-hal-XXXXXX", path[512];
    quanta_hal_options options={0};
    quanta_hal *hal=NULL;
    uint8_t buffer[4]={0};
    uint32_t value=0;
    int fd;
    CHECK(mkdtemp(root)!=NULL);
    snprintf(path,sizeof(path),"%s/mem",root); fd=open(path,O_RDWR|O_CREAT|O_EXCL,0600);
    CHECK(fd>=0);
    if (fd<0) return;
    CHECK(pwrite(fd,"abcd",4,16)==4); close(fd);
    options.name="linux-dev"; options.dev_root=root; options.sys_root=root;
    CHECK(quanta_hal_open(&options,&hal)==QUANTA_OK);
    CHECK(quanta_hal_read_mem(hal,16,buffer,4)==QUANTA_OK && !memcmp(buffer,"abcd",4));
    CHECK(quanta_hal_write_mem(hal,16,"xyz!",4)==QUANTA_PERMISSION_DENIED);
    CHECK(quanta_hal_read_mem(hal,18,buffer,4)==QUANTA_IO_ERROR && buffer[0]==0 && buffer[3]==0);
    CHECK(quanta_hal_read_portio(hal,0,1,&value)==QUANTA_NOT_PRESENT);
    quanta_hal_close(hal);
    options.flags=QUANTA_HAL_ALLOW_WRITE;
    CHECK(quanta_hal_open(&options,&hal)==QUANTA_OK);
    CHECK(quanta_hal_write_mem(hal,16,"xyz!",4)==QUANTA_OK);
    CHECK(quanta_hal_read_mem(hal,16,buffer,4)==QUANTA_OK && !memcmp(buffer,"xyz!",4));
    quanta_hal_close(hal); unlink(path);
    snprintf(path,sizeof(path),"%s/port",root); fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0600);
    CHECK(fd>=0); if(fd>=0){CHECK(pwrite(fd,"\x34\x12",2,32)==2); close(fd);}
    CHECK(quanta_hal_open(&options,&hal)==QUANTA_OK);
    CHECK(quanta_hal_read_portio(hal,32,1,&value)==QUANTA_OK && value==0x34);
    CHECK(quanta_hal_read_portio(hal,32,2,&value)==QUANTA_UNSUPPORTED);
    CHECK(quanta_hal_write_portio(hal,32,1,256)==QUANTA_INVALID_DATA);
    CHECK(quanta_hal_write_portio(hal,32,2,0xabcd)==QUANTA_UNSUPPORTED);
    CHECK(quanta_hal_write_portio(hal,32,1,0xab)==QUANTA_OK);
    CHECK(quanta_hal_read_portio(hal,32,1,&value)==QUANTA_OK && value==0xab);
    quanta_hal_close(hal); unlink(path);
    /* MSR and PCI use distinct namespaces; test actual file backends, not just wrappers. */
#if QUANTA_ARCH_X86
    snprintf(path,sizeof(path),"%s/cpu",root); CHECK(mkdir(path,0700)==0);
    snprintf(path,sizeof(path),"%s/cpu/0",root); CHECK(mkdir(path,0700)==0);
    snprintf(path,sizeof(path),"%s/cpu/0/msr",root); fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0600);
    CHECK(fd>=0); if(fd>=0){CHECK(pwrite(fd,"\x88\x77\x66\x55\x44\x33\x22\x11",8,0x10)==8); close(fd);}
    CHECK(quanta_hal_open(&options,&hal)==QUANTA_OK);
    {
        uint64_t msr=0;
        CHECK(quanta_hal_read_reg(hal,QUANTA_REGISTER_MSR,0x10,0,&msr)==QUANTA_OK && msr==UINT64_C(0x1122334455667788));
        CHECK(quanta_hal_read_reg(hal,QUANTA_REGISTER_MSR,0x10,QUANTA_HAL_CURRENT_CPU,&msr)==QUANTA_INVALID_DATA);
        CHECK(quanta_hal_write_reg(hal,QUANTA_REGISTER_MSR,0x10,0,UINT64_C(0xffeeddcc00112233))==QUANTA_OK);
        CHECK(quanta_hal_read_reg(hal,QUANTA_REGISTER_MSR,0x10,0,&msr)==QUANTA_OK && msr==UINT64_C(0xffeeddcc00112233));
    }
    quanta_hal_close(hal); unlink(path);
    snprintf(path,sizeof(path),"%s/cpu/0",root); rmdir(path);
    snprintf(path,sizeof(path),"%s/cpu",root); rmdir(path);
#endif
    {
        const char *dirs[]={"bus","bus/pci","bus/pci/devices","bus/pci/devices/0000:00:01.0"};
        quanta_pci_address address={0}; address.device=1; address.offset=4;
        for(size_t i=0;i<4;++i){snprintf(path,sizeof(path),"%s/%s",root,dirs[i]);CHECK(mkdir(path,0700)==0);}
        snprintf(path,sizeof(path),"%s/%s/config",root,dirs[3]); fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0600);
        CHECK(fd>=0); if(fd>=0){CHECK(pwrite(fd,"\x78\x56\x34\x12",4,4)==4);close(fd);}
        CHECK(quanta_hal_open(&options,&hal)==QUANTA_OK);
        CHECK(quanta_hal_read_pci(hal,address,4,&value)==QUANTA_OK && value==0x12345678);
        CHECK(quanta_hal_write_pci(hal,address,4,0xfedcba98)==QUANTA_OK);
        CHECK(quanta_hal_read_pci(hal,address,4,&value)==QUANTA_OK && value==0xfedcba98);
        quanta_hal_close(hal); unlink(path);
        for(size_t i=4;i>0;--i){snprintf(path,sizeof(path),"%s/%s",root,dirs[i-1]);rmdir(path);}
    }
    rmdir(root);
}
static void test_affinity(void)
{
    cpu_set_t before, after;
    quanta_affinity_state token={0};
    quanta_cpu_address cpu={0};
    quanta_hal *hal=NULL;
    int found=0;
    if(sched_getaffinity(0,sizeof(before),&before)!=0) return;
    for(unsigned i=0;i<CPU_SETSIZE;++i) if(CPU_ISSET(i,&before)){cpu.number=(uint16_t)i;found=1;break;}
    CHECK(found);
    if(!found) return;
    CHECK(quanta_hal_open(NULL,&hal)==QUANTA_OK);
    if(hal)
    {
        quanta_status status=quanta_hal_set_affinity(hal,cpu,&token);
        CHECK(status==QUANTA_OK || status==QUANTA_PERMISSION_DENIED || status==QUANTA_UNSUPPORTED);
        if(status==QUANTA_OK)
        {
            CHECK(token.active);
            CHECK(quanta_hal_set_affinity(hal,cpu,&token)==QUANTA_INVALID_DATA);
            token.owner_thread++;
            CHECK(quanta_hal_restore_affinity(hal,&token)==QUANTA_INVALID_DATA);
            token.owner_thread--;
            CHECK(quanta_hal_restore_affinity(hal,&token)==QUANTA_OK && !token.active);
            CHECK(sched_getaffinity(0,sizeof(after),&after)==0 && CPU_EQUAL(&before,&after));
            CHECK(quanta_hal_restore_affinity(hal,&token)==QUANTA_INVALID_DATA);
        }
        quanta_hal_close(hal);
    }
}

#endif
int main(void)
{
    test_contract();
#if QUANTA_WITH_DMI && QUANTA_ARCH_X86
    test_physical_metric();
#endif
#if QUANTA_OS_LINUX
    test_linux_files();
    test_affinity();
#endif
    printf("HAL primitive contracts: %u checks, %u failures\n",checks,failures);
    return failures ? 1 : 0;
}
