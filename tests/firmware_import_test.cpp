#include "emulator.h"
extern "C" {
#include "pokewalker.h"
}
#include <cassert>
#include <cstdio>
#include <fstream>
#include <vector>
#include <deque>
#include <algorithm>
static PWEmulator *device;
static std::deque<uint8_t> incoming;
static void tick(){
    if(!pw_run(device,PW_CLOCK/6000)){fprintf(stderr,"CPU: %s\n",pw_error(device));abort();}
    uint8_t b[1024];size_t n=pw_ir_transmit(device,b,sizeof(b));incoming.insert(incoming.end(),b,b+n);
    int16_t audio[64];pw_audio(device,audio,64);
}
static void run(double s){for(int i=0;i<int(s*6000);++i)tick();}
static void press(int key){pw_button(device,key,true);run(.15);pw_button(device,key,false);run(.35);}
extern "C" {
void ir_enable(void){}
void ir_disable(void){}
void ir_send_data(void *data,u32 n){incoming.clear();assert(pw_ir_receive(device,(uint8_t*)data,n));}
u32 ir_recv_data(void *data,u32 size){
 for(int i=0;i<24000 && incoming.empty();++i)tick();
 u32 n=std::min(size,(u32)incoming.size());
 for(u32 i=0;i<n;++i){((uint8_t*)data)[i]=incoming.front();incoming.pop_front();}
 return n;
}
}
int main(int argc,char**argv){
 if(argc!=3){fprintf(stderr,"Usage: test ROM EEPROM (read only)\n");return 2;}
 std::ifstream r(argv[1],std::ios::binary),e(argv[2],std::ios::binary);
 std::vector<uint8_t>rom{std::istreambuf_iterator<char>(r),{}},ee{std::istreambuf_iterator<char>(e),{}};
 char error[256];device=pw_create(rom.data(),rom.size(),ee.data(),ee.size(),error,sizeof(error));assert(device);
 pw_clock_origin(device,1789680000);
 run(1);press(1);run(1);press(16);run(1);press(16);run(1);press(16);press(16);incoming.clear();press(1);
 assert(poke_init_session());
 uint8_t image[65536];
 for(unsigned a=0;a<65536;a+=128)assert(poke_eeprom_read(image+a,(u16)a,128));
 puts("PASS: upstream 3DS handshake and complete EEPROM read against emulated firmware");
 uint8_t item[2]={1,0},result[25]={0};
 assert(poke_eeprom_write(0xCEBC,item,2));assert(poke_eeprom_read(result,0xCEBC,2));assert(!memcmp(item,result,2));
 uint8_t capture[16]={25,0,17,0,85,0,0,0,0,0,0,0,15,0,2,0};
 assert(poke_eeprom_write(0xCEAC,capture,16));assert(poke_eeprom_read(result,0xCEAC,16));assert(!memcmp(capture,result,16));
 poke_packet request,ack;uint8_t watts[3]={0x8E,0,42};
 create_poke_packet(&request,CMD_WRITE,0xF7,watts,sizeof(watts));send_pokepacket(&request);
 assert(recv_pokepacket(&ack)&&ack.header.opcode==CMD_WRITE&&ack.header.extra==0xF7&&ack.payload_size==0);
 assert(poke_flush_health());
 assert(poke_eeprom_read(result,0x156,25));assert(result[14]==0 && result[15]==42);
 uint8_t secondary[25];assert(poke_eeprom_read(secondary,0x256,25));assert(!memcmp(result,secondary,25));
 uint8_t sum=1;for(int i=0;i<24;++i)sum+=result[i];assert(sum==result[24]);
 assert(pw_watts(device)==42);
 create_poke_packet(&request,CMD_DISC,MASTER_EXTRA,nullptr,0);send_pokepacket(&request);run(2);
 assert(pw_watts(device)==42);
 puts("PASS: caught Pokemon/item writes, live RAM watts, upstream addWatts(0), both EEPROM checksums and disconnect persistence");
 pw_destroy(device);
}
