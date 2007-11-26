/**
 * sigtran.cpp
 * This file is part of the YATE Project http://YATE.null.ro 
 *
 * Yet Another Signalling Stack - implements the support for SS7, ISDN and PSTN
 *
 * Yet Another Telephony Engine - a fully featured software PBX and IVR
 * Copyright (C) 2004-2006 Null Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "yatesig.h"


using namespace TelEngine;

SIGTRAN::SIGTRAN()
    : m_trans(None), m_socket(0)
{
}

SIGTRAN::~SIGTRAN()
{
    terminate();
}

void SIGTRAN::terminate()
{
    Socket* tmp = m_socket;
    m_trans = None;
    m_socket = 0;
    m_part.clear();
    delete tmp;
}

bool SIGTRAN::attach(Socket* socket, Transport trans)
{
    terminate();
    if ((trans == None) || !socket)
	return false;
    m_socket = socket;
    m_trans = trans;
    return true;
}

/* vi: set ts=8 sw=4 sts=4 noet: */
