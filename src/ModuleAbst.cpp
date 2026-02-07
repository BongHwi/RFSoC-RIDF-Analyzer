#include "ModuleAbst.h"
#include <TObject.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

ClassImp(ModuleAbst);

ModuleAbst::ModuleAbst() : idx(0){};

int ModuleAbst::decode(char *ptr, int sz, int data[4]){
  printf("decode madc\n");
  return 1;
}

void ModuleAbst::test(){
  printf("test func decode abst\n");
}


