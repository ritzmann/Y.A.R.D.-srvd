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
#include <stdio.h>
#include <string.h>

#include "globaldef.h"
#include "settings.h"


/*** DEFINES ***************************************************************/
#define DEFAULT_SOCK		"/tmp/yardsrv_sock"
#define CONFIG_FILE		"/usr/local/etc/yardsrvd.conf"
#define MAX_KEY_LEN		10
#define MAX_VAL_LEN		30
#define KEY_SOCK			"SOCK"
#define KEY_PORT			"PORT"
#define KEY_BAUD			"BAUD"
#define KEY_TASK			"TASK"
#define KEY_TIME			"TIME"
#define KEY_PERIOD		"PERIOD"


/*** VARIABLES *************************************************************/
static YARDSRV_CONFIG tConfig = { "\0", "\0", 0, "\0", {0, 0}, 0 };


/*** FUNCTIONS *************************************************************/
BOOL settings_oParseCfgFile(VOID)
{
	FILE *ptCfgFile;
	CHAR acLineBuf[MAX_KEY_LEN + MAX_VAL_LEN + 2];
	CHAR *pcKey, *pcVal;
	INT nHour, nMin;
	BOOL oRetVal = TRUE;
	
	
	// Open config file
	ptCfgFile = fopen(CONFIG_FILE, "r");
	if (ptCfgFile == NULL)
	{
		oRetVal = FALSE;
	}
	else
	{
		// Read all keys
		while ( fgets(acLineBuf, sizeof(acLineBuf), ptCfgFile) != NULL)
		{
			if (acLineBuf[0] != '#')
			{
				pcKey = strtok(acLineBuf, "=");
				pcVal = strtok(NULL, "\n");
				
				if ( !strcmp(pcKey, KEY_SOCK) )
				{
					strcpy(tConfig.acSock, pcVal);
				}
				else if ( !strcmp(pcKey, KEY_PORT) )
				{
					strcpy(tConfig.acPort, pcVal);
				}
				else if ( !strcmp(pcKey, KEY_BAUD) )
				{
					tConfig.dwBaud = atoi(pcVal);
				}
				else if ( !strcmp(pcKey, KEY_TASK) )
				{
					strcpy(tConfig.acTask, pcVal);
				}
				else if ( !strcmp(pcKey, KEY_TIME) )
				{
					if (sscanf(pcVal, "%u:%u", &nHour, &nMin ) != 2)
					{
						oRetVal = FALSE;
						break;
					}
					else
					{
						tConfig.tTasktime.bHour = (BYTE)nHour;
						tConfig.tTasktime.bMin = (BYTE)nMin;
					}
				}
				else if ( !strcmp(pcKey, KEY_PERIOD) )
				{
					tConfig.wPeriod = atoi(pcVal);
				}
			}
		}
		
		// Use default sock if no key available
		if ( !strlen(tConfig.acSock) )
		{
			strcpy(tConfig.acSock, DEFAULT_SOCK);
		}
		
		// Error check
		if ( !strlen(tConfig.acPort) )
		{
			oRetVal = FALSE;
		}
		if (tConfig.dwBaud == 0)
		{
			oRetVal = FALSE;
		}
		if ( strlen(tConfig.acTask) && (tConfig.wPeriod == 0) )
		{
			oRetVal = FALSE;
		}
	}
	
	// Close config file
	fclose(ptCfgFile);
	
	return oRetVal;
}

YARDSRV_CONFIG * settings_ptGetCfg(VOID)
{
	return &(tConfig);
}