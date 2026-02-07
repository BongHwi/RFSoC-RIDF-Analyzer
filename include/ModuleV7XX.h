#ifndef __MODULEV7XX__
#define __MODULEV7XX__

#include "ModuleAbst.h"

class ModuleV7XX : public ModuleAbst {
public:
  ModuleV7XX();
  int decode(char *ptr, int sz, int data[4]);
  void test();

  ClassDef(ModuleV7XX, 1);

private:
  int evtflag{0};

};

#endif
