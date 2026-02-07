#include "ModuleMADC.h"
#include <TObject.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

ClassImp(ModuleMADC);

ModuleMADC::ModuleMADC(){};

int ModuleMADC::decode(char *ptr, int sz, int data[4]){
  unsigned int *evtdata = (unsigned int*) ptr;
  int evtsize = sz/sizeof(unsigned int);
  int msk, loop;

  if(idx >= evtsize) {
    return -1;
  }

  loop = 1;
  while(loop){
    msk = (evtdata[idx]&0xc0000000)>>30;
    switch(msk){
    case 0x01:
      // Hedaer
      geo = (evtdata[idx]&0x00ff0000)>> 16;
      idx++;
      break;
    case 0x00:
      // Event
      ch = (evtdata[idx]&0x001f0000) >> 16;
      data[0] = geo;
      data[1] = ch;
      data[2] = edge;
      data[3] = evtdata[idx]&0x7fff;
      //printf("geo %d / ch %d / edge %d / data %d\n",
      //data[0], data[1], data[2], data[3]);
      loop = 0;
      idx++;
      break;
    case 0x11:
      // Ender
      geo = -1;
      idx++;
      break;
    default:
      idx++;
    }
    if(idx >= evtsize){
      return -1;
    }
  }

  return 0;
}

void ModuleMADC::test(){
  printf("test func decode madc\n");
}


