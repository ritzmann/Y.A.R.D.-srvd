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
#ifndef _YARDSER_H__
#define _YARDSER_H__


/*** INCLUDES **************************************************************/
#include "yarddef.h"


/*** PROTOTYPES ************************************************************/
VOID yardserial_vOpen(CHAR *pcDevPath, DWORD dwYardBd, FUNC_PTR pvExitCallback);
VOID yardserial_vClose(VOID);
BOOL yardserial_oUpdateCircBuf(VOID);
BOOL yardserial_oProcessCircBuf(YARD_RES *ptYardRes);
BOOL yardserial_oSendCmd(YARD_CMD *ptYardCmd, BOOL oSByte);
BOOL yardserial_oSendCmdBlocking(YARD_CMD *ptYardCmd, BOOL oSByte, YARD_RES *ptYardRes, DWORD dwTimeout);


#endif