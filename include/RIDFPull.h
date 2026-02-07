#ifndef __RIDFPULL__
#define __RIDFPULL__

#include <TObject.h>

class RIDFPull{
 public:
  RIDFPull(std::string host);
  virtual ~RIDFPull();
  int mktcpsend(char *host, unsigned short port);
  int eb_get(int sock, int com, char *dest);
  int infcon(char *host);
  int pull(char *data);

  ClassDef(RIDFPull, 1);

private:
  int sock{0};
  //char *data{NULL};
  char ebhostname[128]{0};
  int blkn{0};
};


#endif
