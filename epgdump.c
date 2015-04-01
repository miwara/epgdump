#include "config.h"

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

#include "xmldata.c"


SVT_CONTROL	*svttop = NULL;
#define		SECCOUNT	10
char	title[1024];
char	subtitle[1024];
char	Category[1024];
char	*extdesc;
char	ServiceName[1024];

static	EITCHECK	chk;


/*
 * デバック用
*/
void signalhandler()
{
SVT_CONTROL *svtcur;
EIT_CONTROL *eitcur;
int cnt,extcnt,rest;
	svtcur=svttop->next;
	while(svtcur != NULL) {
		if (!svtcur->haveeitschedule) {
			svtcur = svtcur->next;
			continue;
		}
		cnt=extcnt=rest=0;
		eitcur = svtcur->eit;
		while(eitcur != NULL){
			if(!eitcur->servid){
				eitcur = eitcur->next ;
				continue ;
			}
			cnt++;
			if (eitcur->eitextcnt>0) {
				extcnt++;
			}
			eitcur = eitcur->next;
		}
		if (chk.starttime > 0 && chk.maxcycle > 0) {
			rest = chk.starttime + chk.maxcycle - chk.tdttime;
		}
		fprintf(stderr,"sid %d programs %d(ext %d) rest %d(%d)\n",svtcur->event_id,cnt,extcnt,rest,chk.maxcycle);
		svtcur = svtcur->next;
	}
}


/*
 * 引数で"check"と"wait"を指定したときのみ呼ばれる
 * 具体的な部分は分からない(readmeも意味不明)
 *
*/
int     CheckEIT(FILE *infile,SECcache *secs,int count,EITCHECK *echk)
{
	SVT_CONTROL	*svtcur ;
	int 		pid,ret,sdtflg;
	SECcache  *bsecs;

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
int	GetSDTEITInfo(FILE *infile,SECcache *secs,int count)
{
	SVT_CONTROL	*svtcur ;
	int 		pid;
	SECcache	*bsecs;
	int		sdtflg,numsdt;
	int		ret;

	memset(&chk,0,sizeof(EITCHECK));
	sdtflg=numsdt=0;

	chk.waitend = time(NULL) + 10;

#ifdef DEBUGSIGNAL
	signal(SIGINFO,signalhandler);
#endif

/*
XXX BITが取れないときはどうするか？
*/

	while((bsecs = readTS(infile, secs, count)) != NULL) {
		pid = bsecs->pid & 0xFF;
		switch (pid) {
			case 0x10:  //NIT
				dumpNIT(bsecs->buf,svttop);
				break;

			case 0x11: // SDT
				if (sdtflg==0) { 
					sdtflg=1;
					dumpSDT(bsecs->buf, svttop);

					numsdt=0;
					svtcur = svttop->next;
					while(svtcur) {
						if (svtcur->eit == NULL) {
							svtcur->eit = calloc(1, sizeof(EIT_CONTROL));
						}
						svtcur = svtcur->next;
						numsdt++;
					}

				}
				break;
			case 0x12: // EIT
			case 0x26: // EIT(地デジ)
			case 0x27: // EIT(地デジ)
				if (sdtflg) {
					ret = dumpEIT2(bsecs->buf,svttop,NULL);
					if (ret == 0) sdtflg = 0;
				}
				break;
			case 0x14: // TDT
				dumpTDT(bsecs->buf,&chk);
				if (chk.starttime > 0  && chk.maxcycle > 0) {
					if (chk.starttime + chk.maxcycle < chk.tdttime) {
#ifdef DEBUG
printf("start %s cycle %d\n",strTime(chk.starttime,"%Y/%m/%d %H:%M:%S"),chk.maxcycle);
printf("tdt %s\n",strTime(chk.tdttime,"%Y/%m/%d %H:%M:%S"));
#endif
						return 0;
					}
				}
				break;
			case 0x23: // SDTT
			//	ret = dumpSDTT(bsecs->buf);
				break;
			case 0x13: // RST
				printf("RST\n");
				break;
			case 0x24: // BIT
				if (chk.maxcycle == 0 && chk.tdttime > 0) {
					dumpBIT(bsecs->buf,&chk.maxcycle);
					chk.starttime = chk.tdttime;
                                        /* XXX 環境変数等でオーバーライド可能にしたい
                                         *     周期どおりで基本番組情報は十分だが、拡張のほうは
                                         *     なかなか埋まらない
                                         */
					chk.maxcycle = chk.maxcycle * 1.6;
				}
				break;
		}
		if (numsdt == 0 && time(NULL) > chk.waitend) return 1;
	}
	return 0;
}

/*
 * csvcを指定したときのみ呼ばれる
 * チャンネル一覧をcsvで出力する
*/
void	dumpChannel(FILE *outfile)
{
	SVT_CONTROL	*svtcur ;
	char *p;
	svtcur=svttop->next;
	while(svtcur != NULL) {
		if (!svtcur->haveeitschedule) {
			svtcur = svtcur->next;
			continue;
		}
		fprintf(outfile,"%d,", svtcur->event_id);
		p = getBSCSGR(svtcur);
		switch (p[0]) {
			case 'G':
				fprintf(outfile,"%s,",p);
				break;
			case 'B':
				fprintf(outfile,"%s%d_%d,",p,
					getTSID2TP(svtcur->transport_stream_id),
					getTSID2SLOT(svtcur->transport_stream_id));
				break;
			case 'C':
				fprintf(outfile,"%s%d,",p,
					getTSID2TP(svtcur->transport_stream_id));
				break;
		}
		fprintf(outfile,"%s\n",svtcur->servicename);
		svtcur = svtcur->next;
	}
}

/*
 * jsonを指定したときのみ呼ばれる
 * json形式で出力する
*/
void dumpJSON(FILE *outfile)
{
	SVT_CONTROL	*svtcur ;
	EIT_CONTROL	*eitcur ;
	int i;
	char *svtcanma="";
	char *eitcanma;
	char *eitextcanma;

	fprintf(outfile,"[");
	svtcur=svttop->next;
	while(svtcur != NULL) {
		if (!svtcur->haveeitschedule) {
			svtcur = svtcur->next;
			continue;
		}
		fprintf(outfile,"%s{",svtcanma);
		fprintf(outfile,"\"id\":\"%s_%d\",",getBSCSGR(svtcur),svtcur->event_id);
		fprintf(outfile,"\"transport_stream_id\":%d,",svtcur->transport_stream_id);
		fprintf(outfile,"\"original_network_id\":%d,",svtcur->original_network_id);
		fprintf(outfile,"\"service_id\":%d,",svtcur->event_id);
		fprintf(outfile,"\"name\":\"%s\",",svtcur->servicename);
		if (svtcur->original_network_id < 0x0010) {
			fprintf(outfile,"\"satelliteinfo\":{");
			fprintf(outfile,"\"TP\":\"%s%d\",",getTSID2BSCS(svtcur->transport_stream_id),getTSID2TP(svtcur->transport_stream_id));
			fprintf(outfile,"\"SLOT\":%d},",getTSID2SLOT(svtcur->transport_stream_id));
		}
		eitcur = svtcur->eit;
		fprintf(outfile,"\"programs\":[");
		eitcanma="";
		while(eitcur != NULL){
			if(!eitcur->servid){
				eitcur = eitcur->next ;
				continue ;
			}
			fprintf(outfile,"%s{",eitcanma);
			fprintf(outfile,"\"channel\":\"%s_%d\",",getBSCSGR(svtcur),svtcur->event_id);
			memset(title, '\0', sizeof(title));
			if (eitcur->title) strcpy(title, eitcur->title);
			strrep(title, "\"", "\\\"");
			fprintf(outfile,"\"title\":\"%s\",",title);
			memset(subtitle, '\0', sizeof(subtitle));
			if (eitcur->subtitle) strcpy(subtitle, eitcur->subtitle);
			strrep(subtitle, "\"", "\\\"");
			fprintf(outfile,"\"detail\":\"%s\",",subtitle);

			fprintf(outfile,"\"extdetail\":[");
			eitextcanma="";
			for(i=0;i<eitcur->eitextcnt;i++) {
				if (eitcur->eitextdesc[i].item_description && eitcur->eitextdesc[i].item) {
					strcpy(subtitle, eitcur->eitextdesc[i].item_description);
					strrep(subtitle, "\"", "\\\"");
					fprintf(outfile,"%s{\"item_description\":\"%s\",",eitextcanma,subtitle);
			        memset(subtitle, '\0', sizeof(subtitle));
					strcpy(subtitle, eitcur->eitextdesc[i].item);
					strrep(subtitle, "\"", "\\\"");
					fprintf(outfile,"\"item\":\"%s\"}",subtitle);
					eitextcanma=",";
				}
			}
			fprintf(outfile,"],");

			fprintf(outfile,"\"start\":%d0000,",eitcur->start_time);
			fprintf(outfile,"\"end\":%d0000,",eitcur->start_time+eitcur->duration);
			fprintf(outfile,"\"duration\":%d,",eitcur->duration);

			eitextcanma="";
			fprintf(outfile,"\"category\":[");
			for(i=0;i<eitcur->numcontent;i++) {
				fprintf(outfile,"%s{",eitextcanma);
				fprintf(outfile,"\"large\": { \"ja_JP\" : \"%s\", \"en\" : \"%s\"},",getContentStr(eitcur->content[i],eitcur->usernibble[i],CONTENT_LARGE,CONTENT_LANG_JA),getContentStr(eitcur->content[i],eitcur->usernibble[i],CONTENT_LARGE,CONTENT_LANG_EN));
				fprintf(outfile,"\"middle\": { \"ja_JP\" : \"%s\", \"en\" : \"%s\"}}",getContentStr(eitcur->content[i],eitcur->usernibble[i],CONTENT_MIDDLE,CONTENT_LANG_JA),getContentStr(eitcur->content[i],eitcur->usernibble[i],CONTENT_MIDDLE,CONTENT_LANG_EN));
				eitextcanma=",";
			}
			fprintf(outfile,"],");

			eitextcanma="";
			fprintf(outfile,"\"attachinfo\":[");
			for(i=0;i<eitcur->numattachinfo;i++) {
				fprintf(outfile,"%s\"%s\"",eitextcanma,getAttachInfo(eitcur->attachinfo[i]));
				eitextcanma=",";
			}
			fprintf(outfile,"],");

			fprintf(outfile,"\"video\":{");
			fprintf(outfile,"\"resolution\":\"%s\",",getVideoResolution(eitcur->video));
			fprintf(outfile,"\"aspect\":\"%s\"},",getVideoAspect(eitcur->video));

			eitextcanma="";
			fprintf(outfile,"\"audio\":[");
			for(i=0;i<2;i++) {
			if (eitcur->audiodesc[i].audiotype > 0) {
					fprintf(outfile,"%s{\"type\":\"%s\",",eitextcanma,getAudioComponentDescStr(eitcur->audiodesc[i].audiotype));
					fprintf(outfile,"\"langcode\":\"%s\",", eitcur->audiodesc[i].langcode);
					fprintf(outfile,"\"extdesc\":\"%s\"}", eitcur->audiodesc[i].audiodesc?eitcur->audiodesc[i].audiodesc:"");
					eitextcanma=",";
				}
			}
			fprintf(outfile,"],");

			fprintf(outfile,"\"freeCA\":%s,",eitcur->freeCA?"true":"false");
			fprintf(outfile,"\"event_id\":%d",eitcur->event_id);
			eitcur = eitcur->next ;
			fprintf(outfile,"}");
			eitcanma=",";
		}
		fprintf(outfile,"]");
		svtcur  = svtcur->next;
		fprintf(outfile,"}");
		svtcanma=",";
	}
	fprintf(outfile,"]");
}

void usage_exit()
{
  fprintf(stdout, "Usage : epgdump [th] <tsFile> <outfile>\n");
  fprintf(stdout, "-t\t output file format csv\n");
  exit(0);
}

int main(int argc, char *argv[])
{
	FILE *infile;
	FILE *outfile = stdout;
	char *file,*fileout;
	int   inclose = 0;
	int   outclose = 0;
	int	ret;
	SECcache   secs[SECCOUNT];


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

	if(argc < 3){
	  usage_exit();
	}

	int opt;
	char *type = "json"; /* デフォルトの出力ファイルフォーマットはjson */
	while((opt = getopt(argc, argv, "th")) != -1){
	  switch (opt){
	  case 't':
	    type = "csvc";
	    break;

	  case 'h':
	    usage_exit();
	    break;

	  default:
	    usage_exit();
	    break;
	  }
	}

 	file = argv[optind];
 	fileout= argv[optind+1];
	if((infile = fopen(file, "r")) == NULL) {
	  fprintf(stderr, "%s: file does not exist\n", file);
	  return 1;
	}
	inclose = 1;
	if(strcmp(fileout, "-")) {
	  outfile = fopen(fileout, "w+");
	  outclose = 1;
	}


/* 使わないので一旦コメントアウト
 * 別のプログラムに分割した方がいい
	if(argc == 6 && ((strcmp(argv[1], "check") == 0)||(strcmp(argv[1],"wait"))==0)){
		memset(&chk,0,sizeof(EITCHECK));
		chk.svid = atoi(argv[3]);
		chk.evid = atoi(argv[4]);
		if (strcmp(argv[1],"check")==0) {
			chk.starttime = str2timet(argv[5]);
			chk.waitend = time(NULL) + 11;
		}
		else {
			chk.waitend = time(NULL) + atoi(argv[5]);
		}
		ret = CheckEIT(infile,secs, SECCOUNT,&chk);
		if (inclose) fclose(infile);
		// 0..ok 1..fail
		return ret;
	}
*/

	svttop = calloc(1, sizeof(SVT_CONTROL));

	ret = GetSDTEITInfo(infile, secs, SECCOUNT);


	if (strcmp(type, "csvc") == 0){
		dumpChannel(outfile);
	} else {
		dumpJSON(outfile);
	}

	if(inclose) fclose(infile);
	if(outclose) fclose(outfile);

	return ret;
}
