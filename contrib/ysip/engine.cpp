/**
 * engine.cpp
 * Yet Another SIP Stack
 * This file is part of the YATE Project http://YATE.null.ro 
 *
 * Yet Another Telephony Engine - a fully featured software PBX and IVR
 * Copyright (C) 2004 Null Team
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <telengine.h>
#include <yateversn.h>

#include <string.h>
#include <stdlib.h>

#include <ysip.h>

using namespace TelEngine;

SIPParty::SIPParty()
    : m_reliable(false)
{
    Debug(DebugAll,"SIPParty::SIPParty() [%p]",this);
}

SIPParty::SIPParty(bool reliable)
    : m_reliable(reliable)
{
    Debug(DebugAll,"SIPParty::SIPParty(%d) [%p]",reliable,this);
}

SIPParty::~SIPParty()
{
    Debug(DebugAll,"SIPParty::~SIPParty() [%p]",this);
}

SIPEvent::SIPEvent(SIPMessage* message, SIPTransaction* transaction)
    : m_message(message), m_transaction(transaction),
      m_state(SIPTransaction::Invalid)
{
    Debug(DebugAll,"SIPEvent::SIPEvent(%p,%p) [%p]",message,transaction,this);
    if (m_message)
	m_message->ref();
    if (m_transaction) {
	m_transaction->ref();
	m_state = m_transaction->getState();
    }
}

SIPEvent::~SIPEvent()
{
    Debugger debug(DebugAll,"SIPEvent::~SIPEvent"," [%p]",this);
    if (m_transaction)
	m_transaction->deref();
    if (m_message)
	m_message->deref();
}

SIPEngine::SIPEngine(const char* userAgent)
    : m_t1(500000), m_t4(5000000), m_maxForwards(70), m_userAgent(userAgent)
{
    Debug(DebugInfo,"SIPEngine::SIPEngine() [%p]",this);
    if (m_userAgent.null())
	m_userAgent << "YATE/" << YATE_VERSION;
}

SIPEngine::~SIPEngine()
{
    Debug(DebugInfo,"SIPEngine::~SIPEngine() [%p]",this);
}

SIPTransaction* SIPEngine::addMessage(SIPParty* ep, const char *buf, int len)
{
    Debug("SIPEngine",DebugInfo,"addMessage(%p,%d) [%p]",buf,len,this);
    SIPMessage* msg = SIPMessage::fromParsing(ep,buf,len);
    if (ep)
	ep->deref();
    if (msg) {
	SIPTransaction* tr = addMessage(msg);
	msg->deref();
	return tr;
    }
    return 0;
}

SIPTransaction* SIPEngine::addMessage(SIPMessage* message)
{
    Debug("SIPEngine",DebugInfo,"addMessage(%p) [%p]",message,this);
    if (!message)
	return 0;
    const NamedString* br = message->getParam("Via","branch");
    String branch(br ? *br : 0);
    if (!branch.startsWith("z9hG4bK"))
	branch.clear();
    Lock lock(m_mutex);
    ObjList* l = &TransList;
    for (; l; l = l->next()) {
	SIPTransaction* t = static_cast<SIPTransaction*>(l->get());
	if (t && t->processMessage(message,branch))
	    return t;
    }
    if (message->isAnswer()) {
	Debug("SIPEngine",DebugInfo,"Message %p was an unhandled answer [%p]",message,this);
	return 0;
    }
    return new SIPTransaction(message,this,false);
}

bool SIPEngine::process()
{
    SIPEvent* e = getEvent();
    if (!e)
	return false;
    Debug("SIPEngine",DebugInfo,"process() got event %p",e);
    processEvent(e);
    return true;
}

SIPEvent* SIPEngine::getEvent()
{
    Lock lock(m_mutex);
    ObjList* l = &TransList;
    for (; l; l = l->next()) {
	SIPTransaction* t = static_cast<SIPTransaction*>(l->get());
	if (t) {
	    SIPEvent* e = t->getEvent();
	    if (e) {
		Debug("SIPEngine",DebugInfo,"Got event %p (%d) from transaction %p [%p]",
		    e,e->getState(),t,this);
		return e;
	    }
	}
    }
    return 0;
}

void SIPEngine::processEvent(SIPEvent *event)
{
    Lock lock(m_mutex);
    if (event) {
	if (event->isOutgoing() && event->getParty())
	    event->getParty()->transmit(event);
	delete event;
    }
}

unsigned long long SIPEngine::getTimer(char which, bool reliable) const
{
    switch (which) {
	case '1':
	    // T1: RTT Estimate 500ms default
	    return m_t1;
	case '2':
	    // T2: Maximum retransmit interval
	    //  for non-INVITE requests and INVITE responses
	    return 4000000;
	case '4':
	    // T4: Maximum duration a message will remain in the network
	    return m_t4;
	case 'A':
	    // A: INVITE request retransmit interval, for UDP only
	    return m_t1;
	case 'B':
	    // B: INVITE transaction timeout timer
	    return 64*m_t1;
	case 'C':
	    // C: proxy INVITE transaction timeout
	    return 180000000;
	case 'D':
	    // D: Wait time for response retransmits
	    return reliable ? 0 : 32000000;
	case 'E':
	    // E: non-INVITE request retransmit interval, UDP only
	    return m_t1;
	case 'F':
	    // F: non-INVITE transaction timeout timer
	    return 64*m_t1;
	case 'G':
	    // G: INVITE response retransmit interval
	    return m_t1;
	case 'H':
	    // H: Wait time for ACK receipt
	    return 64*m_t1;
	case 'I':
	    // I: Wait time for ACK retransmits
	    return reliable ? 0 : m_t4;
	case 'J':
	    // J: Wait time for non-INVITE request retransmits
	    return reliable ? 0 : 64*m_t1;
	case 'K':
	    // K: Wait time for response retransmits
	    return reliable ? 0 : m_t4;
    }
    Debug("SIPEngine",DebugInfo,"Requested invalid timer '%c' [%p]",which,this);
    return 0;
}

/* vi: set ts=8 sw=4 sts=4 noet: */
