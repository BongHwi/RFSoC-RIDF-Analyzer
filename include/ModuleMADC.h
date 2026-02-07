#ifndef __MODULEMADC__
#define __MODULEMADC__

#include "ModuleAbst.h"

class ModuleMADC : public ModuleAbst {
public:
  ModuleMADC();
  int decode(char *ptr, int sz, int data[4]);
  void test();

  ClassDef(ModuleMADC, 1);

};

#endif
