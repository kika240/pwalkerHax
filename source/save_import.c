#define _POSIX_C_SOURCE 200809L
#include "pokewalker.h"
#include "save_patch.h"
#include "ir.h"
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#define EDIT_PATH "sdmc:/3ds/pwalkerHax/import.pwe"
#define BACKUP_DIR "sdmc:/3ds/pwalkerHax/backups"

static bool load_patch(pw_patch *patch) {
    uint8_t data[PW_PATCH_MAX+1];
    FILE *file=fopen(EDIT_PATH,"rb");
    if (!file) { printf("Missing %s\n",EDIT_PATH); return false; }
    size_t n=fread(data,1,sizeof(data),file); bool ok=!ferror(file); fclose(file);
    char error[160];
    if (!ok || !pw_patch_parse(patch,data,n,error,sizeof(error))) {
        printf("Invalid edit: %s\n",ok?error:"read error"); return false;
    }
    return true;
}
static void describe(const pw_patch *patch) {
    printf("%u field(s) to replace:\n",(unsigned)patch->count);
    for(size_t i=0;i<patch->count;++i) {
        const pw_edit *e=&patch->edits[i];
        if (pw_edit_is_health(e)) {
            uint32_t before=0,after=0;
            for(unsigned n=0;n<e->length;++n) { before=(before<<8)|e->before[n]; after=(after<<8)|e->after[n]; }
            printf("%s: %lu -> %lu\n",pw_edit_name(e),(unsigned long)before,(unsigned long)after);
        } else {
            unsigned a=e->before[0]|(unsigned)e->before[1]<<8;
            unsigned b=e->after[0]|(unsigned)e->after[1]<<8;
            printf("%s: ID %u -> %u\n",pw_edit_name(e),a,b);
            if(e->length==16) printf("Level %u -> %u, flags %u -> %u\n",e->before[12],e->after[12],e->before[14],e->after[14]);
        }
    }
    printf("Same device and walk required.\nFirmware: documented retail ROM.\nNo full-image flashing.\n");
}
static bool confirm_keys(u32 mask) {
    while(aptMainLoop()) {
        hidScanInput();
        if(hidKeysDown()&KEY_B) return false;
        if((hidKeysHeld()&mask)==mask && (hidKeysDown()&mask)) return true;
        gspWaitForVBlank();
    }
    return false;
}
void poke_preview_save_edit(void) {
    pw_patch patch;
    if(load_patch(&patch)) { describe(&patch); printf("Preview only. No IR or device write.\n"); }
}
static bool read_image(uint8_t *image) {
    for(uint32_t a=0;a<PW_EEPROM_SIZE;a+=128) {
        if (!poke_eeprom_read(image+a,(uint16_t)a,128)) { printf("EEPROM read failed @%04lX\n",(unsigned long)a); return false; }
        if (!(a%4096)) printf("Reading %lu / 65536\n",(unsigned long)a);
    }
    return true;
}
static bool backup_image(const uint8_t *image,char *path,size_t capacity) {
    (void)mkdir(BACKUP_DIR,0700);
    int fd=-1;
    for(unsigned i=0;i<100 && fd<0;++i) {
        snprintf(path,capacity,BACKUP_DIR "/pre-import-%lu-%u.bin",(unsigned long)time(NULL),i);
        fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0600);
    }
    if(fd<0) return false;
    FILE *file=fdopen(fd,"wb");
    if(!file) { close(fd); return false; }
    bool ok=fwrite(image,1,PW_EEPROM_SIZE,file)==PW_EEPROM_SIZE;
    if(fflush(file)) ok=false;
    if(fclose(file)) ok=false;
    if(!ok) return false;
    file=fopen(path,"rb"); if(!file) return false;
    uint8_t block[128];
    for(size_t i=0;i<PW_EEPROM_SIZE;i+=sizeof(block)) {
        if(fread(block,1,sizeof(block),file)!=sizeof(block) || memcmp(block,image+i,sizeof(block))) { ok=false; break; }
    }
    if(ferror(file)) ok=false;
    fclose(file); return ok;
}
static bool write_health_field(const pw_edit *edit) {
    // The existing upstream watts/steps feature uses this retail firmware RAM map.
    // Write only the explicitly selected field; preserve the rest of the live cache.
    uint16_t address=(uint16_t)(0xF780 + edit->address - 0x156);
    uint8_t bytes[17]; bytes[0]=(uint8_t)address; memcpy(bytes+1,edit->after,edit->length);
    poke_packet request,ack;
    create_poke_packet(&request,CMD_WRITE,(uint8_t)(address>>8),bytes,edit->length+1);
    send_pokepacket(&request);
    return recv_pokepacket(&ack) && ack.header.opcode==CMD_WRITE && ack.header.extra==(uint8_t)(address>>8) && ack.payload_size==0;
}
static bool flush_health(void) {
    // Reuse upstream's addWatts(0) operation: firmware computes both checksums and
    // persists the live 24-byte cache, preventing old RAM from undoing the edit.
    return poke_flush_health();
}
void poke_import_save_edit(void) {
    pw_patch patch; uint8_t *before=NULL,*after=NULL;
    bool connected=false,started=false; char error[160],backup[200];
    if(!load_patch(&patch)) return;
    describe(&patch);
    printf("Experimental physical import.\nL + R + X: back up then import; B: cancel.\n");
    if(!confirm_keys(KEY_L|KEY_R|KEY_X)) return;
    before=malloc(PW_EEPROM_SIZE); after=malloc(PW_EEPROM_SIZE);
    if(!before || !after) { printf("Out of memory\n"); goto done; }
    ir_enable(); connected=true;
    if(!poke_init_session() || !read_image(before)) goto done;
    if(!pw_patch_check(&patch,before,PW_EEPROM_SIZE,error,sizeof(error))) { printf("Refused: %s\n",error); goto done; }
    if(!backup_image(before,backup,sizeof(backup))) { printf("Backup failed. Nothing imported.\n"); goto done; }
    printf("Backup verified:\n%s\n",backup);
    // Verify again after SD backup I/O; do not pause the live IR session for a
    // second human confirmation, because the firmware would time out.
    if(!read_image(after) || !pw_patch_check(&patch,after,PW_EEPROM_SIZE,error,sizeof(error))) {
        printf("Connection/state changed. Retry from a fresh dump.\n"); goto done;
    }
    bool health=false;
    for(size_t i=0;i<patch.count;++i) {
        const pw_edit *e=&patch.edits[i];
        started=true;
        if(pw_edit_is_health(e)) {
            if(!write_health_field(e)) { printf("RAM update not acknowledged.\n"); goto done; }
            health=true;
        } else {
            uint8_t verify[16];
            if(!poke_eeprom_write(e->address,e->after,e->length) || !poke_eeprom_read(verify,e->address,(u8)e->length) || memcmp(verify,e->after,e->length)) {
                printf("Write/readback failed @%04X\n",e->address); goto done;
            }
        }
    }
    if(health && !flush_health()) { printf("Health cache flush not acknowledged.\n"); goto done; }
    if(!read_image(after)) goto done;
    for(size_t i=0;i<patch.count;++i) {
        const pw_edit *e=&patch.edits[i];
        if(memcmp(after+e->address,e->after,e->length) || (pw_edit_is_health(e) && memcmp(after+e->address+256,e->after,e->length))) {
            printf("Final verification failed @%04X\n",e->address); goto done;
        }
    }
    if(health) {
        uint8_t sum=1; for(unsigned i=0;i<24;++i) sum=(uint8_t)(sum+after[0x156+i]);
        if(after[0x16E]!=sum || memcmp(after+0x156,after+0x256,25)) { printf("Health checksum verification failed\n"); goto done; }
    }
    printf("IMPORT VERIFIED. Keep the backup.\n"); started=false;
done:
    if(started) printf("PARTIAL / UNCERTAIN IMPORT.\nDo not import again blindly.\nKeep backup and make a new dump.\n");
    if(connected) {
        poke_packet disconnect; create_poke_packet(&disconnect,CMD_DISC,MASTER_EXTRA,NULL,0); send_pokepacket(&disconnect); ir_disable();
    }
    free(before); free(after);
}
