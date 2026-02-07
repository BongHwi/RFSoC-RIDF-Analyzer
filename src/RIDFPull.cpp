#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <netinet/in.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <iostream>

#include "RIDFPull.h"
#include "ridf.h"

#define INFCOMPORT  17516   ///< babinfo communication port
#define EB_EFBLOCK_SIZE 0x200000   ///< Usual max size of block data = 4MB (w
#define EB_EFBLOCK_BUFFSIZE EB_EFBLOCK_SIZE * 2  ///< Usual size of block data 
#define INF_GET_RAWDATA    10
#define INF_GET_BLOCKNUM   11

ClassImp(RIDFPull);

RIDFPull::RIDFPull(std::string host){
  //if((data = (char *)malloc(EB_EFBLOCK_BUFFSIZE * 2)) == NULL){
  //printf("too large buffer \n");
  //}

  strncpy(ebhostname, host.c_str(), sizeof(ebhostname)-1);
};

RIDFPull::~RIDFPull(){
  //  delete data;
};

int RIDFPull::mktcpsend(char *host, unsigned short port){
  int tsock = 0;
  struct hostent *thp;
  struct sockaddr_in tsaddr;

  if((tsock = socket(AF_INET,SOCK_STREAM,0)) < 0){
    perror("bi-tcp.mktcpsend: Can't make socket.\n");
    return 0;
  }

  memset((char *)&tsaddr,0,sizeof(tsaddr));

  if((thp = gethostbyname(host)) == NULL){
    printf("bi-tcp.mktcpsend : No such host (%s)\n", host);
    return 0;
  }

  memcpy(&tsaddr.sin_addr,thp->h_addr,thp->h_length);
  tsaddr.sin_family = AF_INET;
  tsaddr.sin_port = htons(port);

  if(connect(tsock,(struct sockaddr *)&tsaddr,sizeof(tsaddr)) < 0){
    perror("bi-tcp.mktcpsend: Error in tcp connect.\n");
    close(tsock);
    return 0;
  }

  return tsock;
}

int RIDFPull::eb_get(int sock, int com, char *dest){
  int len;

  len = sizeof(com);
  send(sock, (char *)&len, sizeof(len), 0);
  send(sock, (char *)&com, len, 0);

  recv(sock, (char *)&len, sizeof(len), MSG_WAITALL);
  recv(sock, dest, len, MSG_WAITALL);

  return len;
}

int RIDFPull::infcon(char *host){
  int infsock;

  /* Connect to babild */
  if(!(infsock = mktcpsend(host, INFCOMPORT))){
    printf("Can't connect to babinfo.\n");
    return 0;  // 연결 실패 시 0 반환 (exit(0) 제거)
  }

  return infsock;
}


// 0  : no new data / or no valid data
// sz : data size
// -1 : error
int RIDFPull::pull(char *data){
  int tblkn = 0;
  int thd, cid = 0;
  int ret = 0;
  int size;

  sock = 0;

  if(!data){
    printf("data buffer is not malloced\n");
    return -1;
  }
    
  /* List Data Socket */
  if((sock = infcon(ebhostname))){
    eb_get(sock, INF_GET_RAWDATA, data);
    close(sock);
  }else{
    printf("Can not connect %s\n", ebhostname);
    return -1;
  }

  memcpy((char *)&size, data, sizeof(size));
  size = size & 0x003fffff;
  
  memcpy((char *)&thd, data+8, sizeof(thd));
  cid = RIDF_CI(thd);

  if(cid == 8){
    memcpy((char *)&tblkn, data+16, sizeof(tblkn));
    if(tblkn != blkn){
      blkn = tblkn;
      ret = size * 2;
    }else{
      ret = 0;
    }
  }else{
    ret = 0;
  }

  return ret;
}

