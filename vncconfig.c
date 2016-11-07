/*基本公共头文件*/
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <linux/kd.h>
#include <strings.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/dir.h>
#include <termios.h>
/*用于声音编程*/
#include <linux/soundcard.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <sys/wait.h>
#include <iconv.h>
#include <jpeglib.h>
 
#include <time.h>
#include <string.h>
#include <sys/timeb.h>
#include <stdlib.h>
/*@-skipposixheaders@*/
#include <sys/types.h>
#include <sys/stat.h>
#include <malloc.h>
#include <stdarg.h>
#include <ctype.h> 
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>

#include "vncconfig.h"
#define  V3CONFIG  "/EMRCV3/CONFIG/Config.ini"
#define  V4CONFIG  "/EMRCV4/CONFIG/Config.ini"
#define  V5CONFIG  "/EMRCV5/CONFIG/Config.ini"

char read_ini_String[MAXSYSINI];

char NetPlazaNo[5];
char _LaneNo[4];
char LaneType;
char _TitleInfo[200];
void VNCCONFIGINIT(void)
{
	struct stat sb1;
	char clienttype[20];
	memset(read_ini_String, 0x00, MAXSYSINI);
	memset(clienttype, 0x00, 20);
	if (stat(V5CONFIG, &sb1) == 0)
	{
		VCNCLoadFile(V5CONFIG, read_ini_String, MAXSYSINI);
		  strcat(clienttype, "V5");
	}
	else if (stat(V4CONFIG, &sb1) == 0)
	{
		VCNCLoadFile(V4CONFIG, read_ini_String, MAXSYSINI);
		strcat(clienttype, "V4");
	}
	else if (stat(V5CONFIG, &sb1) == 0)
	{
		VCNCLoadFile(V5CONFIG, read_ini_String, MAXSYSINI);
		strcat(clienttype, "V3");
	}
	else
	{
		strcat(clienttype, "Unknow");
	}
	if (strlen(read_ini_String) > 0)
	{
		memset(NetPlazaNo, 0x00, sizeof(NetPlazaNo));
		memset(_LaneNo, 0x00, sizeof(_LaneNo));
		memset(_TitleInfo, 0x00, sizeof(_TitleInfo));

		strncpy(NetPlazaNo, vncreadItemInIniFile("NetNo"), 2);
		strncpy((char*)&NetPlazaNo[2], vncreadItemInIniFile("PlazaNo"), 2);
		strncpy(_LaneNo, vncreadItemInIniFile("LaneNo"), 3);
		LaneType = vncreadItemInIniFile("LaneType")[0];
		sprintf(_TitleInfo, "RHYVNC EMRC%s Plaza:%s LaneNO:%s  LaneType:%c",clienttype, NetPlazaNo, _LaneNo, LaneType);
	}
	else
	{
		sprintf(_TitleInfo, "RHYVNC Unknow Client ,Not found config.ini");
	}
	fprintf(stderr, "*%s\r\n", "*************************************************");
	fprintf(stderr, "* %s\r\n", _TitleInfo);
	fprintf(stderr, "* %s\r\n", "2016/01/29 by  mayanhong");
	fprintf(stderr, "*%s\r\n", "**************************************************");
}
char * GetTitleInfo(void)
{
	return _TitleInfo;
}
int  VCNCLoadFile(char *InputFileName, char *LoadInFileStr, int FilrStrLength)
{
	int LoadFinleFd;
	LoadFinleFd = open(InputFileName, O_RDONLY);
	if (LoadFinleFd<0)
	{
		printf("opern %s error \n", InputFileName);
		return -1;
	}
	lseek(LoadFinleFd, SEEK_SET, 0);
	if (read(LoadFinleFd, LoadInFileStr, FilrStrLength)<0)
	{
		printf("read %s error \n", InputFileName);
		close(LoadFinleFd);
		return -1;
	}
	close(LoadFinleFd);
	return 1;
}


/****************************************************************************************
* 在装载的Ini字符串中读取指定内容的值
*****************************************************************************************/
char ItemValue[100];
char  *vncreadItemInIniFile(char *ItemName)
{
	char *ini_addr_ptr;
	int len;
	memset(ItemValue, 0x00, sizeof(ItemValue));
	ini_addr_ptr = strstr(read_ini_String, ItemName);
	if (ini_addr_ptr == NULL)
	{
		ItemValue[0] = '0';
		ItemValue[1] = 0x00;
	 
		return ItemValue;
	}
	ini_addr_ptr = strchr(ini_addr_ptr, '<') + 1;
	len = strchr(ini_addr_ptr, '>') - ini_addr_ptr;
	strncpy(ItemValue, ini_addr_ptr, len);
	ItemValue[len] = 0x00;
	return ItemValue;
}