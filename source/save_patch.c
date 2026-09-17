#include "save_patch.h"
#include <stdio.h>
#include <string.h>

static uint16_t be16(const uint8_t *p) { return (uint16_t)((uint16_t)p[0] << 8 | p[1]); }
static uint16_t le16(const uint8_t *p) { return (uint16_t)((uint16_t)p[1] << 8 | p[0]); }
static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 | (uint32_t)p[2]<<8 | p[3]; }
static bool fail(char *error, size_t cap, const char *message) { if (cap) snprintf(error, cap, "%s", message); return false; }
uint32_t pw_crc32(const uint8_t *p, size_t n) {
    uint32_t crc = UINT32_MAX;
    for (size_t i=0; i<n; ++i) {
        crc ^= p[i];
        for (unsigned bit=0; bit<8; ++bit) crc = (crc>>1) ^ ((crc&1) ? 0xEDB88320u : 0u);
    }
    return ~crc;
}
static bool allowed(uint16_t a, uint16_t n) {
    if (a==0x156) return n==4;
    if (a==0x162 || a==0x164) return n==2;
    if (a>=0xCE8C && a<=0xCEAC && (a-0xCE8C)%16==0) return n==16;
    if (a>=0xCEBC && a<=0xCEEC && (a-0xCEBC)%4==0) return n==2;
    return false;
}
static bool item_valid(uint16_t item) { return item<=536 && !(item>=113 && item<=134); }
static bool replacement_valid(const pw_edit *e) {
    const uint8_t *b=e->after;
    if (e->address==0x156) return be32(b)<=9999999u;
    if (e->address==0x162) return true;
    if (e->address==0x164) return be16(b)<=9999;
    if (e->length==2) return item_valid(le16(b));
    if (le16(b)==0) { for (unsigned i=0;i<16;++i) if (b[i]) return false; return true; }
    if (le16(b)>493 || !item_valid(le16(b+2)) || b[12]<1 || b[12]>100 || (b[13]&0xC0) || (b[14]&0xFC)) return false;
    for (unsigned i=4;i<12;i+=2) if (le16(b+i)>467) return false;
    return true;
}
bool pw_edit_is_health(const pw_edit *e) { return e->address < 0x280; }
const char *pw_edit_name(const pw_edit *e) {
    if (e->address==0x156) return "Total steps";
    if (e->address==0x162) return "Total days";
    if (e->address==0x164) return "Watts";
    if (e->address<0xCEBC) return "Caught Pokemon";
    if (e->address<0xCEC8) return "Dowsed item";
    return "Peer gift";
}
bool pw_patch_parse(pw_patch *out, const uint8_t *data, size_t length, char *error, size_t cap) {
    if (!out || !data || length<PW_PATCH_HEADER || length>PW_PATCH_MAX) return fail(error,cap,"Invalid patch length");
    if (memcmp(data,"PWEDIT1\0",8) || be16(data+8)!=1 || be32(data+12)!=length) return fail(error,cap,"Unknown patch format or length mismatch");
    for (size_t i=84;i<88;++i) if (data[i]) return fail(error,cap,"Unsupported reserved header flags");
    size_t count=be16(data+10);
    if (!count || count>PW_PATCH_RECORDS) return fail(error,cap,"Invalid edit count");
    uint8_t copy[PW_PATCH_MAX]; memcpy(copy,data,length); memset(copy+16,0,4);
    if (pw_crc32(copy,length)!=be32(data+16)) return fail(error,cap,"Patch CRC32 mismatch");
    pw_patch p={0}; p.count=count;
    memcpy(p.identity,data+20,40); memcpy(p.trainer,data+60,4);
    memcpy(p.walk_time,data+64,4); memcpy(p.companion,data+68,16);
    size_t cursor=PW_PATCH_HEADER; uint32_t last_end=0;
    for (size_t i=0;i<count;++i) {
        if (length-cursor<4) return fail(error,cap,"Truncated edit header");
        pw_edit *e=&p.edits[i]; e->address=be16(data+cursor); e->length=be16(data+cursor+2); cursor+=4;
        if (!allowed(e->address,e->length) || e->address<last_end) return fail(error,cap,"Protected, overlapping or unsupported edit range");
        if (length-cursor<2u*e->length) return fail(error,cap,"Truncated edit data");
        memcpy(e->before,data+cursor,e->length); memcpy(e->after,data+cursor+e->length,e->length); cursor+=2u*e->length;
        if (!memcmp(e->before,e->after,e->length) || !replacement_valid(e)) return fail(error,cap,"Invalid or empty replacement");
        last_end=(uint32_t)e->address+e->length;
    }
    if (cursor!=length) return fail(error,cap,"Unexpected trailing data");
    *out=p; return true;
}
static uint8_t checksum(const uint8_t *data,size_t n) { uint8_t sum=1; while(n--) sum=(uint8_t)(sum+*data++); return sum; }
static bool reliable(const uint8_t *b,size_t a,size_t n) {
    return b[a+n]==checksum(b+a,n) && b[a+256+n]==checksum(b+a+256,n) && !memcmp(b+a,b+a+256,n);
}
bool pw_patch_check(const pw_patch *p,const uint8_t *b,size_t n,char *error,size_t cap) {
    if (!p || !b || n!=PW_EEPROM_SIZE || memcmp(b,"nintendo",8)) return fail(error,cap,"Invalid EEPROM image");
    if (!p->count || p->count>PW_PATCH_RECORDS) return fail(error,cap,"Invalid patch count");
    if (!reliable(b,0x83,40) || !reliable(b,0xED,104) || !reliable(b,0x156,24)) return fail(error,cap,"Damaged or inconsistent EEPROM checksums");
    if (memcmp(p->identity,b+0x83,40) || memcmp(p->trainer,b+0xF9,4)) return fail(error,cap,"This edit belongs to another Pokewalker or trainer");
    if (memcmp(p->walk_time,b+0x14D,4) || memcmp(p->companion,b+0x8F00,16)) return fail(error,cap,"Walk changed: make a fresh dump and edit it again");
    uint32_t last_end=0;
    for(size_t i=0;i<p->count;++i) {
        const pw_edit *e=&p->edits[i];
        if (!allowed(e->address,e->length) || e->address<last_end || !replacement_valid(e)) return fail(error,cap,"Unsupported edit");
        last_end=(uint32_t)e->address+e->length;
        if (memcmp(b+e->address,e->before,e->length)) return fail(error,cap,"An edited field changed since the dump: make a fresh dump");
    }
    return true;
}
bool pw_patch_apply_image(const pw_patch *p,uint8_t *b,size_t n,char *error,size_t cap) {
    if (!pw_patch_check(p,b,n,error,cap)) return false;
    bool health=false;
    for(size_t i=0;i<p->count;++i) { memcpy(b+p->edits[i].address,p->edits[i].after,p->edits[i].length); health |= pw_edit_is_health(&p->edits[i]); }
    if (health) { b[0x16E]=checksum(b+0x156,24); memcpy(b+0x256,b+0x156,25); }
    return true;
}
