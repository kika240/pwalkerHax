#include "save_patch.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned char*load(const char*p,size_t*n,size_t max){FILE*f=fopen(p,"rb");if(!f)return NULL;unsigned char*b=malloc(max+1);if(!b){fclose(f);return NULL;}*n=fread(b,1,max+1,f);if(ferror(f)||*n>max){free(b);b=NULL;}fclose(f);return b;}
int main(int argc,char**argv){
 if(argc!=3){fprintf(stderr,"Usage: %s import.pwe PWEEPROM.bin\nRead-only validation; no IR, no device write.\n",argv[0]);return 2;}
 size_t pn=0,en=0;unsigned char*p=load(argv[1],&pn,PW_PATCH_MAX),*e=load(argv[2],&en,PW_EEPROM_SIZE);
 if(!p||!e){fprintf(stderr,"Cannot read input files or invalid length\n");free(p);free(e);return 1;}
 pw_patch patch;char error[180];int code=0;
 if(!pw_patch_parse(&patch,p,pn,error,sizeof(error))||!pw_patch_check(&patch,e,en,error,sizeof(error))){fprintf(stderr,"REFUSED: %s\n",error);code=1;}
 else {printf("VALID: %zu edits, matching device and walk\n",patch.count);for(size_t i=0;i<patch.count;++i)printf("%04X  %u bytes  %s\n",patch.edits[i].address,patch.edits[i].length,pw_edit_name(&patch.edits[i]));}
 free(p);free(e);return code;
}
