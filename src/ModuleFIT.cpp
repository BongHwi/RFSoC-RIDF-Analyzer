#include "ModuleFIT.h"
#include <TObject.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

ClassImp(ModuleFIT);

ModuleFIT::ModuleFIT(){};

int ModuleFIT::decode(char *ptr, int sz, int data[4]){
  unsigned int *evtdata = (unsigned int*) ptr;
  int evtsize = sz/sizeof(unsigned int);
  int msk, loop;

  if(idx >= evtsize) {
    return -1;
  }
  
  loop = 1;
  while(loop){
    msk = (evtdata[idx]&0xf0000000) >> 28;
    switch(msk){
    case 0x06:
      geo = evtdata[idx]&0x00000fff;
      idx++;
      break;
    case 0x00:
    case 0x04:
      ch = (evtdata[idx]&0x07f00000) >> 20;
      edge = (evtdata[idx]&0x08000000) >> 27;
      data[0] = geo;
      data[1] = ch;
      data[2] = edge;
      data[3] = evtdata[idx]&0xfffff;
      loop = 0;
      idx++;
      break;
    default:
      // error or trigger data
      idx++;
    }
    if(idx >= evtsize){
      return -1;
    }
  }
  return 0;
}


void ModuleFIT::test(){
  printf("test func decode FIT\n");
}


