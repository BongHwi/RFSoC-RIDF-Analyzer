#ifndef __MODULEFIT__
#define __MODULEFIT__

#include "ModuleAbst.h"

class ModuleFIT : public ModuleAbst {
public:
  ModuleFIT();
  int decode(char *ptr, int sz, int data[4]);
  void test();

  ClassDef(ModuleFIT, 1);
private:
  int evtflag{0};

};

#endif
