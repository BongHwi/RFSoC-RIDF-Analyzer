#ifndef __MODULEC16__
#define __MODULEC16__

#include "ModuleAbst.h"

class ModuleC16 : public ModuleAbst {
public:
  ModuleC16();
  int decode(char *ptr, int sz, int data[4]);
  void test();

  ClassDef(ModuleC16, 1);

private:
  int evtflag{0};

};

#endif
