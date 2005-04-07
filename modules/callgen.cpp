/**
 * callgen.cpp
 * This file is part of the YATE Project http://YATE.null.ro
 * 
 * Call Generator
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

#include <yatephone.h>

#include <stdlib.h>
#include <unistd.h>

using namespace TelEngine;

static Configuration s_cfg;
static bool s_runs = false;
static int s_total = 0;
static int s_current = 0;
static int s_ringing = 0;
static int s_answers = 0;

static int s_numcalls = 0;

static const char s_help[] = "callgen {start|stop|drop|pause|resume|single|info|load|save|set paramname[=value]}";

class GenConnection : public Channel
{
public:
    GenConnection(const String& callto);
    ~GenConnection();
    virtual void disconnected(bool final, const char *reason);
    void ringing();
    void answered();
    void hangup();
    void makeSource();
    inline const String& party() const
	{ return m_callto; }
    inline u_int64_t age() const
	{ return Time::now() - m_start; }
    static bool oneCall(String* target = 0);
private:
    String m_callto;
    u_int64_t m_start;
};

class GenThread : public Thread
{
public:
    GenThread()
	: Thread("CallGen")
	{ }
    virtual void run();
};

class CmdHandler : public MessageReceiver
{
public:
    enum {
	Drop,
	Status,
	Command,
	Help
    };
    virtual bool received(Message &msg, int id);
    bool doCommand(String& line, String& rval);
};

class CallGenPlugin : public Driver
{
public:
    CallGenPlugin();
    virtual ~CallGenPlugin();
    virtual void initialize();
private:
    bool m_first;
};

INIT_PLUGIN(CallGenPlugin);

GenConnection::GenConnection(const String& callto)
    : Channel(__plugin), m_callto(callto)
{
    m_start = Time::now();
    status("calling");
    driver()->lock();
    ++s_current;
    driver()->unlock();
}

GenConnection::~GenConnection()
{
    driver()->lock();
    --s_current;
    driver()->unlock();
}

bool GenConnection::oneCall(String* target)
{
    Message m("call.route");
    m.addParam("driver","callgen");
    m.addParam("caller",s_cfg.getValue("parameters","caller","yate"));
    String callto(s_cfg.getValue("parameters","callto"));
    if (callto.null()) {
	String called(s_cfg.getValue("parameters","called"));
	if (called.null()) {
	    int n_min = s_cfg.getIntValue("parameters","minnum");
	    if (n_min <= 0)
		return false;
	    int n_max = s_cfg.getIntValue("parameters","maxnum",n_min);
	    if (n_max < n_min)
		return false;
	    called = (unsigned)(n_min + (((n_max - n_min) * (int64_t)::random()) / RAND_MAX));
	}
	if (target)
	    *target = called;
	m.addParam("called",called);
	if (!Engine::dispatch(m) || m.retValue().null()) {
	    Debug("CallGen",DebugInfo,"No route to call '%s'",called.c_str());
	    return false;
	}
	callto = m.retValue();
	m.retValue().clear();
    }
    if (target) {
	if (*target)
	    *target << " ";
	*target << callto;
    }
    m = "call.execute";
    m.addParam("callto",callto);
    GenConnection* conn = new GenConnection(callto);
    m.addParam("id",conn->id());
    m.userData(conn);
    if (Engine::dispatch(m)) {
	conn->setTarget(m.getValue("targetid"));
	if (conn->targetid().null()) {
	    Debug(DebugInfo,"Answering now generated call %s [%p] because we have no targetid",
		conn->id().c_str(),conn);
	    conn->answered();
	}
	conn->deref();
	return true;
    }
    Debug("CallGen",DebugInfo,"Rejecting '%s' unconnected to '%s'",
	conn->id().c_str(),callto.c_str());
    conn->destruct();
    return false;
}

void GenConnection::disconnected(bool final, const char *reason)
{
    Debug("CallGen",DebugInfo,"Disconnected '%s' reason '%s' [%p]",id().c_str(),reason,this);
    m_status = "disconnected";
}

void GenConnection::ringing()
{
    Debug("CallGen",DebugInfo,"Ringing '%s' [%p]",id().c_str(),this);
    m_status = "ringing";
    s_mutex.lock();
    ++s_ringing;
    bool media =s_cfg.getBoolValue("parameters","earlymedia",true);
    s_mutex.unlock();
    if (media)
	makeSource();
}

void GenConnection::answered()
{
    Debug("CallGen",DebugInfo,"Answered '%s' [%p]",id().c_str(),this);
    m_status = "answered";
    s_mutex.lock();
    ++s_answers;
    s_mutex.unlock();
    makeSource();
}

void GenConnection::hangup()
{
}

void GenConnection::makeSource()
{
    if (getSource())
	return;
    s_mutex.lock();
    String src(s_cfg.getValue("parameters","source"));
    s_mutex.unlock();
    if (src) {
	Message m("chan.attach");
	complete(m,false);
	m.addParam("source",src);
	m.userData(this);
	Engine::dispatch(m);
    }
}

void GenThread::run()
{
    for (;;) {
	Thread::sleep(1);
	Lock lock(s_mutex);
	int maxcalls = s_cfg.getIntValue("parameters","maxcalls",5);
	if (!s_runs || (s_current >= maxcalls) || (s_numcalls <= 0))
	    continue;
	--s_numcalls;
	lock.drop();
	GenConnection::oneCall();
    }
}

bool CmdHandler::doCommand(String& line, String& rval)
{
    if (line.startSkip("set")) {
	int q = line.find('=');
	s_mutex.lock();
	if (q >= 0) {
	    String val = line.substr(q+1).trimBlanks();
	    line = line.substr(0,q).trimBlanks().toLower();
	    s_cfg.setValue("parameters",line,val.c_str());
	    rval << "Set '" << line << "' to '" << val << "'";
	}
	else {
	    line.toLower();
	    rval << "Value of '" << line << "' is '" << s_cfg.getValue("parameters",line) << "'";
	}
	s_mutex.unlock();
    }
    else if (line == "info") {
	s_mutex.lock();
	rval << "Made " << s_total << " calls, "
	    << s_ringing << " ring, "
	    << s_answers << " answered, "
	    << s_current << " running";
	if (s_runs)
	    rval << ", " << s_numcalls << " to go";
	s_mutex.unlock();
    }
    else if (line == "start") {
	s_mutex.lock();
	s_numcalls = s_cfg.getIntValue("parameters","numcalls",100);
	rval << "Generating " << s_numcalls << " new calls";
	s_runs = true;
	s_mutex.unlock();
    }
    else if (line == "stop") {
	s_mutex.lock();
	s_runs = false;
	s_numcalls = 0;
	s_mutex.unlock();
	dropAll();
	rval << "Stopping generator and clearing calls";
    }
    else if (line == "drop") {
	s_mutex.lock();
	bool tmp = s_runs;
	s_runs = false;
	s_mutex.unlock();
	dropAll();
	s_runs = tmp;
	rval << "Clearing calls and continuing";
    }
    else if (line == "pause") {
	s_runs = false;
	rval << "No longer generating new calls";
    }
    else if (line == "resume") {
	s_mutex.lock();
	rval << "Resumed generating new calls, " << s_numcalls << " to go";
	s_runs = true;
	s_mutex.unlock();
    }
    else if (line == "single") {
	String dest;
	if (GenConnection::oneCall(&dest))
	    rval << "Calling " << dest;
	else {
	    rval << "Failed to start call";
	    if (dest)
		rval << " to " << dest;
	}
    }
    else if (line == "load") {
	s_mutex.lock();
	s_cfg.load();
	rval << "Loaded config from " << s_cfg;
	s_mutex.unlock();
    }
    else if (line == "save") {
	s_mutex.lock();
	if (s_cfg.getBoolValue("general","cansave",true)) {
	    s_cfg.save();
	    rval << "Saved config to " << s_cfg;
	}
	else
	    rval << "Saving is disabled from config file";
	s_mutex.unlock();
    }
    else if (line.null() || (line == "help") || (line == "?"))
	rval << "Usage: " << s_help;
    else
	return false;
    rval << "\n";
    return true;
}

bool CmdHandler::received(Message &msg, int id)
{
    String tmp;
    switch (id) {
	case Status:
	    tmp = msg.getValue("module");
	    if (tmp.null() || (tmp == "callgen")) {
		s_mutex.lock();
		msg.retValue() << "name=callgen,type=varchans,format=Status|Callto"
		    << ";total=" << s_total
		    << ",ring=" << s_ringing
		    << ",answered=" << s_answers
		    << ",chans=" << s_current << ";";
		ObjList *l = &s_calls;
		bool first = true;
		for (; l; l=l->next()) {
		    GenConnection *c = static_cast<GenConnection *>(l->get());
		    if (c) {
			if (first)
			    first = false;
			else
			    msg.retValue() << ",";
			msg.retValue() << c->id() << "=" << c->status() << "|" << c->party();
		    }
		}
		msg.retValue() << "\n";
		s_mutex.unlock();
		if (tmp)
		    return true;
	    }
	    break;
	case Command:
	    tmp = msg.getValue("line");
	    if (tmp.startSkip("callgen"))
		return doCommand(tmp,msg.retValue());
	    break;
	case Help:
	    tmp = msg.getValue("line");
	    if (tmp.null() || (tmp == "callgen")) {
		msg.retValue() << "  " << s_help << "\n";
		if (tmp)
		    return true;
	    }
	    break;
    }
    return false;
}

CallGenPlugin::CallGenPlugin()
    : Driver("callgen","varchan"), m_first(true)
{
    Output("Loaded module Call Generator");
}

CallGenPlugin::~CallGenPlugin()
{
    Output("Unloading module Call Generator");
}

void CallGenPlugin::initialize()
{
    Output("Initializing module Call Generator");
    s_cfg = Engine::configFile("callgen");
    s_cfg.load();
    if (m_first) {
	m_first = false;

	CmdHandler* cmh = new CmdHandler;
	Engine::install(new MessageRelay("engine.status",cmh,CmdHandler::Status));
	Engine::install(new MessageRelay("engine.command",cmh,CmdHandler::Command));
	Engine::install(new MessageRelay("engine.help",cmh,CmdHandler::Help));

	GenThread* gen = new GenThread;
	if (!gen->startup()) {
	    Debug(DebugGoOn,"Failed to start call generator thread");
	    delete gen;
	}
    }
}

/* vi: set ts=8 sw=4 sts=4 noet: */
