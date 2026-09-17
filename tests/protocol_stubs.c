#include "utils.h"
#include <string.h>
#include <ctype.h>
u16 swap16(u16 v){return (u16)(v<<8|v>>8);}
u32 swap32(u32 v){return __builtin_bswap32(v);}
void xor_data(void *p,u16 n){u8*b=p;while(n--)*b++^=0xAA;}
void decode_string(char*out,const u16*in){(void)in;out[0]=0;}
void string_to_img(void*dst,const u8 width,const char*str,bool centered){(void)dst;(void)width;(void)str;(void)centered;}
void string_upper(char*dst,const char*src){while(*src)*dst++=(char)toupper(*src++);*dst=0;}
void progress_bar(int c,int t,int w){(void)c;(void)t;(void)w;}
int msleep(int n){(void)n;return 0;}
