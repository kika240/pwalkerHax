#include "pokewalker.h"
#include "save_patch.h"
#include <3ds.h>
#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static uint8_t device[65536],baseline[65536],health[24];
static unsigned writes,flushes,backups,fail_write;
static poke_packet pending;
bool aptMainLoop(void){return true;}
void hidScanInput(void){}
u32 hidKeysDown(void){return KEY_L|KEY_R|KEY_X;}
u32 hidKeysHeld(void){return KEY_L|KEY_R|KEY_X;}
void gspWaitForVBlank(void){}
void ir_enable(void){}
void ir_disable(void){}
bool poke_init_session(void){return true;}
bool poke_eeprom_read(void*out,u16 addr,u8 n){assert((size_t)addr+n<=sizeof(device));memcpy(out,device+addr,n);return true;}
bool poke_eeprom_write(u16 addr,const void*data,u16 n){assert((size_t)addr+n<=sizeof(device));++writes;if(writes==fail_write)return false;memcpy(device+addr,data,n);return true;}
void create_poke_packet(poke_packet*p,u8 op,u8 extra,const u8*b,u16 n){memset(p,0,sizeof(*p));p->header.opcode=op;p->header.extra=extra;p->payload_size=n;if(n)memcpy(p->payload,b,n);}
void send_pokepacket(poke_packet*p){pending=*p;if(p->header.opcode==CMD_WRITE){++writes;assert(p->header.extra==0xF7);unsigned a=p->payload[0]-0x80;assert(a+p->payload_size-1<=sizeof(health));memcpy(health+a,p->payload+1,p->payload_size-1);}}
bool recv_pokepacket(poke_packet*p){memset(p,0,sizeof(*p));p->header=pending.header;return true;}
bool poke_flush_health(void){++flushes;memcpy(device+0x156,health,24);u8 sum=1;for(int i=0;i<24;++i)sum=(u8)(sum+health[i]);device[0x16E]=sum;memcpy(device+0x256,device+0x156,25);return true;}
static void b16(uint8_t*p,unsigned n){p[0]=(uint8_t)(n>>8);p[1]=(uint8_t)n;}
static void b32(uint8_t*p,uint32_t n){p[0]=(uint8_t)(n>>24);p[1]=(uint8_t)(n>>16);p[2]=(uint8_t)(n>>8);p[3]=(uint8_t)n;}
static void sum(size_t a,size_t n){uint8_t s=1;for(size_t i=0;i<n;++i)s=(uint8_t)(s+device[a+i]);device[a+n]=s;memcpy(device+a+256,device+a,n+1);}
static void reset(void){memcpy(device,baseline,sizeof(device));memcpy(health,device+0x156,24);writes=flushes=fail_write=0;}
static void check_backups(void){
 DIR*d=opendir("sdmc:/3ds/pwalkerHax/backups");assert(d);struct dirent*entry;backups=0;
 while((entry=readdir(d))){if(entry->d_name[0]=='.')continue;char path[512];snprintf(path,sizeof(path),"sdmc:/3ds/pwalkerHax/backups/%s",entry->d_name);FILE*f=fopen(path,"rb");assert(f);uint8_t b[65537];size_t n=fread(b,1,sizeof(b),f);fclose(f);assert(n==65536&&!memcmp(b,baseline,65536));++backups;}closedir(d);
}
int main(void){
 assert(!mkdir("sdmc:",0700));assert(!mkdir("sdmc:/3ds",0700));assert(!mkdir("sdmc:/3ds/pwalkerHax",0700));
 memcpy(device,"nintendo",8);device[0x83]=42;device[0xF9]=7;device[0x150]=9;device[0x8F00]=25;device[0x8F0C]=20;b16(device+0x164,100);sum(0x83,40);sum(0xED,104);sum(0x156,24);memcpy(baseline,device,sizeof(device));
 uint8_t raw[104]={0};memcpy(raw,"PWEDIT1\0",8);b16(raw+8,1);b16(raw+10,2);b32(raw+12,sizeof(raw));memcpy(raw+20,device+0x83,40);memcpy(raw+60,device+0xF9,4);memcpy(raw+64,device+0x14D,4);memcpy(raw+68,device+0x8F00,16);
 b16(raw+88,0x164);b16(raw+90,2);b16(raw+92,100);b16(raw+94,777);b16(raw+96,0xCEBC);b16(raw+98,2);raw[102]=1;b32(raw+16,pw_crc32(raw,sizeof(raw)));
 FILE*f=fopen("sdmc:/3ds/pwalkerHax/import.pwe","wb");assert(f&&fwrite(raw,1,sizeof(raw),f)==sizeof(raw));fclose(f);
 reset();poke_preview_save_edit();assert(writes==0&&flushes==0);
 reset();f=fopen("sdmc:/3ds/pwalkerHax/backups","wb");assert(f);fclose(f);poke_import_save_edit();assert(writes==0&&flushes==0);assert(!memcmp(device,baseline,sizeof(device)));assert(!unlink("sdmc:/3ds/pwalkerHax/backups"));
 reset();device[0x83]^=1;sum(0x83,40);poke_import_save_edit();assert(writes==0&&flushes==0);
 reset();poke_import_save_edit();assert(writes==2&&flushes==1&&device[0x164]==3&&device[0x165]==9&&device[0xCEBC]==1);check_backups();assert(backups==1);
 reset();fail_write=2;poke_import_save_edit();assert(writes==2&&flushes==0&&device[0xCEBC]==0);check_backups();assert(backups==2);
 puts("PASS: preview has no writes, failed backup/wrong device block writes, verified backup precedes import, failed write stops import");
}
