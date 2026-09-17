#include "save_patch.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void b16(uint8_t*p,unsigned n){p[0]=(uint8_t)(n>>8);p[1]=(uint8_t)n;}
static void b32(uint8_t*p,uint32_t n){p[0]=(uint8_t)(n>>24);p[1]=(uint8_t)(n>>16);p[2]=(uint8_t)(n>>8);p[3]=(uint8_t)n;}
static void sum(uint8_t*b,size_t a,size_t n){uint8_t s=1;for(size_t i=0;i<n;++i)s=(uint8_t)(s+b[a+i]);b[a+n]=s;memcpy(b+a+256,b+a,n+1);}
static void fixture(uint8_t *b){
 memset(b,0,PW_EEPROM_SIZE);memcpy(b,"nintendo",8);
 for(unsigned i=0;i<40;++i)b[0x83+i]=(uint8_t)(i+1);
 b[0xF9]=42;b[0x150]=9;b[0x8F00]=25;b[0x8F0C]=20;
 b32(b+0x156,1200);b16(b+0x162,4);b16(b+0x164,100);
 sum(b,0x83,40);sum(b,0xED,104);sum(b,0x156,24);
}
static void crc(uint8_t*p,size_t n){b32(p+12,(uint32_t)n);memset(p+16,0,4);b32(p+16,pw_crc32(p,n));}
static size_t make(uint8_t*p,const uint8_t*b){
 memset(p,0,PW_PATCH_MAX);memcpy(p,"PWEDIT1\0",8);b16(p+8,1);b16(p+10,2);
 memcpy(p+20,b+0x83,40);memcpy(p+60,b+0xF9,4);memcpy(p+64,b+0x14D,4);memcpy(p+68,b+0x8F00,16);
 b16(p+88,0x164);b16(p+90,2);memcpy(p+92,b+0x164,2);b16(p+94,777);
 b16(p+96,0xCEBC);b16(p+98,2);memcpy(p+100,b+0xCEBC,2);p[102]=1;
 crc(p,104);return 104;
}
static uint8_t* read(const char*path,size_t*n){FILE*f=fopen(path,"rb");assert(f);assert(!fseek(f,0,SEEK_END));long len=ftell(f);assert(len>=0);rewind(f);uint8_t*b=malloc((size_t)len+1);assert(b);*n=fread(b,1,(size_t)len+1,f);assert(*n==(size_t)len);fclose(f);return b;}
int main(int argc,char**argv){
 uint8_t *base=malloc(PW_EEPROM_SIZE),*copy=malloc(PW_EEPROM_SIZE);assert(base&&copy);fixture(base);
 uint8_t raw[PW_PATCH_MAX],bad[PW_PATCH_MAX];size_t n=make(raw,base);pw_patch patch;char error[180];
 assert(pw_crc32((const uint8_t*)"123456789",9)==0xCBF43926u);
 assert(pw_patch_parse(&patch,raw,n,error,sizeof(error)));assert(pw_patch_check(&patch,base,PW_EEPROM_SIZE,error,sizeof(error)));
 memcpy(copy,base,PW_EEPROM_SIZE);assert(pw_patch_apply_image(&patch,copy,PW_EEPROM_SIZE,error,sizeof(error)));
 assert(copy[0x164]==3&&copy[0x165]==9&&copy[0xCEBC]==1);assert(!memcmp(copy+0x156,copy+0x256,25));
 for(size_t i=0;i<PW_EEPROM_SIZE;++i)if(i!=0x164&&i!=0x165&&i!=0x16E&&i!=0x264&&i!=0x265&&i!=0x26E&&i!=0xCEBC)assert(base[i]==copy[i]);
 assert(!pw_patch_check(&patch,copy,PW_EEPROM_SIZE,error,sizeof(error)));
 for(size_t i=0;i<n;++i)assert(!pw_patch_parse(&patch,raw,i,error,sizeof(error)));
 memcpy(bad,raw,n);bad[50]^=1;assert(!pw_patch_parse(&patch,bad,n,error,sizeof(error)));
 memcpy(bad,raw,n);b16(bad+88,0x0080);crc(bad,n);assert(!pw_patch_parse(&patch,bad,n,error,sizeof(error)));
 memcpy(bad,raw,n);b16(bad+96,0x0164);crc(bad,n);assert(!pw_patch_parse(&patch,bad,n,error,sizeof(error)));
 memcpy(bad,raw,n);b16(bad+94,10000);crc(bad,n);assert(!pw_patch_parse(&patch,bad,n,error,sizeof(error)));
 memcpy(bad,raw,n);bad[102]=120;crc(bad,n);assert(!pw_patch_parse(&patch,bad,n,error,sizeof(error)));
 assert(pw_patch_parse(&patch,raw,n,error,sizeof(error)));
 memcpy(copy,base,PW_EEPROM_SIZE);copy[0x83]^=1;sum(copy,0x83,40);assert(!pw_patch_check(&patch,copy,PW_EEPROM_SIZE,error,sizeof(error)));
 memcpy(copy,base,PW_EEPROM_SIZE);copy[0x150]^=1;sum(copy,0xED,104);assert(!pw_patch_check(&patch,copy,PW_EEPROM_SIZE,error,sizeof(error)));
 memcpy(copy,base,PW_EEPROM_SIZE);copy[0xCEBC]=2;assert(!pw_patch_check(&patch,copy,PW_EEPROM_SIZE,error,sizeof(error)));
 // Deterministic malformed-input corpus. Recomputing CRC exercises deeper parsing.
 uint32_t seed=7123;
 for(unsigned trial=0;trial<5000;++trial){memcpy(bad,raw,n);seed=seed*1664525u+1013904223u;size_t i=seed%n;bad[i]^=(uint8_t)((seed>>24)|1);crc(bad,n);(void)pw_patch_parse(&patch,bad,n,error,sizeof(error));}
 puts("PASS: patch bounds/CRC, ranges, stale fields, pairing/walk identity, mirrored checksums and malformed inputs");
 if(argc==4){
  size_t rn,bn,en;uint8_t*r=read(argv[1],&rn),*b=read(argv[2],&bn),*e=read(argv[3],&en);
  assert(pw_patch_parse(&patch,r,rn,error,sizeof(error)));assert(patch.count==19);
  assert(pw_patch_apply_image(&patch,b,bn,error,sizeof(error)));assert(bn==en&&!memcmp(b,e,bn));
  free(r);free(b);free(e);puts("PASS: Swift export -> C importer produces byte-identical EEPROM (19 edit groups)");
 }
 free(base);free(copy);return 0;
}
