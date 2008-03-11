/**
 * management.cpp
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

#define MAKE_NAME(x) { #x, SS7MsgSNM::x }
static const TokenDict s_snm_names[] = {
    // this list must be kept in synch with the header
    MAKE_NAME(COO),
    MAKE_NAME(ECO),
    MAKE_NAME(RCT),
    MAKE_NAME(TFP),
    MAKE_NAME(RST),
    MAKE_NAME(RSP), // alias
    MAKE_NAME(LIN),
    MAKE_NAME(TRA),
    MAKE_NAME(DLC),
    MAKE_NAME(UPU),
    MAKE_NAME(COA),
    MAKE_NAME(ECA),
    MAKE_NAME(TFC),
    MAKE_NAME(TCP),
    MAKE_NAME(TFPA), // alias
    MAKE_NAME(RSR),
    MAKE_NAME(LUN),
    MAKE_NAME(TRW),
    MAKE_NAME(CSS),
    MAKE_NAME(TFR),
    MAKE_NAME(RCP),
    MAKE_NAME(LIA),
    MAKE_NAME(CNS),
    MAKE_NAME(TCR),
    MAKE_NAME(RCR),
    MAKE_NAME(LUA),
    MAKE_NAME(CNP),
    MAKE_NAME(CBD),
    MAKE_NAME(TFA),
    MAKE_NAME(LID),
    MAKE_NAME(CBA),
    MAKE_NAME(TCA),
    MAKE_NAME(TFAA), // alias
    MAKE_NAME(LFU),
    MAKE_NAME(LLT),
    MAKE_NAME(LLI), // alias
    MAKE_NAME(LRT),
    MAKE_NAME(LRI), // alias
    { 0, 0 }
};

static const TokenDict s_snm_group[] = {
    // this list must be kept in synch with the header
    MAKE_NAME(CHM),
    MAKE_NAME(ECM),
    MAKE_NAME(FCM),
    MAKE_NAME(TFM),
    MAKE_NAME(RSM),
    MAKE_NAME(MIM),
    MAKE_NAME(TRM),
    MAKE_NAME(DLM),
    MAKE_NAME(UFC),
    { 0, 0 }
};
#undef MAKE_NAME


// Constructor
SS7MsgSNM::SS7MsgSNM(unsigned char type)
    : SignallingMessage(lookup((Type)type,"Unknown")),
    m_type(type)
{
}

// Parse a received buffer and build a message from it
SS7MsgSNM* SS7MsgSNM::parse(SS7Management* receiver, unsigned char type,
    SS7PointCode::Type pcType, const unsigned char* buf, unsigned int len)
{
    SS7MsgSNM* msg = new SS7MsgSNM(type);
    Debug(receiver,DebugAll,"Decoding msg=%s from buf=%p len=%u [%p]",
	msg->name(),buf,len,receiver);
    // TODO: parse the rest of the message. Check extra bytes (message specific)
    if (!(buf && len))
	return msg;
    unsigned int required = 0;
    unsigned int expected = 0;
    while (true) {
	// TFP,TFR,TFA: Q.704 15.7 The must be at lease 2 bytes in buffer
	if (type == TFP || type == TFR || type == TFA) {
	    if (len < 2) {
		required = 2;
		break;
	    }
	    expected = 2;
	    // 2 bytes destination
	    SS7PointCode pc;
	    unsigned char spare;
	    if (pc.assign(pcType,buf,len,&spare)) {
		String tmp;
		tmp << pc;
		msg->params().addParam("destination",tmp);
		tmp.hexify(&spare,1);
		msg->params().addParam("space",tmp);
	    }
	    else
		Debug(receiver,DebugNote,
		    "Failed to decode destination for msg=%s len=%u [%p]",
		    msg->name(),len,receiver);
	    break;
	}
	break;
    }
    return msg;
}

const TokenDict* SS7MsgSNM::names()
{
    return s_snm_names;
}


#define MAKE_NAME(x) { #x, SS7MsgMTN::x }
static const TokenDict s_mtn_names[] = {
    // this list must be kept in synch with the header
    MAKE_NAME(SLTM),
    MAKE_NAME(SLTA),
    { 0, 0 }
};
#undef MAKE_NAME

const TokenDict* SS7MsgMTN::names()
{
    return s_mtn_names;
}


bool SS7Management::receivedMSU(const SS7MSU& msu, const SS7Label& label, SS7Layer3* network, int sls)
{
    if (msu.getSIF() != SS7MSU::SNM)
	return false;
    Debug(this,DebugStub,"Please implement SS7Management::receivedMSU(%p,%p,%p,%d) [%p]",
	&msu,&label,network,sls,this);

    unsigned int len = msu.length() - label.length() - 1;
    // according to Q.704 there should be at least the heading codes (8 bit)
    const unsigned char* buf = msu.getData(label.length()+1,1);
    if (!buf)
	return false;
    SS7MsgSNM* msg = SS7MsgSNM::parse(this,buf[0],label.type(),buf+1,len-1);
    if (!msg)
	return false;

    // TODO: implement

    String l;
    l << label;
    String tmp;
    tmp.hexify((void*)buf,len,' ');
    String params;
    unsigned int n = msg->params().count();
    if (n)
	for (unsigned int i = 0; i < n; i++) {
	    NamedString* ns = static_cast<NamedString*>(msg->params().getParam(i));
	    if (ns)
		params.append(String(ns->name()) + "=" + *ns,",");
	}
    Debug(this,DebugMild,
	"Unhandled SNM type=%s group=%s label=%s params:%s len=%u: %s ",
	msg->name(),lookup(msg->group(),s_snm_group,"Spare"),
	l.c_str(),params.c_str(),len,tmp.c_str());

    TelEngine::destruct(msg);
    return false;
}

void SS7Management::notify(SS7Layer3* network, int sls)
{
    Debug(this,DebugStub,"Please implement SS7Management::notify(%p,%d) [%p]",
	network,sls,this);
    if (network && network->operational(sls)) {
	// one link gone up - see MTP restart procedure (Q.704 - 9.1, 9.2)
	// TODO: implement MTP restart procedure (Q.704 - 9.1, 9.2)
	// for now just send a Traffic Restart Allowed
	// FIXME: get point codes and network indicator from configuration
	SS7PointCode dpc(1,8,1);
	SS7PointCode opc(1,8,2);
	unsigned char sio = SS7MSU::National;
	SS7PointCode::Type type = network->type(sio);
	sio |= SS7MSU::SNM;
	SS7Label lbl(type,dpc,opc,sls,0);
	SS7MSU tra(sio,lbl,0,1);
	unsigned char* d = tra.getData(lbl.length()+1,1);
	d[0] = SS7MsgSNM::TRA;
	transmitMSU(tra,lbl,sls);
    }
}


bool SS7Maintenance::receivedMSU(const SS7MSU& msu, const SS7Label& label, SS7Layer3* network, int sls)
{
    if (msu.getSIF() != SS7MSU::MTN)
	return false;
    XDebug(this,DebugStub,"Possibly incomplete SS7Maintenance::receivedMSU(%p,%p,%p,%d) [%p]",
	&msu,&label,network,sls,this);

    unsigned int mlen = msu.length()-label.length()-1;
    // Q.707 says test pattern length should be 1-15 but we accept 0 as well
    const unsigned char* s = msu.getData(label.length()+1,2);
    if (!s)
	return false;
    unsigned char len = s[1] >> 4;
    // get a pointer to the test pattern
    const unsigned char* t = msu.getData(label.length()+3,len);
    if (!t) {
	Debug(this,DebugMild,"Received MTN type %02X length %u with invalid pattern length %u [%p]",
	    s[0],msu.length(),len,this);
	return false;
    }
    switch (s[0]) {
	case SS7MsgMTN::SLTM:
	    {
		Debug(this,DebugNote,"Received SLTM with test pattern length %u",len);
		SS7Label lbl(label,sls,0);
		SS7MSU answer(msu.getSIO(),lbl,0,len+2);
		unsigned char* d = answer.getData(lbl.length()+1,len+2);
		if (!d)
		    return false;
		*d++ = SS7MsgMTN::SLTA;
		*d++ = len << 4;
		while (len--)
		    *d++ = *t++;
		return transmitMSU(answer,lbl,sls) >= 0;
	    }
	    return true;
	case SS7MsgMTN::SLTA:
	    Debug(this,DebugNote,"Received SLTA with test pattern length %u",len);
	    return true;
    }

    String tmp;
    tmp.hexify((void*)s,mlen,' ');
    Debug(this,DebugMild,"Unhandled MTN type %s length %u: %s",
	SS7MsgMTN::lookup((SS7MsgMTN::Type)s[0],"unknown"),mlen,tmp.c_str());
    return false;
}

void SS7Maintenance::notify(SS7Layer3* network, int sls)
{
    Debug(this,DebugStub,"Please implement SS7Maintenance::notify(%p,%d) [%p]",
	network,sls,this);
}

/* vi: set ts=8 sw=4 sts=4 noet: */
