/*
 * hasnextevent check /dev/ptx0.t0 21234 4384 2012-07-15T11:30:00
 *
 * 引数１・check(固定)
 * 引数２・デバイス名
 * 引数３・EITサービスID
 * 引数４・EITイベントID
 * 引数５・EITイベント開始時刻
 *
 * デバイスからTSを読み出し、引数で渡されたサービスIDのイベントIDが
 * EITの次のイベントにあるかチェックします。
 * 次のイベントにある場合０で終了します。
 * 次のイベントにない場合１で終了します。
 * EIT情報が取得できない場合等含めて最長約１０秒で終了します。
 * 第５引数とイベント開始時刻差を秒数で標準出力に出力します。
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <iconv.h>
#include <time.h>
#include <signal.h>

#include "ts.h"
#include "sdt.h"
#include "eit.h"
#include "nit.h"
#include "tdt.h"
#include "bit.h"
#include "ts_ctl.h"

#define	SECCOUNT	10

SVT_CONTROL	*svttop = NULL;
static	EITCHECK	chk;

int CheckEIT(FILE *infile,SECcache *secs,int count,EITCHECK *echk)
{
  SVT_CONTROL	*svtcur ;
  int		pid,ret,sdtflg;
  SECcache      *bsecs;

  svttop = calloc(1, sizeof(SVT_CONTROL));
  sdtflg = 0;

  while((bsecs = readTS(infile, secs, count)) != NULL) {
    pid = bsecs->pid & 0xFF;
    switch (pid) {
    case 0x11: //SDT
      if (sdtflg==0) {
	sdtflg=1;
	dumpSDT(bsecs->buf, svttop);
	svtcur = svttop->next;
	while(svtcur) {
	  if (svtcur->eit == NULL) {
	    svtcur->eit = calloc(1, sizeof(EIT_CONTROL));
	  }
	  svtcur = svtcur->next;
	}
      }
      break;
    case 0x12: // EIT
      ret = dumpEIT2(bsecs->buf,svttop,echk);
      if (ret == EIT_CHECKOK || ret == EIT_CHECKNG) { //CHECK COMPLETE
	return ret - EIT_CHECKOK;
      }
      if (ret == EIT_SDTNOTFOUND) sdtflg=0;
      break;
    case 0x14: // TDT
      dumpTDT(bsecs->buf,echk);
      break;
    case 0x13: // RST
      printf("RST\n");
      break;
    }
    if (echk->waitend < time(NULL)) {return 1;}
  }
  return 1; // EOF
}

void usage_exit()
{
  fprintf(stdout, "Usage : ");
  exit(0);
}

int main(int argc, char argv[])
{
  FILE *infile;
  SECcache   secs[SECCOUNT];
  int ret;

  /* 興味のあるpidを指定 */
  memset(secs, 0,  sizeof(SECcache) * SECCOUNT);
  secs[0].pid = 0x11; /* SDT */
  secs[1].pid = 0x12; /* EIT */
  secs[2].pid = 0x14; /* TDT */
  secs[3].pid = 0x23; /* SDTT */
  secs[4].pid = 0x28; /* SDTT */
  secs[5].pid = 0x10; /* NIT */
  secs[6].pid = 0x13; /* RST */
  secs[7].pid = 0x24; /* BIT */

  if( argc != 5 ) {
    usage_exit();
  }

  memset(&chk,0,sizeof(EITCHECK));
  chk.svid = atoi(argv[2]);
  chk.evid = atoi(argv[3]);
  chk.starttime = str2timet(argv[4]);
  chk.waitend = time(NULL) + 11;

  infile = fopen(argv[1], "r");
  ret = CheckEIT(infile,secs, SECCOUNT,&chk);
  // 0..success 1..failed

  fclose(infile);
  return ret;
}
