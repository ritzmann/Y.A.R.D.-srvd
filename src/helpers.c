/***************************************************************************
 *   Copyright (C) 2009 by M. Feser   *
 *      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

/*** INCLUDES **************************************************************/
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#include "globaldef.h"
#include "helpers.h"


/*** FUNCTIONS *************************************************************/
BOOL helpers_oMonitorTimeoutUs(struct timeval *ptStartTime, DWORD dwTimeoutUs)
{
	struct timeval tCurTime;
	LONG lTimeDiffSec, lTimeDiffUsec;
	
	
	// Calc elapsed time
	gettimeofday(&tCurTime, NULL);
	lTimeDiffSec = (tCurTime.tv_sec - ptStartTime->tv_sec);
	lTimeDiffUsec = lTimeDiffSec*1000000 + (tCurTime.tv_usec - ptStartTime->tv_usec);
	
	// Check for timeout
	if (lTimeDiffUsec >= dwTimeoutUs)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

DWORD helpers_dwGetCurTime(VOID)
{
	DWORD dwTime;
	time_t tTime;
	struct tm *ptLocalTm;
	
	
	// Get time and adjust UTC to local time
	time(&tTime);
	ptLocalTm = localtime(&tTime);

	// Adjust UTC to local time
	dwTime = (LONG)tTime + ptLocalTm->tm_gmtoff;
	
	// Adjust time to 01.01.2005
	return (dwTime - 1104537600);
}

DWORD helpers_dwGetAdjTime(DAY_TIME tDaytime, DWORD dwSecOff, CHAR *pcTimeStr)
{
	CHAR *pcTmpStr;
	DWORD dwTime;
	time_t tTime;
	struct tm *ptLocalTm;
	
	
	// Get time, and add offset
	time(&tTime);
	ptLocalTm = localtime(&tTime);
	dwTime = (LONG)tTime + dwSecOff;
	
	// Adjust to local daytime
	ptLocalTm = localtime(&dwTime);
	ptLocalTm->tm_sec = 0;
	ptLocalTm->tm_min = tDaytime.bMin;
	ptLocalTm->tm_hour = tDaytime.bHour;
	dwTime = (LONG)mktime(ptLocalTm);
	pcTmpStr = asctime(ptLocalTm);
	strcpy(pcTimeStr, pcTmpStr);
	dwTime += ptLocalTm->tm_gmtoff;
	
	// Adjust time to 01.01.2005
	return (dwTime - 1104537600);
}