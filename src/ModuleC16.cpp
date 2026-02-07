#include "ModuleC16.h"
#include <TObject.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

ClassImp(ModuleC16);

ModuleC16::ModuleC16(){};

int ModuleC16::decode(char *ptr, int sz, int data[4]){
  unsigned short *evtdata = (unsigned short*) ptr;
  int evtsize = sz/sizeof(unsigned short);

  if(idx >= evtsize) {
    return -1;
  }

  geo = 0;
  edge = 0;
  data[0] = geo;
  data[1] = ch;
  data[2] = edge;
  data[3] = (unsigned int)evtdata[idx];

  idx++;
  ch++;

  return 0;
}


void ModuleC16::test(){
  printf("test func decode C16\n");
}


