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
#ifndef _SETTINGS_H__
#define _SETTINGS_H__


/*** TYPEDEFINITIONS *******************************************************/
typedef struct
{
	CHAR acSock[MAX_PATH_LEN];
	CHAR acPort[MAX_PATH_LEN];
	DWORD dwBaud;
	CHAR acTask[MAX_PATH_LEN];
	DAY_TIME tTasktime;
	WORD wPeriod;
} YARDSRV_CONFIG;


/*** PROTOTYPES ************************************************************/
BOOL settings_oParseCfgFile(VOID);
YARDSRV_CONFIG * settings_ptGetCfg(VOID);


#endif