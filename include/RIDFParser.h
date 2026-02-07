#ifndef __RIDFPARSER__
#define __RIDFPARSER__

#include <TObject.h>
#include "ModuleAbst.h"
#include "RIDFPull.h"

class RIDFParser{
public:
  RIDFParser();
  virtual ~RIDFParser();

  // for debug purpose
  void test(void);
  
  int getblockdata(FILE *fd, char *buff);

  /**
   * @fn
   * @brief  : get index of next event
   * @buff   : in  : data buffer
   * @idx    : in  : offset of buffer
   * @sz     : in  : size of data buffer
   * @nidx   : out : next event index
   * @sidx   : out : next segment index
   * @evtn   : out : event number
   * @ts     : out : timestamp (if ts event header)
   * @return : index of this event
   */
  int getevtindex(char *buff, int idx, int sz, int *nidx, int *sidx,
		int *evtn, unsigned long long int *ts);

  /** 
   * @fn
   * @brief  : get index of next segment
   * @buff   : in  : data buffer
   * @sidx   : in  : offset of buffer
   * @sz     : in  : size of data buffer
   * @nsidx  : out : next segment index
   * @segid  : out : segment id of this segment
   * @return : index of this segment
   */
  int getsegindex(char *buff, int sidx, int sz, int *nsidx, int *segid);


  /** 
   * @fn
   * @brief  : get buffer pointer of this segment
   * @buff   : in  : data buffer
   * @sidx   : in  : offset of buffer
   * @ssz    : out : size of segment content
   * @return : pointer of this segment content
   */
  char* getsegbuff(char *buff, int sidx, int *ssz);

  int file(const char *file);
  int online(const char *host);
  char* nextevtdata(int *evtn, int *idx, int *sz, int *flag);
  int close(void);
  int rewindfile(void);
  char *status(void);
  int *listsegid(int *sz);
  void showsegid(void);
  int getgblock();

  /**
   * @fn
   * @evtn   : event number
   * @return : flag 0=normal, 1=no evt data, -2=end of file, -3=not file open
   */
  int nextevt(int *evtn);

  /**
   * @fn
   * @segid  : segment id
   * @return : 0 = normal, -1 = end of segment data
   */
  int nextseg(int *segid);

  /**
   * @fn
   * @segid   : segment id (only lower 8bit will be used), -1 case, do not parse module data
   * @data[4] : geo, ch, edge, value
   * @return  : 0=normal, 1=no decoder (return 32bit value), -1=end of segment
   */
  int nextdata(int segid, int data[4]);

  int mksegid(int dev, int fp, int det, int mod){
    return (0x3f & dev) << 20 | (0x3f & fp) << 14 |(0x3f & det) << 8 | mod;
  }

  int segdev(int seg){ return (seg >> 20) & 0x3f; }
  int segfp(int seg){ return (seg >> 14) & 0x3f; }
  int segdet(int seg){ return (seg >> 8) & 0x3f; }
  int segmod(int seg){ return seg & 0xff; }


  ClassDef(RIDFParser, 1);

private:
  int gidx{0};
  int gsz{0};
  int gnidx{0};
  int gsidx{0};
  int gnsidx{0};
  int gssz{0};
  int gevtn{0};
  unsigned long long int gts{0};
  char *gbuff{NULL};
  FILE *gfd{NULL};
  int  *seglist{NULL};
  char gline[256]{0};
  char fpath[128]{0};
  ModuleAbst *decoder{NULL};
  RIDFPull *puller{NULL};

};

#endif
