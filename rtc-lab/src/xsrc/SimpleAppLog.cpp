
#include "SimpleAppLog.h"
#include <time.h>


void _print_time()
{
	
	struct tm *sTm;

	time_t now = time (0);
	//sTm = gmtime (&now);
	sTm = localtime (&now);
	
	//char buff[32];
	//strftime (buff, sizeof(buff), "%Y-%m-%d %H:%M:%S ", sTm);
	//strftime (buff, sizeof(buff), "%m-%d %H:%M:%S", sTm);
	//printf ("%s ", buff);
	printf("%02d-%02d %02d:%02d:%02d "
		, sTm->tm_mon+1, sTm->tm_mday, sTm->tm_hour, sTm->tm_min, sTm->tm_sec);
}

#ifdef WIN32
#include <windows.h>
void _print_time_ms()
{
	SYSTEMTIME sys;

	GetLocalTime( &sys );
	//printf( "%4d/%02d/%02d %02d:%02d:%02d.%03d ÐÇÆÚ%1d\n",
	//	sys.wYear,sys.wMonth,sys.wDay,sys.wHour,sys.wMinute, sys.wSecond,sys.wMilliseconds,sys.wDayOfWeek);
	printf("%02d-%02d %02d:%02d:%02d.%03d ", sys.wMonth,sys.wDay,sys.wHour,sys.wMinute, sys.wSecond,sys.wMilliseconds);
}

#else

#include <sys/time.h>
void _print_time_ms()
{
	struct tm *sTm;

	time_t now = time (0);
	//sTm = gmtime (&now);
	sTm = localtime (&now);

	//char buff[32];
	//strftime (buff, sizeof(buff), "%Y-%m-%d %H:%M:%S ", sTm);
	//strftime (buff, sizeof(buff), "%m-%d %H:%M:%S", sTm);
	//printf ("%s.%03d ", buff, time_stamp.tv_usec / 1000);

	struct timeval time_stamp;
	gettimeofday(&(time_stamp), NULL);
	printf("%02d-%02d %02d:%02d:%02d.%03d "
		, sTm->tm_mon+1, sTm->tm_mday, sTm->tm_hour, sTm->tm_min, sTm->tm_sec, time_stamp.tv_usec / 1000);
	
}

#endif


void SimplePrintTime()
{
	_print_time_ms();
}


void SimpleLogPrintBuf(const char * prefix, const void * _buf, plw_u32 buflen)
{
	char printBuf[512];
	int len = 0;
	const plw_u8 * buf = (const plw_u8 *)_buf;
	
	if(!prefix) prefix = "";

	len += sprintf(printBuf+len, "%s %04X: ", prefix, 0);
	for(plw_u32 i = 0; i < buflen; i++)
	{
		len += sprintf(printBuf+len, "%02X", buf[i]);
		if((i+1) % 16 ==0) 
		{
			sdbgi("%s\n", printBuf);
			len = 0;
			len += sprintf(printBuf+len, "%s %04X: ", prefix, i+1);
		}
	}
	sdbgi("%s\n", printBuf);
}


// static plw_task_t g_flush_task = 0;
// static plw_bool g_flush_stopreq = PLW_FALSE;
// static plw_bool g_flush_exited= PLW_FALSE;
// static plw_u32 g_flush_interval_ms = 1000;
// static void flush_thread(void * param)
// {
// 	plw_u32 loop_interval = g_flush_interval_ms;
// 	plw_u32 counter = 0;
// 	if(loop_interval > 1000) loop_interval = 1000;
// 	while(!g_flush_stopreq)
// 	{
// 		plw_delay(loop_interval);
// 		counter += loop_interval;
// 		if(counter >= g_flush_interval_ms)
// 		{
// 			counter = 0;
// 			fflush(stdout);
// 		}
		
// 	}
// 	g_flush_exited = PLW_TRUE;
// }

// void SimpleLogStartFlush(plw_u32 inerval_ms)
// {
// 	if(g_flush_task) return;
// 	if(inerval_ms == 0) inerval_ms = 1000;
// 	g_flush_interval_ms = inerval_ms;
// 	plw_task_create(flush_thread, NULL, 0, PLW_PRIOR_NORMAL, &g_flush_task);

// }

// void SimpleLogStopFlush()
// {
// 	if(!g_flush_task) return;
// 	g_flush_stopreq = PLW_TRUE;
// 	while(!g_flush_exited)
// 	{
// 		plw_delay(100);
// 	}
// 	plw_delay(100);
// 	plw_task_delete(g_flush_task);
// 	g_flush_task = 0;
// }
