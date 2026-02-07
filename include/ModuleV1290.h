#ifndef __MODULEV1290__
#define __MODULEV1290__

#include "ModuleAbst.h"

class ModuleV1290 : public ModuleAbst {
public:
  ModuleV1290();
  int decode(char *ptr, int sz, int data[4]);
  void test();

  ClassDef(ModuleV1290, 1);

private:
  int evtflag{0};

};

#endif
