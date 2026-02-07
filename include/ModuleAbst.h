#ifndef __MODULEABST__
#define __MODULEABST__

#include <TObject.h>
class ModuleAbst {
public:
  ModuleAbst();
  virtual ~ModuleAbst(){}

  virtual int decode(char *ptr, int sz, int data[4]);
  virtual void test() = 0;

  ClassDef(ModuleAbst, 1);

protected:
  int idx{0};
  int geo{-1};
  int ch{0};
  int edge{0};
};

#endif
