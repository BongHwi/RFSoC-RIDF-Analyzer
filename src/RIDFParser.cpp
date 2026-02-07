#include "RIDFParser.h"
#include "ridf.h"
#include "ModuleV7XX.h"
#include "ModuleMADC.h"
#include "ModuleFIT.h"
#include "ModuleC16.h"
#include "ModuleV1290.h"
#include <TObject.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

ClassImp(RIDFParser);

RIDFParser::RIDFParser(){
  if(!seglist){
    seglist = (int *)malloc(4*256);
  }
};

RIDFParser::~RIDFParser(){
}

void RIDFParser::test(){
}

int RIDFParser::file(const char *file){
  if(gfd){
    fclose(gfd);
  }

  if(puller){
    delete puller;
  }

  if(!gbuff){
    gbuff = (char *)malloc(1024*1024);
  }

  gfd = fopen(file, "r");
  gidx = 0;
  gsz = 0;
  gnidx = 0;
  gsidx = 0;
  gnsidx = 0;
  gevtn = 0;
  gssz = 0;

  if(gfd == NULL){
    return -1;
  }

  sprintf(fpath, "%s", file);

  return 1;
};

int RIDFParser::online(const char *host){
  if(gfd){
    fclose(gfd);
  }

  if(puller){
    delete puller;
  }

  if(!gbuff){
    gbuff = (char *)malloc(1024*1024);
  }

  gidx = 0;
  gsz = 0;
  gnidx = 0;
  gsidx = 0;
  gnsidx = 0;
  gevtn = 0;
  gssz = 0;

  sprintf(fpath, "%s", host);

  puller = new RIDFPull(host);
  
  return 1;
};


int RIDFParser::rewindfile(void){
  if(gfd){
    rewind(gfd);
    gidx = 0;
    gsz = 0;
    gnidx = 0;
    gsidx = 0;
    gnsidx = 0;
    gevtn = 0;
    return 1;
  }

  return 0;
}

int RIDFParser::close(void){
  if(gfd){
    fclose(gfd);
    return 1;
  }

  return 0;
}

char *RIDFParser::status(void){
  
  if(!gfd){
    sprintf(gline, "ridf is not opened");
  }else{
    sprintf(gline, "ridf %s", fpath);
  }

  return gline;
}

int *RIDFParser::listsegid(int *sz){
  int lsz, evtn, idx, esz, flag, tidx;
  int nidx, sidx, segid, nsidx;
  unsigned long long int ts;
  char *buff;

  lsz = 0;
  flag = 1;
  while(flag){
    buff = nextevtdata(&evtn, &idx, &esz, &flag);
    if(flag == 0){
      sidx = gsidx;
      tidx = gidx;
    }else{
      tidx = getevtindex(buff, idx, esz, &nidx, &sidx, &evtn, &ts);
    }

    if(tidx >= 0){
      while((sidx = getsegindex(buff, sidx, esz, &nsidx, &segid)) >= 0){
	memcpy((char *)(seglist+lsz), (char *)&segid, 4);
	sidx = nsidx;
	lsz ++;
      }
    }else{
      lsz = 0;
    }
  }
		      
  *sz = lsz;
  return seglist;
}

void RIDFParser::showsegid(void){
  int sz, i;
  unsigned int s;
  struct stbsegid bseg;

  listsegid(&sz);

  for(i=0;i<sz;i++){
    memcpy((char *)&s, (char *)(seglist+i), 4);
    memcpy((char *)&bseg, (char *)&s, 4);
    printf("Dev %2d / FP %2d / Det %2d / Mod %2d : 0x%08x \n",
	   bseg.device, bseg.focal, bseg.detector, bseg.module, s);
  }
}

// flag
// 0: normal
// 1: no evt data
// -2: end of file
// -3: not file open
char *RIDFParser::nextevtdata(int *evtn, int *idx, int *sz, int *flag){

  if(!gfd && !puller){
    *flag = -3;
    return NULL;
  }

  // read block data
  if(gidx == 0){
    gsz = getgblock();
    gsidx = 0;
    if(gsz > 0){
      gidx = 8;
    }
  }

  if(gsz == 0){
    // no online data
    *flag = 1;
    return NULL;
  }

  if(gidx < 0 || gsz < 0){
    *flag = -2;
    return NULL;
  }

  *sz = gsz;  

  // find evtdata
  gidx = getevtindex(gbuff, gidx, gsz, &gnidx, &gsidx, evtn, &gts);
  gnsidx = gsidx;
  if(gidx < 0){
    gidx = 0;
    *idx = 0;
    *flag = 1;
  }else if(gnidx < gsz - 4){
    *idx = gidx;
    gidx = gnidx;
    *flag = 0;
  }else{
    gidx = 0;
    *idx = 0;
    *flag = 0;
  }
  return gbuff;
}

int RIDFParser::getgblock(){
  if(gfd){
    return getblockdata(gfd, gbuff);
  }
  if(puller){
    return puller->pull(gbuff);
  }

  return -1;
}

int RIDFParser::getblockdata(FILE *fd, char *buff){
  int sz = 0;

  if(!feof(fd)){
    fread(buff, 8, 1, fd);
    memcpy((char *)&sz, buff, 4);
    sz = (sz & 0x003fffff) * 2;
    fread(buff+8, sz-8, 1, fd);
  }else{
    return -1;
  }

  return sz;
}

int RIDFParser::getevtindex(char *buff, int idx, int sz, int *nidx, int *sidx,
			    int *evtn, unsigned long long int *ts){
  int n = 0;
  int hd = 0;
  int cid=0, csz=0;

  n = idx;

  if(sz < 4){
    return 0;
  }

  while(n < sz-4){
    memcpy((char *)&hd, buff+n, 4);
    cid = (hd & 0x0fc00000) >> 22;
    csz = (hd & 0x003fffff) * 2;
    if(cid == 3 || cid == 6){
      *nidx = n + csz;
      memcpy((char *)evtn, buff+n+8, 4);
      if(cid == 3){
	*sidx = n + 12;
	*ts = 0;
      }else{  // cid=6 with TS
	*sidx = n + 20;
	memcpy((char *)ts, buff+n+12, 8);
      }
      return n;
    }else{
      n += csz;
    }
  }

  return -1;
}

int RIDFParser::getsegindex(char *buff, int sidx, int sz, int *nsidx, int *segid){
  int n = 0, hd = 0, csz=0, cid=0;

  n = sidx;

  while(n < sz-4){
    memcpy((char *)&hd, buff+n, 4);
    cid = (hd & 0x0fc00000) >> 22;
    csz = (hd & 0x003fffff) * 2;
    if(cid == 4){
      *nsidx = n + csz;
      memcpy((char *)segid, buff+n+8, 4);
      return n;
    }else{
      n += csz;
    }
  }  
  return -1;
}

char *RIDFParser::getsegbuff(char *buff, int sidx, int *ssz){
  int hd = 0, csz=0, cid=0;

  memcpy((char *)&hd, buff+sidx, 4);
  cid = (hd & 0x0fc00000) >> 22;
  csz = (hd & 0x003fffff) * 2;
  if(cid == 4){
    *ssz = csz - 12;
    return buff + sidx + 12;
  }else{
    return 0;
  }
}

int RIDFParser::nextevt(int *evtn){
  int tidx, tsz, tflag;

  nextevtdata(evtn, &tidx, &tsz, &tflag);

  return tflag;
}

//0 = normal, -1 = end of segment data
int RIDFParser::nextseg(int *segid){
  int mod;

  gsidx = gnsidx;
  gsidx = getsegindex(gbuff, gsidx, gsz, &gnsidx, segid);
  gssz = gnsidx - gsidx - 12;
  mod = *segid & 0xff;

  if(decoder) delete decoder;
  
  switch(mod){
  case 0:
    decoder = new ModuleC16();
    break;
  case 21:
    decoder = new ModuleV7XX();
    break;
  case 25:
    decoder = new ModuleV1290();
    break;
  case 32:
    decoder = new ModuleMADC();
    break;
  case 47:
    decoder = new ModuleFIT();
    break;
  default:
    decoder = NULL;
    break;
  }

  if(gsidx < 0){
    return -1;
  }else{
    return 0;
  }
}

//data[4] : geo, ch, edge, value
//return  : 0=normal, 1=no decorder (return 32bit value), -1=end of segment
int RIDFParser::nextdata(int segid, int data[4]){
  int ival, ret;
  data[0] = 0;
  data[1] = 0;
  data[2] = 0;
  ret = 0;

  if(gnsidx <= gsidx + 12){
    return -1;
  }

  if(decoder == NULL){
    memcpy((char *)&ival, gbuff+gsidx+12, 4);
    data[3] = ival;
    gsidx += 4;
  }else{
    ret = decoder->decode(gbuff+gsidx+12, gssz, data);
  }

  return ret;
}
