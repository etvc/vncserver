#ifndef __VNCCONFIG__
#define __VNCCONFIG__
#define MAXSYSINI 4096
void VNCCONFIGINIT(void);
char * GetTitleInfo(void);
int  VCNCLoadFile(char *InputFileName, char *LoadInFileStr, int FilrStrLength);
char * vncreadItemInIniFile(char * ItemName);
#endif