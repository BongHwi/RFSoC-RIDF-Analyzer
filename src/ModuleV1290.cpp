#include "ModuleV1290.h"
#include <TObject.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

ClassImp(ModuleV1290);

ModuleV1290::ModuleV1290(){};

int ModuleV1290::decode(char *ptr, int sz, int data[4]){
   unsigned int *evtdata = (unsigned int*) ptr;
   int evtsize = sz/sizeof(unsigned int);
   unsigned int ih, loop;

   if(idx >= evtsize) {
     evtflag = 0;
     return -1;
   }

   loop = 1;
   while(loop){
     ih = evtdata[idx]&0xf8000000;
     if (ih == 0x40000000) {
       // Global Header
       evtflag = 1;
       geo = (evtdata[idx]&0x0000001f);
       idx++;
     } else if (ih == 0x08000000) {
       // TDC Header
       idx++;
     } else if (ih == 0x00000000 && evtflag == 1) {
       ch = (evtdata[idx]&0x03e00000) >> 21;
       edge = (evtdata[idx]&0x04000000) >> 26;
       data[0] = geo;
       data[1] = ch;
       data[2] = edge;
       data[3] = evtdata[idx]&0x001fffff;
       loop = 0;
       idx ++;
     } else if (ih == 0x18000000) {
       // TDC Trailer
       idx ++;
     } else if (ih == 0x20000000) {
       // TDC Error
       idx ++;
     } else if (ih == 0x80000000) {
       // Global Trailer
       idx ++;
       evtflag = 0;
     }

     if(idx >= evtsize){
       evtflag = 0;
       return -1;
     }

   }

	 
   return 0;
}


void ModuleV1290::test(){
  printf("test func decode V1290\n");
}


