#include "ModuleV7XX.h"
#include <TObject.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

ClassImp(ModuleV7XX);

ModuleV7XX::ModuleV7XX(){};

int ModuleV7XX::decode(char *ptr, int sz, int data[4]){
   unsigned int *evtdata = (unsigned int*) ptr;
   int evtsize = sz/sizeof(unsigned int);
   int ih, loop;

   if(idx >= evtsize) {
     return -1;
   }

   loop = 1;
   while(loop){
     ih = (evtdata[idx]&0x06000000);
     if (ih == 0x02000000) {
       geo = (evtdata[idx]&0xf8000000)>>27;
       evtflag = 1;
       idx++;
     } else if (ih == 0 && evtflag == 1) {
       ch = (evtdata[idx]&0x001f0000) >> 16;
       data[0] = geo;
       data[1] = ch;
       data[2] = edge;
       data[3] = evtdata[idx]&0x1fff;
       loop = 0;
       idx++;
     } else if (ih == 0x04000000) {
       evtflag = 0;
       idx++;
     } else {
       idx++;
     }
     if(idx >= evtsize){
       return -1;
     }
   }
   return 0;
}


void ModuleV7XX::test(){
  printf("test func decode V7XX\n");
}


