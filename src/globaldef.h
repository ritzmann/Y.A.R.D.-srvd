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
#ifndef _GLOBDEF_H__
#define _GLOBDEF_H__


/*** DEFINES ***************************************************************/
#define MAX_PATH_LEN		100
#define MAX_LOGMSG_LEN		100
#define PERM_MASK			0666

/*** TYPEDEFINITIONS *******************************************************/
typedef unsigned char	BYTE;
typedef unsigned short	WORD;
typedef unsigned long	DWORD;

typedef signed char		CHAR;
typedef signed short	SHORT;
typedef signed long		LONG;
typedef signed int		INT;

typedef void			VOID;
typedef VOID (* FUNC_PTR)(VOID);

typedef enum
{
	FALSE = 0,
 	TRUE
} BOOL;

typedef struct
{
	BYTE bHour;
	BYTE bMin;
} DAY_TIME;


#endif