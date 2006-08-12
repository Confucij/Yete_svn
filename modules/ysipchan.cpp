/**
 * ysipchan.cpp
 * This file is part of the YATE Project http://YATE.null.ro
 *
 * Yet Another Sip Channel
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

#include <yatephone.h>
#include <yatesip.h>

#include <string.h>


using namespace TelEngine;
namespace { // anonymous

#define EXPIRES_MIN 60
#define EXPIRES_DEF 600
#define EXPIRES_MAX 3600

/* Yate Payloads for the AV profile */
static TokenDict dict_payloads[] = {
    { "mulaw",   0 },
    { "alaw",    8 },
    { "gsm",     3 },
    { "lpc10",   7 },
    { "slin",   11 },
    { "g726",    2 },
    { "g722",    9 },
    { "g723",    4 },
    { "g728",   15 },
    { "g729",   18 },
    { "ilbc",   98 },
    { "ilbc20", 98 },
    { "ilbc30", 98 },
    { "h261",   31 },
    { "h263",   34 },
    { "mpv",    32 },
    {      0,    0 },
};

/* SDP Payloads for the AV profile */
static TokenDict dict_rtpmap[] = {
    { "PCMU/8000",     0 },
    { "PCMA/8000",     8 },
    { "GSM/8000",      3 },
    { "LPC/8000",      7 },
    { "L16/8000",     11 },
    { "G726-32/8000",  2 },
    { "G722/8000",     9 },
    { "G723/8000",     4 },
    { "G728/8000",    15 },
    { "G729/8000",    18 },
    { "iLBC/8000",    98 },
    { "H261/90000",   31 },
    { "H263/90000",   34 },
    { "MPV/90000",    32 },
    {           0,     0 },
};

static TokenDict dict_errors[] = {
    { "incomplete", 484 },
    { "noroute", 404 },
    { "noroute", 604 },
    { "noconn", 503 },
    { "noauth", 401 },
    { "nomedia", 415 },
    { "nocall", 481 },
    { "busy", 486 },
    { "busy", 600 },
    { "noanswer", 487 },
    { "rejected", 406 },
    { "rejected", 606 },
    { "forbidden", 403 },
    { "forbidden", 603 },
    { "offline", 404 },
    { "congestion", 480 },
    { "failure", 500 },
    { "pending", 491 },
    { "looping", 483 },
    {  0,   0 },
};

static const char s_dtmfs[] = "0123456789*#ABCDF";

class RtpMedia : public String
{
public:
    RtpMedia(const char* media, const char* formats, int rport = -1, int lport = -1);
    virtual ~RtpMedia();
    inline bool isAudio() const
	{ return m_audio; }
    inline const String& suffix() const
	{ return m_suffix; }
    inline const String& id() const
	{ return m_id; }
    inline const String& format() const
	{ return m_format; }
    inline const String& formats() const
	{ return m_formats; }
    inline const String& remotePort() const
	{ return m_rPort; }
    inline const String& localPort() const
	{ return m_lPort; }
    const char* fmtList() const;
    bool update(const char* formats, int rport = -1, int lport = -1);
    void update(const Message& msg, bool pickFormat);
private:
    bool m_audio;
    // suffix used for this type
    String m_suffix;
    // list of supported format names
    String m_formats;
    // format used for sending data
    String m_format;
    // id of the local RTP channel
    String m_id;
    // remote RTP port
    String m_rPort;
    // local RTP port
    String m_lPort;
};

class YateUDPParty : public SIPParty
{
public:
    YateUDPParty(Socket* sock, const SocketAddr& addr, int localPort, const char* localAddr = 0);
    ~YateUDPParty();
    virtual void transmit(SIPEvent* event);
    virtual const char* getProtoName() const;
    virtual bool setParty(const URI& uri);
protected:
    Socket* m_sock;
    SocketAddr m_addr;
};

class YateSIPEndPoint;

class YateSIPEngine : public SIPEngine
{
public:
    YateSIPEngine(YateSIPEndPoint* ep);
    virtual bool buildParty(SIPMessage* message);
    virtual bool checkUser(const String& username, const String& realm, const String& nonce,
	const String& method, const String& uri, const String& response,
	const SIPMessage* message, GenObject* userData);
    inline bool prack() const
	{ return m_prack; }
    inline bool info() const
	{ return m_info; }
private:
    static bool copyAuthParams(NamedList* dest, const NamedList& src);
    YateSIPEndPoint* m_ep;
    bool m_prack;
    bool m_info;
};

class YateSIPLine : public String
{
    YCLASS(YateSIPLine,String)
public:
    YateSIPLine(const String& name);
    virtual ~YateSIPLine();
    void setupAuth(SIPMessage* msg) const;
    SIPMessage* buildRegister(int expires) const;
    void login();
    void logout();
    bool process(SIPEvent* ev);
    void timer(const Time& when);
    bool update(const Message& msg);
    inline const String& getLocalAddr() const
	{ return m_localAddr; }
    inline const String& getPartyAddr() const
	{ return m_outbound ? m_outbound : m_partyAddr; }
    inline int getLocalPort() const
	{ return m_localPort; }
    inline int getPartyPort() const
	{ return m_partyPort; }
    inline bool localDetect() const
	{ return m_localDetect; }
    inline const String& getFullName() const
	{ return m_display; }
    inline const String& getUserName() const
	{ return m_username; }
    inline const String& getAuthName() const
	{ return m_authname ? m_authname : m_username; }
    inline const String& domain() const
	{ return m_domain ? m_domain : m_registrar; }
    inline bool valid() const
	{ return m_valid; }
    inline bool marked() const
	{ return m_marked; }
    inline void marked(bool mark)
	{ m_marked = mark; }
private:
    void clearTransaction();
    void detectLocal(const SIPMessage* msg);
    bool change(String& dest, const String& src);
    bool change(int& dest, int src);
    void keepalive();
    void setValid(bool valid, const char* reason = 0);
    String m_registrar;
    String m_username;
    String m_authname;
    String m_password;
    String m_outbound;
    String m_domain;
    String m_display;
    u_int64_t m_resend;
    u_int64_t m_keepalive;
    int m_interval;
    int m_alive;
    SIPTransaction* m_tr;
    bool m_marked;
    bool m_valid;
    String m_localAddr;
    String m_partyAddr;
    int m_localPort;
    int m_partyPort;
    bool m_localDetect;
};

class YateSIPEndPoint : public Thread
{
public:
    YateSIPEndPoint();
    ~YateSIPEndPoint();
    bool Init(void);
    void run(void);
    bool incoming(SIPEvent* e, SIPTransaction* t);
    void invite(SIPEvent* e, SIPTransaction* t);
    void regreq(SIPEvent* e, SIPTransaction* t);
    void options(SIPEvent* e, SIPTransaction* t);
    bool generic(SIPEvent* e, SIPTransaction* t);
    bool buildParty(SIPMessage* message, const char* host = 0, int port = 0, const YateSIPLine* line = 0);
    inline YateSIPEngine* engine() const
	{ return m_engine; }
    inline int port() const
	{ return m_port; }
    inline Socket* socket() const
	{ return m_sock; }
private:
    void addMessage(const char* buf, int len, const SocketAddr& addr, int port);
    int m_port;
    String m_local;
    Socket* m_sock;
    SocketAddr m_addr;
    YateSIPEngine *m_engine;
};

class YateSIPRefer : public Thread
{
public:
    YateSIPRefer(const String& transferorID, const String& transferredID, 
	Driver* transferredDrv, Message* msg, SIPMessage* sipNotify);
    virtual void run(void);
    virtual void cleanup(void);
private:
    bool route(void);
    String m_transferorID;           // Transferor channel's id
    String m_transferredID;          // Transferred channel's id
    Driver* m_transferredDrv;        // Transferred driver's pointer
    Message* m_msg;                  // 'call.route' message
    SIPMessage* m_sipNotify;         // NOTIFY message to send the result
};

class YateSIPConnection : public Channel
{
    YCLASS(YateSIPConnection,Channel)
public:
    enum {
	Incoming = 0,
	Outgoing = 1,
	Ringing = 2,
	Established = 3,
	Cleared = 4,
    };
    enum {
	MediaMissing,
	MediaStarted,
	MediaMuted
    };
    YateSIPConnection(SIPEvent* ev, SIPTransaction* tr);
    YateSIPConnection(Message& msg, const String& uri, const char* target = 0);
    ~YateSIPConnection();
    virtual void disconnected(bool final, const char *reason);
    virtual bool msgProgress(Message& msg);
    virtual bool msgRinging(Message& msg);
    virtual bool msgAnswered(Message& msg);
    virtual bool msgTone(Message& msg, const char* tone);
    virtual bool msgText(Message& msg, const char* text);
    virtual bool msgUpdate(Message& msg);
    virtual bool callRouted(Message& msg);
    virtual void callAccept(Message& msg);
    virtual void callRejected(const char* error, const char* reason, const Message* msg);
    void startRouter();
    bool process(SIPEvent* ev);
    bool checkUser(SIPTransaction* t, bool refuse = true);
    void doBye(SIPTransaction* t);
    void doCancel(SIPTransaction* t);
    void doInfo(SIPTransaction* t);
    void doRefer(SIPTransaction* t);
    void reInvite(SIPTransaction* t);
    void hangup();
    inline const SIPDialog& dialog() const
	{ return m_dialog; }
    inline void setStatus(const char *stat, int state = -1)
	{ status(stat); if (state >= 0) m_state = state; }
    inline void setReason(const char* str = "Request Terminated", int code = 487)
	{ m_reason = str; m_reasonCode = code; }
    inline SIPTransaction* getTransaction() const
	{ return m_tr; }
    inline const String& callid() const
	{ return m_callid; }
    inline const String& user() const
	{ return m_user; }
    inline const String& getHost() const
	{ return m_host; }
    inline int getPort() const
	{ return m_port; }
    inline const String& getRtpAddr() const
	{ return m_externalAddr ? m_externalAddr : m_rtpLocalAddr; }
    inline void referTerminated()
	{ m_referring = false; }
private:
    virtual void statusParams(String& str);
    void setMedia(ObjList* media);
    void clearTransaction();
    void detachTransaction2();
    SIPMessage* createDlgMsg(const char* method, const char* uri = 0);
    bool emitPRACK(const SIPMessage* msg);
    bool dispatchRtp(RtpMedia* media, const char* addr, bool start, bool pick);
    SDPBody* createSDP(const char* addr = 0, ObjList* mediaList = 0);
    SDPBody* createProvisionalSDP(Message& msg);
    SDPBody* createPasstroughSDP(Message& msg, bool update = true);
    SDPBody* createRtpSDP(const char* addr, const Message& msg);
    SDPBody* createRtpSDP(bool start = false);
    bool startRtp();
    bool addSdpParams(Message& msg, const SIPBody* body);
    bool addRtpParams(Message& msg, const String& natAddr, const SIPBody* body);
    bool initUnattendedTransfer(Message*& msg, SIPMessage*& sipNotify, const SIPMessage* sipRefer, const SIPHeaderLine* refHdr);

    SIPTransaction* m_tr;
    SIPTransaction* m_tr2;
    // are we already hung up?
    bool m_hungup;
    // should we send a BYE?
    bool m_byebye;
    // should we CANCEL?
    bool m_cancel;
    int m_state;
    String m_reason;
    int m_reasonCode;
    String m_callid;
    // SIP dialog of this call, used for re-INVITE or BYE
    SIPDialog m_dialog;
    // remote URI as we send in dialog messages
    URI m_uri;
    // our external IP address, possibly outside of a NAT
    String m_externalAddr;
    // if we do RTP forwarding or not
    bool m_rtpForward;
    // if we forward the SDP as-is
    bool m_sdpForward;
    // remote RTP address
    String m_rtpAddr;
    // local RTP address
    String m_rtpLocalAddr;
    // list of media descriptors
    ObjList* m_rtpMedia;
    // unique SDP session number
    int m_sdpSession;
    // SDP version number, incremented each time we generate a new SDP
    int m_sdpVersion;
    String m_host;
    String m_user;
    String m_line;
    int m_port;
    Message* m_route;
    ObjList* m_routes;
    bool m_authBye;
    int m_mediaStatus;
    bool m_inband;
    bool m_info;
    // REFER already running
    bool m_referring;
};

class YateSIPGenerate : public GenObject
{
    YCLASS(YateSIPGenerate,GenObject)
public:
    YateSIPGenerate(SIPMessage* m);
    virtual ~YateSIPGenerate();
    bool process(SIPEvent* ev);
    bool busy() const
	{ return m_tr != 0; }
    int code() const
	{ return m_code; }
private:
    void clearTransaction();
    SIPTransaction* m_tr;
    int m_code;
};

class UserHandler : public MessageHandler
{
public:
    UserHandler()
	: MessageHandler("user.login",150)
	{ }
    virtual bool received(Message &msg);
};

class SipHandler : public MessageHandler
{
public:
    SipHandler()
	: MessageHandler("xsip.generate",110)
	{ }
    virtual bool received(Message &msg);
};

class SIPDriver : public Driver
{
public:
    SIPDriver();
    ~SIPDriver();
    virtual void initialize();
    virtual bool msgExecute(Message& msg, String& dest);
    virtual bool msgRoute(Message& msg);
    virtual bool received(Message& msg, int id);
    inline YateSIPEndPoint* ep() const
	{ return m_endpoint; }
    YateSIPConnection* findCall(const String& callid);
    YateSIPConnection* findDialog(const SIPDialog& dialog);
    YateSIPLine* findLine(const String& line);
    YateSIPLine* findLine(const String& addr, int port, const String& user = String::empty());
    bool validLine(const String& line);
private:
    YateSIPEndPoint *m_endpoint;
};

static SIPDriver plugin;
static ObjList s_lines;
static Configuration s_cfg;
static String s_realm = "Yate";
static int s_maxForwards = 20;
static bool s_privacy = false;
static bool s_auto_nat = true;
static bool s_inband = false;
static bool s_info = false;
static bool s_forward_sdp = false;
static bool s_auth_register = true;

static int s_expires_min = EXPIRES_MIN;
static int s_expires_def = EXPIRES_DEF;
static int s_expires_max = EXPIRES_MAX;

// Parse a SDP and return a possibly filtered list of SDP media
static ObjList* parseSDP(const SDPBody* sdp, String& addr, ObjList* oldMedia = 0, const char* media = 0)
{
    const NamedString* c = sdp->getLine("c");
    if (c) {
	String tmp(*c);
	if (tmp.startSkip("IN IP4")) {
	    tmp.trimBlanks();
	    // Handle the case media is muted
	    if (tmp == "0.0.0.0")
		tmp.clear();
	    addr = tmp;
	}
    }
    ObjList* lst = 0;
    c = sdp->getLine("m");
    for (; c; c = sdp->getNextLine(c)) {
	String tmp(*c);
	int sep = tmp.find(' ');
	if (sep < 1)
	    continue;
	String type = tmp.substr(0,sep);
	tmp >> " ";
	if (media && (type != media))
	    continue;
        int port = 0;
	tmp >> port >> " RTP/AVP";
	String fmt;
	bool defcodecs = s_cfg.getBoolValue("codecs","default",true);
	int ptime = 0;
	while (tmp[0] == ' ') {
	    int var = -1;
	    tmp >> " " >> var;
	    int mode = 0;
	    String payload(lookup(var,dict_payloads));

	    const ObjList* l = sdp->lines().find(c);
	    while (l && (l = l->skipNext())) {
		const NamedString* s = static_cast<NamedString*>(l->get());
		if (s->name() == "m")
		    break;
		if (s->name() != "a")
		    continue;
		String line(*s);
		if (line.startSkip("ptime:",false))
		    line >> ptime;
		else if (line.startSkip("rtpmap:",false)) {
		    int num = -1;
		    line >> num >> " ";
		    if (num == var) {
			for (const TokenDict* map = dict_rtpmap; map->token; map++) {
			    if (line.startsWith(map->token,false,true)) {
				const char* pload = lookup(map->value,dict_payloads);
				if (pload)
				    payload = pload;
				break;
			    }
			}
		    }
		}
		else if (line.startSkip("fmtp:",false)) {
		    int num = -1;
		    line >> num >> " ";
		    if (num == var) {
			if (line.startSkip("mode=",false))
			    line >> mode;
		    }
		}
	    }

	    if (payload == "ilbc") {
		if ((mode == 20) || (ptime == 20))
		    payload = "ilbc20";
		else if ((mode == 30) || (ptime == 30))
		    payload = "ilbc30";
		else
		    payload = s_cfg.getValue("hacks","ilbc_default","ilbc30");
	    }

	    XDebug(&plugin,DebugAll,"Payload %d format '%s'",var,payload.c_str());
	    if (payload && s_cfg.getBoolValue("codecs",payload,defcodecs && DataTranslator::canConvert(payload))) {
		if (fmt)
		    fmt << ",";
		fmt << payload;
	    }
	}
	RtpMedia* rtp = 0;
	// try to take the media descriptor from the old list
	if (oldMedia) {
	    ObjList* om = oldMedia->find(type);
	    if (om)
		rtp = static_cast<RtpMedia*>(om->remove(false));
	}
	if (rtp)
	    rtp->update(fmt,port);
	else
	    rtp = new RtpMedia(type,fmt,port);
	if (!lst)
	    lst = new ObjList;
	lst->append(rtp);
	if (media)
	    return lst;
    }
    return lst;
}

// Check if an IPv4 address belongs to one of the non-routable blocks
static bool isPrivateAddr(const String& host)
{
    if (host.startsWith("192.168.") || host.startsWith("169.254.") || host.startsWith("10."))
	return true;
    String s(host);
    if (!s.startSkip("172.",false))
	return false;
    int i = 0;
    s >> i;
    return (i >= 16) && (i <= 31) && s.startsWith(".");
}

// Check if there may be a NAT between an address embedded in the protocol
//  and an address obtained from the network layer
static bool isNatBetween(const String& embAddr, const String& netAddr)
{
    return isPrivateAddr(embAddr) && !isPrivateAddr(netAddr);
}

// List of critical headers we don't want to handle generically
static const char* rejectHeaders[] = {
    "via",
    "route",
    "record-route",
    "call-id",
    "cseq",
    "from",
    "to",
    "max-forwards",
    "content-length",
    "www-authenticate",
    "proxy-authenticate",
    "authorization",
    "proxy-authorization",
    0
};

// Copy headers from SIP message to Yate message
static void copySipHeaders(Message& msg, const SIPMessage& sip)
{
    const ObjList* l = sip.header.skipNull();
    for (; l; l = l->skipNext()) {
	const SIPHeaderLine* t = static_cast<const SIPHeaderLine*>(l->get());
	String name(t->name());
	name.toLower();
	const char** hdr = rejectHeaders;
	for (; *hdr; hdr++)
	    if (name == *hdr)
		break;
	if (*hdr)
	    continue;
	String tmp(*t);
	const ObjList* p = t->params().skipNull();
	for (; p; p = p->skipNext()) {
	    NamedString* s = static_cast<NamedString*>(p->get());
	    tmp << ";" << s->name();
	    if (!s->null())
		tmp << "=" << *s;
	}
	msg.addParam("sip_"+name,tmp);
    }
}

// Copy headers from Yate message to SIP message
static void copySipHeaders(SIPMessage& sip, const Message& msg, const char* prefix = "sip_")
{
    unsigned int n = msg.length();
    for (unsigned int i = 0; i < n; i++) {
	NamedString* str = msg.getParam(i);
	if (!str)
	    continue;
	String name(str->name());
	if (!name.startSkip(prefix,false))
	    continue;
	if (name.trimBlanks().null())
	    continue;
	sip.addHeader(name,*str);
    }
}

// Copy privacy related information from SIP message to Yate message
static void copyPrivacy(Message& msg, const SIPMessage& sip)
{
    bool anonip = (sip.getHeaderValue("Anonymity") &= "ipaddr");
    const SIPHeaderLine* hl = sip.getHeader("Remote-Party-ID");
    if (!(anonip || hl))
	return;
    const NamedString* p = hl ? hl->getParam("screen") : 0;
    if (p)
	msg.setParam("screened",*p);
    String priv;
    if (anonip)
	priv.append("addr",",");
    p = hl ? hl->getParam("privacy") : 0;
    if (p) {
	if ((*p &= "full") || (*p &= "full-network"))
	    priv.append("name,uri",",");
	else if ((*p &= "name") || (*p &= "name-network"))
	    priv.append("name",",");
	else if ((*p &= "uri") || (*p &= "uri-network"))
	    priv.append("uri",",");
    }
    if (priv)
	msg.setParam("privacy",priv);
}

// Copy privacy related information from Yate message to SIP message
static void copyPrivacy(SIPMessage& sip, const Message& msg)
{
    String screened(msg.getValue("screened"));
    String privacy(msg.getValue("privacy"));
    if (screened.null() && privacy.null())
	return;
    bool screen = screened.toBoolean();
    bool anonip = (privacy.find("addr") >= 0);
    bool privname = (privacy.find("name") >= 0);
    bool privuri = (privacy.find("uri") >= 0);
    // allow for a simple "privacy=yes" or similar
    if (privacy.toBoolean(false))
	privname = privuri = true;
    if (anonip)
	sip.setHeader("Anonymity","ipaddr");
    if (screen || privname || privuri) {
	const char* caller = msg.getValue("caller","anonymous");
	String tmp;
	tmp << "\"" << msg.getValue("callername",caller) << "\"";
	tmp << " <" << caller << "@" << msg.getValue("domain","domain") << ">";
	SIPHeaderLine* hl = new SIPHeaderLine("Remote-Party-ID",tmp);
	if (screen)
	    hl->setParam("screen","yes");
	if (privname && privuri)
	    hl->setParam("privacy","full");
	else if (privname)
	    hl->setParam("privacy","name");
	else if (privuri)
	    hl->setParam("privacy","uri");
	else
	    hl->setParam("privacy","none");
	sip.addHeader(hl);
    }
}

RtpMedia::RtpMedia(const char* media, const char* formats, int rport, int lport)
    : String(media), m_audio(true), m_formats(formats)
{
    DDebug(&plugin,DebugAll,"RtpMedia::RtpMedia('%s','%s',%d,%d) [%p]",
	media,formats,rport,lport,this);
    if (operator!=("audio")) {
	m_audio = false;
	m_suffix << "_" << media;
    }
    int q = m_formats.find(',');
    m_format = m_formats.substr(0,q);
    if (rport >= 0)
	m_rPort = rport;
    if (lport >= 0)
	m_lPort = lport;
}

RtpMedia::~RtpMedia()
{
    DDebug(&plugin,DebugAll,"RtpMedia::~RtpMedia() '%s' [%p]",c_str(),this);
}

const char* RtpMedia::fmtList() const
{
    if (m_formats)
	return m_formats.c_str();
    if (m_format)
	return m_format.c_str();
    // unspecified audio assumed to support G711
    if (m_audio)
	return "alaw,mulaw";
    return 0;
}

// Update members with data taken from a SDP, return true if something changed
bool RtpMedia::update(const char* formats, int rport, int lport)
{
    DDebug(&plugin,DebugAll,"RtpMedia::update('%s',%d,%d) [%p]",
	formats,rport,lport,this);
    bool chg = false;
    String tmp(formats);
    if (m_formats != tmp) {
	chg = true;
	m_formats = tmp;
	int q = m_formats.find(',');
	m_format = m_formats.substr(0,q);
    }
    if (rport >= 0) {
	tmp = rport;
	if (m_rPort != tmp) {
	    chg = true;
	    m_rPort = tmp;
	}
    }
    if (lport >= 0) {
	tmp = lport;
	if (m_lPort != tmp) {
	    chg = true;
	    m_lPort = tmp;
	}
    }
    return chg;
}

// Update members from a dispatched "chan.rtp" message
void RtpMedia::update(const Message& msg, bool pickFormat)
{
    m_id = msg.getValue("rtpid",m_id);
    m_lPort = msg.getValue("localport",m_lPort);
    if (pickFormat)
	m_format = msg.getValue("format");
}

YateUDPParty::YateUDPParty(Socket* sock, const SocketAddr& addr, int localPort, const char* localAddr)
    : m_sock(sock), m_addr(addr)
{
    DDebug(&plugin,DebugAll,"YateUDPParty::YateUDPParty() %s:%d [%p]",localAddr,localPort,this);
    m_localPort = localPort;
    m_party = m_addr.host();
    m_partyPort = m_addr.port();
    if (localAddr)
	m_local = localAddr;
    else {
	SocketAddr laddr;
	if (laddr.local(addr))
	    m_local = laddr.host();
	else
	    m_local = "localhost";
    }
    DDebug(&plugin,DebugAll,"YateUDPParty local %s:%d party %s:%d",
	m_local.c_str(),m_localPort,
	m_party.c_str(),m_partyPort);
}

YateUDPParty::~YateUDPParty()
{
    DDebug(&plugin,DebugAll,"YateUDPParty::~YateUDPParty() [%p]",this);
    m_sock = 0;
}

void YateUDPParty::transmit(SIPEvent* event)
{
    const SIPMessage* msg = event->getMessage();
    if (!msg)
	return;
    String tmp;
    if (msg->isAnswer())
	tmp << "code " << msg->code;
    else
	tmp << "'" << msg->method << " " << msg->uri << "'";
    if (plugin.debugAt(DebugInfo)) {
	String buf((char*)msg->getBuffer().data(),msg->getBuffer().length());
	Debug(&plugin,DebugInfo,"Sending %s %p to %s:%d\n------\n%s------",
	    tmp.c_str(),msg,m_addr.host().c_str(),m_addr.port(),buf.c_str());
    }
    m_sock->sendTo(
	msg->getBuffer().data(),
	msg->getBuffer().length(),
	m_addr
    );
}

const char* YateUDPParty::getProtoName() const
{
    return "UDP";
}

bool YateUDPParty::setParty(const URI& uri)
{
    if (m_partyPort && m_party && s_cfg.getBoolValue("general","ignorevia",true))
	return true;
    if (uri.getHost().null())
	return false;
    int port = uri.getPort();
    if (port <= 0)
	port = 5060;
    if (!m_addr.host(uri.getHost())) {
	Debug(&plugin,DebugWarn,"Could not resolve UDP party name '%s' [%p]",
	    uri.getHost().safe(),this);
	return false;
    }
    m_addr.port(port);
    m_party = uri.getHost();
    m_partyPort = port;
    DDebug(&plugin,DebugInfo,"New UDP party is %s:%d (%s:%d) [%p]",
	m_party.c_str(),m_partyPort,
	m_addr.host().c_str(),m_addr.port(),
	this);
    return true;
}

YateSIPEngine::YateSIPEngine(YateSIPEndPoint* ep)
    : SIPEngine(s_cfg.getValue("general","useragent")),
      m_ep(ep), m_prack(false), m_info(false)
{
    addAllowed("INVITE");
    addAllowed("BYE");
    addAllowed("CANCEL");
    if (s_cfg.getBoolValue("general","registrar",true))
	addAllowed("REGISTER");
    if (s_cfg.getBoolValue("general","transfer",true))
	addAllowed("REFER");
    if (s_cfg.getBoolValue("general","options",true))
	addAllowed("OPTIONS");
    m_prack = s_cfg.getBoolValue("general","prack");
    if (m_prack)
	addAllowed("PRACK");
    m_info = s_cfg.getBoolValue("general","info",true);
    if (m_info)
	addAllowed("INFO");
    NamedList *l = s_cfg.getSection("methods");
    if (l) {
	unsigned int len = l->length();
	for (unsigned int i=0; i<len; i++) {
	    NamedString *n = l->getParam(i);
	    if (!n)
		continue;
	    String meth(n->name());
	    meth.toUpper();
	    addAllowed(meth);
	}
    }
}

bool YateSIPEngine::buildParty(SIPMessage* message)
{
    return m_ep->buildParty(message);
}

bool YateSIPEngine::copyAuthParams(NamedList* dest, const NamedList& src)
{
    // we added those and we want to exclude them from copy
    static TokenDict exclude[] = {
	{ "protocol", 1 },
	// purposely copy the username and realm
	{ "nonce", 1 },
	{ "method", 1 },
	{ "uri", 1 },
	{ "response", 1 },
	{ "ip_host", 1 },
	{ "ip_port", 1 },
	{ "address", 1 },
	{  0,   0 },
    };
    if (!dest)
	return true;
    unsigned int n = src.length();
    for (unsigned int i = 0; i < n; i++) {
	NamedString* s = src.getParam(i);
	if (!s)
	    continue;
	if (s->name().toInteger(exclude,0))
	    continue;
	dest->setParam(s->name(),*s);
    }
    return true;
}

bool YateSIPEngine::checkUser(const String& username, const String& realm, const String& nonce,
    const String& method, const String& uri, const String& response,
    const SIPMessage* message, GenObject* userData)
{
    NamedList* params = YOBJECT(NamedList,userData);

    Message m("user.auth");
    m.addParam("protocol","sip");
    if (username) {
	m.addParam("username",username);
	m.addParam("realm",realm);
	m.addParam("nonce",nonce);
	m.addParam("response",response);
    }
    m.addParam("method",method);
    m.addParam("uri",uri);
    if (message) {
	m.addParam("ip_host",message->getParty()->getPartyAddr());
	m.addParam("ip_port",String(message->getParty()->getPartyPort()));
	String addr = message->getParty()->getPartyAddr();
	if (addr) {
	    addr << ":" << message->getParty()->getPartyPort();
	    m.addParam("address",addr);
	}
    }

    if (params) {
	const char* str = params->getValue("caller");
	if (str)
	    m.addParam("caller",str);
	str = params->getValue("called");
	if (str)
	    m.addParam("called",str);
    }

    if (!Engine::dispatch(m))
	return false;

    // empty password returned means authentication succeeded
    if (m.retValue().null())
	return copyAuthParams(params,m);
    // check for refusals
    if (m.retValue() == "-") {
	if (params) {
	    const char* err = m.getValue("error");
	    if (err)
		params->setParam("error",err);
	    err = m.getValue("reason");
	    if (err)
		params->setParam("reason",err);
	}
	return false;
    }
    // password works only with username
    if (!username)
	return false;

    String res;
    buildAuth(username,realm,m.retValue(),nonce,method,uri,res);
    if (res == response)
	return copyAuthParams(params,m);
    // if the URI included some parameters retry after stripping them off
    int sc = uri.find(';');
    if (sc < 0)
	return false;
    buildAuth(username,realm,m.retValue(),nonce,method,uri.substr(0,sc),res);
    return (res == response) && copyAuthParams(params,m);
}

YateSIPEndPoint::YateSIPEndPoint()
    : Thread("YSIP EndPoint"), m_sock(0), m_engine(0)
{
    Debug(&plugin,DebugAll,"YateSIPEndPoint::YateSIPEndPoint() [%p]",this);
}

YateSIPEndPoint::~YateSIPEndPoint()
{
    Debug(&plugin,DebugAll,"YateSIPEndPoint::~YateSIPEndPoint() [%p]",this);
    plugin.channels().clear();
    s_lines.clear();
    if (m_engine) {
	// send any pending events
	while (m_engine->process())
	    ;
	delete m_engine;
	m_engine = 0;
    }
    if (m_sock) {
	delete m_sock;
	m_sock = 0;
    }
}

bool YateSIPEndPoint::buildParty(SIPMessage* message, const char* host, int port, const YateSIPLine* line)
{
    if (message->isAnswer())
	return false;
    DDebug(&plugin,DebugAll,"YateSIPEndPoint::buildParty(%p,'%s',%d,%p)",
	message,host,port,line);
    URI uri(message->uri);
    if (line) {
	if (!host)
	    host = line->getPartyAddr();
	if (port <= 0)
	    port = line->getPartyPort();
	line->setupAuth(message);
    }
    if (!host) {
	host = uri.getHost().safe();
	if (port <= 0)
	    port = uri.getPort();
    }
    if (port <= 0)
	port = 5060;
    SocketAddr addr(AF_INET);
    if (!addr.host(host)) {
	Debug(&plugin,DebugWarn,"Error resolving name '%s'",host);
	return false;
    }
    addr.port(port);
    DDebug(&plugin,DebugAll,"built addr: %s:%d",
	addr.host().c_str(),addr.port());
    // reuse the variables now we finished with them
    host = line ? line->getLocalAddr().c_str() : (const char*)0;
    port = line ? line->getLocalPort() : 0;
    if (!host)
	host = m_local;
    if (port <= 0)
	port = m_port;
    YateUDPParty* party = new YateUDPParty(m_sock,addr,port,host);
    message->setParty(party);
    party->deref();
    return true;
}

bool YateSIPEndPoint::Init()
{
    if (m_sock) {
	Debug(&plugin,DebugInfo,"Already initialized.");
	return true;
    }

    m_sock = new Socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (!m_sock->valid()) {
	Debug(&plugin,DebugGoOn,"Unable to allocate UDP socket");
	return false;
    }
    
    SocketAddr addr(AF_INET);
    addr.port(s_cfg.getIntValue("general","port",5060));
    addr.host(s_cfg.getValue("general","addr","0.0.0.0"));

    if (!m_sock->bind(addr)) {
	Debug(&plugin,DebugWarn,"Unable to bind to preferred port - using random one instead");
	addr.port(0);
	if (!m_sock->bind(addr)) {
	    Debug(&plugin,DebugGoOn,"Unable to bind to any port");
	    return false;
	}
    }
    
    if (!m_sock->getSockName(addr)) {
	Debug(&plugin,DebugGoOn,"Unable to figure out what I'm bound to");
	return false;
    }
    if (!m_sock->setBlocking(false)) {
	Debug(&plugin,DebugGoOn,"Unable to set non-blocking mode");
	return false;
    }
    Debug(&plugin,DebugCall,"Started on %s:%d", addr.host().safe(), addr.port());
    if (addr.host() != "0.0.0.0")
	m_local = addr.host();
    m_port = addr.port();
    m_engine = new YateSIPEngine(this);
    return true;
}

void YateSIPEndPoint::addMessage(const char* buf, int len, const SocketAddr& addr, int port)
{
    SIPMessage* msg = SIPMessage::fromParsing(0,buf,len);
    if (!msg)
	return;

    if (!msg->isAnswer()) {
	URI uri(msg->uri);
	YateSIPLine* line = plugin.findLine(addr.host(),addr.port(),uri.getUser());
	const char* host = 0;
	if (line && line->getLocalPort()) {
	    host = line->getLocalAddr();
	    port = line->getLocalPort();
	}
	YateUDPParty* party = new YateUDPParty(m_sock,addr,port,host);
	msg->setParty(party);
	party->deref();
    }
    m_engine->addMessage(msg);
    msg->deref();
}

void YateSIPEndPoint::run()
{
    struct timeval tv;
    char buf[1500];
    /* Watch stdin (fd 0) to see when it has input. */
    for (;;)
    {
	/* Wait up to 5000 microseconds. */
	tv.tv_sec = 0;
	tv.tv_usec = 5000;
	bool ok = false;
	m_sock->select(&ok,0,0,&tv);
	if (ok)
	{
	    // we can read the data
	    int res = m_sock->recvFrom(buf,sizeof(buf)-1,m_addr);
	    if (res <= 0) {
		if (!m_sock->canRetry()) {
		    Debug(&plugin,DebugGoOn,"Error on read: %d", m_sock->error());
		}
	    } else if (res >= 72) {
		buf[res]=0;
		Debug(&plugin,DebugInfo,"Received %d bytes SIP message from %s:%d\n------\n%s------",
		    res,m_addr.host().c_str(),m_addr.port(),buf);
		// we got already the buffer and here we start to do "good" stuff
		addMessage(buf,res,m_addr,m_port);
		//m_engine->addMessage(new YateUDPParty(m_sock,m_addr,m_port),buf,res);
	    }
#ifdef DEBUG
	    else
		Debug(&plugin,DebugInfo,"Received short SIP message of %d bytes",res);
#endif
	}
	else
	    Thread::check();
	SIPEvent* e = m_engine->getEvent();
	// hack: use a loop so we can use break and continue
	for (; e; m_engine->processEvent(e),e = 0) {
	    if (!e->getTransaction())
		continue;
	    plugin.lock();
	    GenObject* obj = static_cast<GenObject*>(e->getTransaction()->getUserData());
	    RefPointer<YateSIPConnection> conn = YOBJECT(YateSIPConnection,obj);
	    YateSIPLine* line = YOBJECT(YateSIPLine,obj);
	    YateSIPGenerate* gen = YOBJECT(YateSIPGenerate,obj);
	    plugin.unlock();
	    if (conn) {
		if (conn->process(e)) {
		    delete e;
		    break;
		}
		else
		    continue;
	    }
	    if (line) {
		if (line->process(e)) {
		    delete e;
		    break;
		}
		else
		    continue;
	    }
	    if (gen) {
		if (gen->process(e)) {
		    delete e;
		    break;
		}
		else
		    continue;
	    }
	    if ((e->getState() == SIPTransaction::Trying) &&
		!e->isOutgoing() && incoming(e,e->getTransaction())) {
		delete e;
		break;
	    }
	}
    }
}

bool YateSIPEndPoint::incoming(SIPEvent* e, SIPTransaction* t)
{
    if (t->isInvite())
	invite(e,t);
    else if (t->getMethod() == "BYE") {
	YateSIPConnection* conn = plugin.findCall(t->getCallID());
	if (conn)
	    conn->doBye(t);
	else
	    t->setResponse(481);
    }
    else if (t->getMethod() == "CANCEL") {
	YateSIPConnection* conn = plugin.findCall(t->getCallID());
	if (conn)
	    conn->doCancel(t);
	else
	    t->setResponse(481);
    }
    else if (t->getMethod() == "INFO") {
	YateSIPConnection* conn = plugin.findCall(t->getCallID());
	if (conn)
	    conn->doInfo(t);
	else
	    t->setResponse(481);
    }
    else if (t->getMethod() == "REGISTER")
	regreq(e,t);
    else if (t->getMethod() == "OPTIONS")
	options(e,t);
    else if (t->getMethod() == "REFER") {
	YateSIPConnection* conn = plugin.findCall(t->getCallID());
	if (conn)
	    conn->doRefer(t);
	else
	    t->setResponse(481);
    }
    else
	return generic(e,t);
    return true;
}

void YateSIPEndPoint::invite(SIPEvent* e, SIPTransaction* t)
{
    if (!plugin.canAccept()) {
	Debug(&plugin,DebugWarn,"Refusing new SIP call, full or exiting");
	t->setResponse(480);
	return;
    }

    if (e->getMessage()->getParam("To","tag")) {
	SIPDialog dlg(*e->getMessage());
	YateSIPConnection* conn = plugin.findDialog(dlg);
	if (conn)
	    conn->reInvite(t);
	else {
	    Debug(&plugin,DebugWarn,"Got re-INVITE for missing dialog");
	    t->setResponse(481);
	}
	return;
    }

    YateSIPConnection* conn = new YateSIPConnection(e,t);
    conn->startRouter();

}

void YateSIPEndPoint::regreq(SIPEvent* e, SIPTransaction* t)
{
    if (Engine::exiting()) {
	Debug(&plugin,DebugWarn,"Dropping request, engine is exiting");
	t->setResponse(500, "Server Shutting Down");
	return;
    }
    const SIPMessage* message = e->getMessage();
    const SIPHeaderLine* hl = message->getHeader("Contact");
    if (!hl) {
	t->setResponse(400);
	return;
    }

    Message msg("user.register");
    String user;
    int age = t->authUser(user,false,&msg);
    DDebug(&plugin,DebugAll,"User '%s' age %d",user.c_str(),age);
    if (((age < 0) || (age > 10)) && s_auth_register) {
	t->requestAuth(s_realm,"",age >= 0);
	return;
    }

    // TODO: track registrations, allow deregistering all
    if (*hl == "*") {
	t->setResponse(200);
	return;
    }

    URI addr(*hl);
    msg.setParam("username",user);
    msg.setParam("number",addr.getUser());
    msg.setParam("driver","sip");
    String data("sip/" + addr);
    bool nat = isNatBetween(addr.getHost(),message->getParty()->getPartyAddr());
    if (msg.getBoolValue("nat_support",s_auto_nat && nat)) {
	Debug(&plugin,DebugInfo,"Registration NAT detected: private '%s:%d' public '%s:%d'",
		    addr.getHost().c_str(),addr.getPort(),
		    message->getParty()->getPartyAddr().c_str(),
		    message->getParty()->getPartyPort());
	String tmp(addr.getHost());
	tmp << ":" << addr.getPort();
	msg.addParam("reg_nat_addr",tmp);
	int pos = data.find(tmp);
	if (pos >= 0) {
	    int len = tmp.length();
	    tmp.clear();
	    tmp << data.substr(0,pos) << message->getParty()->getPartyAddr()
		<< ":" << message->getParty()->getPartyPort() << data.substr(pos + len);
	    data = tmp;
	}
    }
    msg.setParam("data",data);
    msg.setParam("ip_host",message->getParty()->getPartyAddr());
    msg.setParam("ip_port",String(message->getParty()->getPartyPort()));

    bool dereg = false;
    String tmp(message->getHeader("Expires"));
    int expires = tmp.toInteger(-1);
    if (expires < 0)
	expires = s_expires_def;
    if (expires > s_expires_max)
	expires = s_expires_max;
    if (expires && (expires < s_expires_min)) {
	tmp = s_expires_min;
	SIPMessage* r = new SIPMessage(t->initialMessage(),423);
	r->addHeader("Min-Expires",tmp);
	t->setResponse(r);
	r->deref();
	return;
    }
    tmp = expires;
    msg.setParam("expires",tmp);
    if (!expires) {
	msg = "user.unregister";
	dereg = true;
    }
    hl = message->getHeader("User-Agent");
    if (hl)
	msg.setParam("device",*hl);
    // Always OK deregistration attempts
    if (Engine::dispatch(msg) || dereg) {
	if (dereg) {
	    t->setResponse(200);
	    Debug(&plugin,DebugNote,"Unregistered user '%s'",user.c_str());
	}
	else {
	    tmp = msg.getValue("expires",tmp);
	    if (tmp.null())
		tmp = expires;
	    SIPMessage* r = new SIPMessage(t->initialMessage(),200);
	    r->addHeader("Expires",tmp);
	    t->setResponse(r);
	    r->deref();
	    Debug(&plugin,DebugNote,"Registered user '%s' expires in %s s",
		user.c_str(),tmp.c_str());
	}
    }
    else
	t->setResponse(404);
}

void YateSIPEndPoint::options(SIPEvent* e, SIPTransaction* t)
{
    const SIPHeaderLine* acpt = e->getMessage()->getHeader("Accept");
    if (acpt) {
	if (*acpt != "application/sdp") {
	    t->setResponse(415);
	    return;
	}
    }
    t->setResponse(200);
}

bool YateSIPEndPoint::generic(SIPEvent* e, SIPTransaction* t)
{
    String meth(t->getMethod());
    meth.toLower();
    String user;
    if (s_cfg.getBoolValue("methods",meth,true)) {
	int age = t->authUser(user);
	DDebug(&plugin,DebugAll,"User '%s' age %d",user.c_str(),age);
	if ((age < 0) || (age > 10)) {
	    t->requestAuth(s_realm,"",age >= 0);
	    return true;
	}
    }

    const SIPMessage* message = e->getMessage();
    Message m("sip." + meth);
    if (message->getParam("To","tag")) {
	SIPDialog dlg(*message);
	YateSIPConnection* conn = plugin.findDialog(dlg);
	if (conn) {
	    m.userData(conn);
	    conn->complete(m);
	}
    }
    if (user)
	m.addParam("username",user);
    m.addParam("ip_host",message->getParty()->getPartyAddr());
    m.addParam("ip_port",String(message->getParty()->getPartyPort()));
    m.addParam("sip_uri",t->getURI());
    m.addParam("sip_callid",t->getCallID());
    // establish the dialog here so user code will have the dialog tag handy
    t->setDialogTag();
    m.addParam("xsip_dlgtag",t->getDialogTag());
    copySipHeaders(m,*message);

    if (Engine::dispatch(m)) {
	t->setResponse(m.getIntValue("code",200));
	return true;
    }
    return false;
}

// transferorID: Channel id of the sip connection that received the REFER request
// transferredID: Channel id of the transferor's peer
// transferredDrv: Channel driver of the transferor's peer
// msg: already populated 'call.route'
// sipNotify: already populated SIPMessage("NOTIFY")
YateSIPRefer::YateSIPRefer(const String& transferorID, const String& transferredID,
			   Driver* transferredDrv, Message* msg, SIPMessage* sipNotify)
    : Thread("SIP Transfer"), m_transferorID(transferorID), m_transferredID(transferredID),
      m_transferredDrv(transferredDrv), m_msg(msg), m_sipNotify(sipNotify)
{
}

void YateSIPRefer::run()
{
    bool ok = false;
    if (m_transferredDrv && m_msg)
	ok = route();
    // Send response
    String s(ok ? "SIP/2.0 200 OK\r\n" : "SIP/2.0 603 Declined\r\n");
    m_sipNotify->setBody(new SIPStringBody("message/sipfrag;version=2.0",s));
    plugin.ep()->engine()->addMessage(m_sipNotify);
    // Notify termination to transferor
    plugin.lock();
    YateSIPConnection* conn = static_cast<YateSIPConnection*>(plugin.find(m_transferorID));
    if (conn)
	conn->referTerminated();
    plugin.unlock();
}

bool YateSIPRefer::route()
{
    DDebug(&plugin,DebugAll,"%s thread ('%s') [%p]. Transferring to '%s'",name(),m_transferredID.c_str(),this,m_msg->getValue("called"));
    RefPointer<Channel> chan;
    // Route the call
    bool ok = Engine::dispatch(m_msg);
    m_transferredDrv->lock();
    chan = m_transferredDrv->find(m_transferredID);
    m_transferredDrv->unlock();
    if (!chan) {
	DDebug(&plugin,DebugAll,"%s thread ('%s') [%p]. Connection vanished while routing!",name(),m_transferredID.c_str(),this);
	return false;
    }
    m_msg->userData(chan);
    if (ok) {
	DDebug(&plugin,DebugAll,"%s thread ('%s') [%p]. Call succesfully routed.",name(),m_transferredID.c_str(),this);
	if ((m_msg->retValue() == "-") || (m_msg->retValue() == "error"))
	    m_msg->setParam("reason","unknown");
	else if (m_msg->getIntValue("antiloop",1) <= 0)
	    m_msg->setParam("reason","Call is looping");
	else {
	    *m_msg = "call.execute";
	    m_msg->setParam("callto",m_msg->retValue());
	    m_msg->clearParam("error");
	    m_msg->retValue().clear();
	    // Execute the call
	    ok = Engine::dispatch(m_msg);
	    DDebug(&plugin,DebugAll,"%s thread ('%s') [%p]. 'call.execute' %s.",
		name(),m_transferredID.c_str(),this,ok ? "succeeded" : "failed");
	}
    }
    else
	DDebug(&plugin,DebugAll,"%s thread ('%s') [%p]. 'call.route' failed.",
	    name(),m_transferredID.c_str(),this);
    return ok;
}

void YateSIPRefer::cleanup()
{
    delete m_msg;
}

// Incoming call constructor - just before starting the routing thread
YateSIPConnection::YateSIPConnection(SIPEvent* ev, SIPTransaction* tr)
    : Channel(plugin,0,false),
      m_tr(tr), m_tr2(0), m_hungup(false), m_byebye(true), m_cancel(false),
      m_state(Incoming), m_rtpForward(false), m_sdpForward(false), m_rtpMedia(0),
      m_sdpSession(0), m_sdpVersion(0), m_port(0), m_route(0), m_routes(0),
      m_authBye(true), m_mediaStatus(MediaMissing), m_inband(s_inband), m_info(s_info),
      m_referring(false)
{
    Debug(this,DebugAll,"YateSIPConnection::YateSIPConnection(%p,%p) [%p]",ev,tr,this);
    setReason();
    m_tr->ref();
    m_routes = m_tr->initialMessage()->getRoutes();
    m_callid = m_tr->getCallID();
    m_dialog = *m_tr->initialMessage();
    m_host = m_tr->initialMessage()->getParty()->getPartyAddr();
    m_port = m_tr->initialMessage()->getParty()->getPartyPort();
    m_address << m_host << ":" << m_port;
    filterDebug(m_address);
    m_uri = m_tr->initialMessage()->getHeader("From");
    m_uri.parse();
    m_tr->setUserData(this);

    URI uri(m_tr->getURI());
    YateSIPLine* line = plugin.findLine(m_host,m_port,m_uri.getUser());
    Message *m = message("call.preroute");
    m->addParam("caller",m_uri.getUser());
    m->addParam("called",uri.getUser());
    if (m_uri.getDescription())
	m->addParam("callername",m_uri.getDescription());
    const SIPHeaderLine* hl = m_tr->initialMessage()->getHeader("Call-Info");
    if (hl) {
	const NamedString* type = hl->getParam("purpose");
	if (!type || *type == "info")
	    m->addParam("caller_info_uri",*type);
	else if (*type == "icon")
	    m->addParam("caller_icon_uri",*type);
	else if (*type == "card")
	    m->addParam("caller_card_uri",*type);
    }

    if (line) {
	// call comes from line we have registered to - trust it...
	m_user = line->getUserName();
	m_externalAddr = line->getLocalAddr();
	m_line = *line;
	m->addParam("username",m_user);
	m->addParam("in_line",m_line);
    }
    else {
	String user;
	int age = tr->authUser(user,false,m);
	DDebug(this,DebugAll,"User '%s' age %d",user.c_str(),age);
	if (age >= 0) {
	    if (age < 10) {
		m_user = user;
		m->addParam("username",m_user);
	    }
	    else
		m->addParam("expired_user",user);
	    m->addParam("xsip_nonce_age",String(age));
	}
    }
    if (s_privacy)
	copyPrivacy(*m,*ev->getMessage());

    String tmp(ev->getMessage()->getHeaderValue("Max-Forwards"));
    int maxf = tmp.toInteger(s_maxForwards);
    if (maxf > s_maxForwards)
	maxf = s_maxForwards;
    tmp = maxf-1;
    m->addParam("antiloop",tmp);
    m->addParam("ip_host",m_host);
    m->addParam("ip_port",String(m_port));
    m->addParam("sip_uri",uri);
    m->addParam("sip_from",m_uri);
    m->addParam("sip_to",ev->getMessage()->getHeaderValue("To"));
    m->addParam("sip_callid",m_callid);
    m->addParam("device",ev->getMessage()->getHeaderValue("User-Agent"));
    copySipHeaders(*m,*ev->getMessage());
    if (ev->getMessage()->body && ev->getMessage()->body->isSDP()) {
	setMedia(parseSDP(static_cast<SDPBody*>(ev->getMessage()->body),m_rtpAddr,m_rtpMedia));
	if (m_rtpMedia) {
	    m_rtpForward = true;
	    // guess if the call comes from behind a NAT
	    bool nat = isNatBetween(m_rtpAddr,m_host);
	    if (m->getBoolValue("nat_support",s_auto_nat && nat)) {
		Debug(this,DebugInfo,"RTP NAT detected: private '%s' public '%s'",
		    m_rtpAddr.c_str(),m_host.c_str());
		m->addParam("rtp_nat_addr",m_rtpAddr);
		m_rtpAddr = m_host;
	    }
	    m->addParam("rtp_addr",m_rtpAddr);
	    ObjList* l = m_rtpMedia->skipNull();
	    for (; l; l = l->skipNext()) {
		RtpMedia* r = static_cast<RtpMedia*>(l->get());
		m->addParam("media"+r->suffix(),"yes");
		m->addParam("rtp_port"+r->suffix(),r->remotePort());
		m->addParam("formats"+r->suffix(),r->formats());
	    }
	}
	if (s_forward_sdp) {
	    const DataBlock& raw = ev->getMessage()->body->getBody();
	    String tmp((const char*)raw.data(),raw.length());
	    m->addParam("sdp_raw",tmp);
	    m_rtpForward = true;
	}
	if (m_rtpForward)
	    m->addParam("rtp_forward","possible");
    }
    DDebug(this,DebugAll,"RTP addr '%s' [%p]",m_rtpAddr.c_str(),this);
    m_route = m;
    Message* s = message("chan.startup");
    s->addParam("caller",m_uri.getUser());
    s->addParam("called",uri.getUser());
    if (m_user)
	s->addParam("username",m_user);
    Engine::enqueue(s);
}

// Outgoing call constructor - in call.execute handler
YateSIPConnection::YateSIPConnection(Message& msg, const String& uri, const char* target)
    : Channel(plugin,0,true),
      m_tr(0), m_tr2(0), m_hungup(false), m_byebye(true), m_cancel(true),
      m_state(Outgoing), m_rtpForward(false), m_sdpForward(false), m_rtpMedia(0),
      m_sdpSession(0), m_sdpVersion(0), m_port(0), m_route(0), m_routes(0),
      m_authBye(false), m_mediaStatus(MediaMissing), m_inband(s_inband), m_info(s_info),
      m_referring(false)
{
    Debug(this,DebugAll,"YateSIPConnection::YateSIPConnection(%p,'%s') [%p]",
	&msg,uri.c_str(),this);
    m_targetid = target;
    setReason();
    m_inband = msg.getBoolValue("dtmfinband",s_inband);
    m_info = msg.getBoolValue("dtmfinfo",s_info);
    m_rtpForward = msg.getBoolValue("rtp_forward");
    m_line = msg.getValue("line");
    String tmp;
    YateSIPLine* line = 0;
    if (m_line) {
	line = plugin.findLine(m_line);
	if (line && (uri.find('@') < 0)) {
	    if (!uri.startsWith("sip:"))
		tmp = "sip:";
	    tmp << uri << "@" << line->domain();
	}
	if (line)
	    m_externalAddr = line->getLocalAddr();
    }
    if (tmp.null())
	tmp = uri;
    m_uri = tmp;
    m_uri.parse();
    SIPMessage* m = new SIPMessage("INVITE",m_uri);
    plugin.ep()->buildParty(m,msg.getValue("host"),msg.getIntValue("port"),line);
    if (!m->getParty()) {
	Debug(this,DebugWarn,"Could not create party for '%s' [%p]",m_uri.c_str(),this);
	m->destruct();
	tmp = "Invalid address: ";
	tmp << m_uri;
	msg.setParam("reason",tmp);
	setReason(tmp);
	return;
    }
    int maxf = msg.getIntValue("antiloop",s_maxForwards);
    m->addHeader("Max-Forwards",String(maxf));
    copySipHeaders(*m,msg,"osip_");
    String caller = msg.getValue("caller",(line ? line->getUserName().c_str() : (const char*)0));
    String display = msg.getValue("callername",(line ? line->getFullName().c_str() : (const char*)0));
    m->complete(plugin.ep()->engine(),
	caller,
	msg.getValue("domain",(line ? line->domain().c_str() : (const char*)0)));
    if (display) {
	String desc;
	desc << "\"" << display << "\" ";
	SIPHeaderLine* hl = const_cast<SIPHeaderLine*>(m->getHeader("From"));
	if (hl)
	    *hl = desc + *hl;
    }
    if (msg.getParam("calledname")) {
	String desc;
	desc << "\"" << msg.getValue("calledname") << "\" ";
	SIPHeaderLine* hl = const_cast<SIPHeaderLine*>(m->getHeader("To"));
	if (hl)
	    *hl = desc + *hl;
    }
    if (plugin.ep()->engine()->prack())
	m->addHeader("Supported","100rel");
    m_host = m->getParty()->getPartyAddr();
    m_port = m->getParty()->getPartyPort();
    m_address << m_host << ":" << m_port;
    filterDebug(m_address);
    m_dialog = *m;
    if (s_privacy)
	copyPrivacy(*m,msg);

    // add some Call-Info headers
    const char* info = msg.getValue("caller_info_uri");
    if (info) {
	SIPHeaderLine* hl = new SIPHeaderLine("Call-Info",info);
	hl->setParam("purpose","info");
	m->addHeader(hl);
    }
    info = msg.getValue("caller_icon_uri");
    if (info) {
	SIPHeaderLine* hl = new SIPHeaderLine("Call-Info",info);
	hl->setParam("purpose","icon");
	m->addHeader(hl);
    }
    info = msg.getValue("caller_card_uri");
    if (info) {
	SIPHeaderLine* hl = new SIPHeaderLine("Call-Info",info);
	hl->setParam("purpose","card");
	m->addHeader(hl);
    }

    SDPBody* sdp = createPasstroughSDP(msg);
    if (!sdp)
	sdp = createRtpSDP(m_host,msg);
    m->setBody(sdp);
    m_tr = plugin.ep()->engine()->addMessage(m);
    if (m_tr) {
	m_tr->ref();
	m_callid = m_tr->getCallID();
	m_tr->setUserData(this);
    }
    m->deref();
    setMaxcall(msg);
    Message* s = message("chan.startup");
    s->setParam("caller",caller);
    s->setParam("called",msg.getValue("called"));
    s->setParam("billid",msg.getValue("billid"));
    s->setParam("username",msg.getValue("username"));
    s->setParam("calledfull",m_uri.getUser());
    Engine::enqueue(s);
}

YateSIPConnection::~YateSIPConnection()
{
    Debug(this,DebugAll,"YateSIPConnection::~YateSIPConnection() [%p]",this);
    hangup();
    clearTransaction();
    setMedia(0);
    if (m_route) {
	delete m_route;
	m_route = 0;
    }
    if (m_routes) {
	delete m_routes;
	m_routes = 0;
    }
}

void YateSIPConnection::setMedia(ObjList* media)
{
    if (media == m_rtpMedia)
	return;
    ObjList* tmp = m_rtpMedia;
    m_rtpMedia = media;
    if (tmp) {
	ObjList* l = tmp->skipNull();
	for (; l; l = l->skipNext()) {
	    RtpMedia* m = static_cast<RtpMedia*>(l->get());
	    clearEndpoint(*m);
	}
	tmp->destruct();
    }
}

void YateSIPConnection::startRouter()
{
    Message* m = m_route;
    m_route = 0;
    Channel::startRouter(m);
}

void YateSIPConnection::clearTransaction()
{
    if (!(m_tr || m_tr2))
	return;
    Lock lock(driver());
    if (m_tr) {
	m_tr->setUserData(0);
	if (m_tr->isIncoming()) {
	    if (m_tr->setResponse(m_reasonCode,m_reason.null() ? "Request Terminated" : m_reason.c_str()))
		m_byebye = false;
	}
	m_tr->deref();
	m_tr = 0;
    }
    // cancel any pending reINVITE
    if (m_tr2) {
	m_tr2->setUserData(0);
	if (m_tr2->isIncoming())
	    m_tr2->setResponse(487);
	m_tr2->deref();
	m_tr2 = 0;
    }
}

void YateSIPConnection::detachTransaction2()
{
    Lock lock(driver());
    if (m_tr2) {
	m_tr2->setUserData(0);
	m_tr2->deref();
	m_tr2 = 0;
    }
}

void YateSIPConnection::hangup()
{
    if (m_hungup)
	return;
    m_hungup = true;
    const char* error = lookup(m_reasonCode,dict_errors);
    Debug(this,DebugAll,"YateSIPConnection::hangup() state=%d trans=%p error='%s' code=%d reason='%s' [%p]",
	m_state,m_tr,error,m_reasonCode,m_reason.c_str(),this);
    Message* m = message("chan.hangup");
    if (m_reason)
	m->addParam("reason",m_reason);
    Engine::enqueue(m);
    switch (m_state) {
	case Cleared:
	    clearTransaction();
	    return;
	case Incoming:
	    if (m_tr) {
		clearTransaction();
		return;
	    }
	    break;
	case Outgoing:
	case Ringing:
	    if (m_cancel && m_tr) {
		SIPMessage* m = new SIPMessage("CANCEL",m_uri);
		plugin.ep()->buildParty(m,m_host,m_port,plugin.findLine(m_line));
		if (!m->getParty())
		    Debug(this,DebugWarn,"Could not create party for '%s:%d' [%p]",
			m_host.c_str(),m_port,this);
		else {
		    const SIPMessage* i = m_tr->initialMessage();
		    m->copyHeader(i,"Via");
		    m->copyHeader(i,"From");
		    m->copyHeader(i,"To");
		    m->copyHeader(i,"Call-ID");
		    String tmp;
		    tmp << i->getCSeq() << " CANCEL";
		    m->addHeader("CSeq",tmp);
		    plugin.ep()->engine()->addMessage(m);
		}
		m->deref();
	    }
	    break;
    }
    clearTransaction();
    m_state = Cleared;

    if (m_byebye) {
	m_byebye = false;
	SIPMessage* m = createDlgMsg("BYE");
	if (m) {
	    if (m_reason) {
		// FIXME: add SIP and Q.850 cause codes, set the proper reason
		SIPHeaderLine* hl = new SIPHeaderLine("Reason","SIP");
		hl->setParam("text","\"" + m_reason + "\"");
		m->addHeader(hl);
	    }
	    plugin.ep()->engine()->addMessage(m);
	    m->deref();
	}
    }
    if (!error)
	error = m_reason.c_str();
    disconnect(error);
}

// Creates a new message in an existing dialog
SIPMessage* YateSIPConnection::createDlgMsg(const char* method, const char* uri)
{
    if (!uri)
	uri = m_uri;
    SIPMessage* m = new SIPMessage(method,uri);
    m->addRoutes(m_routes);
    plugin.ep()->buildParty(m,m_host,m_port,plugin.findLine(m_line));
    if (!m->getParty()) {
	Debug(this,DebugWarn,"Could not create party for '%s:%d' [%p]",
	    m_host.c_str(),m_port,this);
	m->destruct();
	return 0;
    }
    m->addHeader("Call-ID",m_callid);
    String tmp;
    tmp << "<" << m_dialog.localURI << ">";
    SIPHeaderLine* hl = new SIPHeaderLine("From",tmp);
    tmp = m_dialog.localTag;
    if (tmp.null() && m_tr)
	tmp = m_tr->getDialogTag();
    if (tmp)
	hl->setParam("tag",tmp);
    m->addHeader(hl);
    tmp.clear();
    tmp << "<" << m_dialog.remoteURI << ">";
    hl = new SIPHeaderLine("To",tmp);
    tmp = m_dialog.remoteTag;
    if (tmp.null() && m_tr)
	tmp = m_tr->getDialogTag();
    if (tmp)
	hl->setParam("tag",tmp);
    m->addHeader(hl);
    return m;
}

// Emit a PRovisional ACK if enabled in the engine
bool YateSIPConnection::emitPRACK(const SIPMessage* msg)
{
    if (!plugin.ep()->engine()->prack())
	return false;
    if (!(msg && msg->isAnswer() && (msg->code > 100) && (msg->code < 200)))
	return false;
    const SIPHeaderLine* rs = msg->getHeader("RSeq");
    const SIPHeaderLine* cs = msg->getHeader("CSeq");
    if (!(rs && cs))
	return false;
    String tmp;
    const SIPHeaderLine* co = msg->getHeader("Contact");
    if (co) {
	tmp = *co;
	Regexp r("^[^<]*<\\([^>]*\\)>.*$");
	if (tmp.matches(r))
	    tmp = tmp.matchString(1);
    }
    SIPMessage* m = createDlgMsg("PRACK",tmp);
    if (!m)
	return false;
    tmp = *rs;
    tmp << " " << *cs;
    m->addHeader("RAck",tmp);
    plugin.ep()->engine()->addMessage(m);
    m->deref();
    return true;
}

// Creates a SDP for provisional (1xx) messages
SDPBody* YateSIPConnection::createProvisionalSDP(Message& msg)
{
    if (m_rtpForward)
	return createPasstroughSDP(msg);
    // check if our peer can source at least audio data
    if (!(getPeer() && getPeer()->getSource() && msg.getBoolValue("earlymedia",true)))
	return 0;
    if (m_rtpAddr.null())
	return 0;
    return createRtpSDP(true);
}

// Creates a SDP from RTP address data present in message
SDPBody* YateSIPConnection::createPasstroughSDP(Message& msg, bool update)
{
    String tmp = msg.getValue("rtp_forward");
    msg.clearParam("rtp_forward");
    if (!(m_rtpForward && tmp.toBoolean()))
	return 0;
    String* raw = msg.getParam("sdp_raw");
    if (raw) {
	m_sdpForward = m_sdpForward || s_forward_sdp;
	if (m_sdpForward) {
	    msg.setParam("rtp_forward","accepted");
	    return new SDPBody("application/sdp",raw->safe(),raw->length());
	}
    }
    String addr(msg.getValue("rtp_addr"));
    if (addr.null())
	return 0;

    ObjList* lst = 0;
    unsigned int n = msg.length();
    for (unsigned int i = 0; i < n; i++) {
	const NamedString* p = msg.getParam(i);
	if (!p)
	    continue;
	// search for rtp_port or rtp_port_MEDIANAME parameters
	tmp = p->name();
	if (!tmp.startSkip("rtp_port",false))
	    continue;
	if (tmp && (tmp[0] != '_'))
	    continue;
	// now tmp holds the suffix for the media, null for audio
	bool audio = tmp.null();
	// check if media is supported, default only for audio
	if (!msg.getBoolValue("media"+tmp,audio))
	    continue;
	int port = p->toInteger();
	if (!port)
	    continue;
	const char* fmts = msg.getValue("formats"+tmp);
	if (!fmts)
	    continue;
	if (audio)
	    tmp = "audio";
	else
	    tmp >> "_";
	RtpMedia* rtp = 0;
	// try to take the media descriptor from the old list
	if (update && m_rtpMedia) {
	    ObjList* om = m_rtpMedia->find(tmp);
	    if (om)
		rtp = static_cast<RtpMedia*>(om->remove(false));
	}
	if (rtp)
	    rtp->update(fmts,-1,port);
	else
	    rtp = new RtpMedia(tmp,fmts,-1,port);
	if (!lst)
	    lst = new ObjList;
	lst->append(rtp);
    }
    if (!lst)
	return 0;

    SDPBody* sdp = createSDP(addr,lst);
    if (update) {
	m_rtpLocalAddr = addr;
	setMedia(lst);
    }
    else
	lst->destruct();

    if (sdp)
	msg.setParam("rtp_forward","accepted");
    return sdp;
}

// Dispatches a RTP message for a media, optionally start RTP and pick parameters
bool YateSIPConnection::dispatchRtp(RtpMedia* media, const char* addr, bool start, bool pick)
{
    if (!(media && addr))
	return false;
    Message m("chan.rtp");
    complete(m,true);
    m.userData(static_cast<CallEndpoint *>(this));
    m.addParam("media",*media);
    m.addParam("direction","bidir");
    if (m_rtpLocalAddr)
	m.addParam("localip",m_rtpLocalAddr);
    m.addParam("remoteip",addr);
    if (start) {
	m.addParam("remoteport",media->remotePort());
	m.addParam("format",media->format());
    }
    if (!Engine::dispatch(m))
	return false;
    if (!pick)
	return true;
    m_rtpForward = false;
    m_rtpLocalAddr = m.getValue("localip",m_rtpLocalAddr);
    m_mediaStatus = MediaStarted;
    media->update(m,start);
    return true;
}

// Creates a set of unstarted external RTP channels from remote addr and builds SDP from them
SDPBody* YateSIPConnection::createRtpSDP(const char* addr, const Message& msg)
{

    bool defaults = true;
    ObjList* lst = 0;
    unsigned int n = msg.length();
    for (unsigned int i = 0; i < n; i++) {
	const NamedString* p = msg.getParam(i);
	if (!p)
	    continue;
	// search for rtp_port or rtp_port_MEDIANAME parameters
	String tmp(p->name());
	if (!tmp.startSkip("media",false))
	    continue;
	if (tmp && (tmp[0] != '_'))
	    continue;
	// since we found at least one media declaration disable defaults
	defaults = false;
	// now tmp holds the suffix for the media, null for audio
	bool audio = tmp.null();
	// check if media is supported, default only for audio
	if (!p->toBoolean(audio))
	    continue;
	const char* fmts = msg.getValue("formats"+tmp);
	if (audio && !fmts)
	    fmts = "alaw,mulaw";
	if (!fmts)
	    continue;
	if (audio)
	    tmp = "audio";
	else
	    tmp >> "_";
	RtpMedia* rtp = 0;
	// try to take the media descriptor from the old list
	if (m_rtpMedia) {
	    ObjList* om = m_rtpMedia->find(tmp);
	    if (om)
		rtp = static_cast<RtpMedia*>(om->remove(false));
	}
	if (rtp)
	    rtp->update(fmts);
	else
	    rtp = new RtpMedia(tmp,fmts);
	if (!lst)
	    lst = new ObjList;
	lst->append(rtp);
    }

    if (defaults && !lst) {
	lst = new ObjList;
	lst->append(new RtpMedia("audio",msg.getValue("formats","alaw,mulaw")));
    }

    setMedia(lst);

    ObjList* l = m_rtpMedia->skipNull();
    for (; l; l = l->skipNext()) {
	RtpMedia* m = static_cast<RtpMedia*>(l->get());
	if (!dispatchRtp(m,addr,false,true))
	    return 0;
    }
    return createSDP(getRtpAddr());
}

// Creates a set of started external RTP channels from remote addr and builds SDP from them
SDPBody* YateSIPConnection::createRtpSDP(bool start)
{
    if (m_rtpAddr.null()) {
	m_mediaStatus = MediaMuted;
	return createSDP(0);
    }

    ObjList* l = m_rtpMedia->skipNull();
    for (; l; l = l->skipNext()) {
	RtpMedia* m = static_cast<RtpMedia*>(l->get());
	if (!dispatchRtp(m,m_rtpAddr,start,true))
	    return 0;
    }
    return createSDP(getRtpAddr());
}

// Starts an already created set of external RTP channels
bool YateSIPConnection::startRtp()
{
    if (m_mediaStatus != MediaStarted)
	return false;
    DDebug(this,DebugAll,"YateSIPConnection::startRtp() [%p]",this);

    bool ok = true;
    ObjList* l = m_rtpMedia->skipNull();
    for (; l; l = l->skipNext()) {
	RtpMedia* m = static_cast<RtpMedia*>(l->get());
	ok = dispatchRtp(m,m_rtpAddr,true,false) && ok;
    }
    return ok;
}

// Creates a SDP body from transport address and list of media descriptors
SDPBody* YateSIPConnection::createSDP(const char* addr, ObjList* mediaList)
{
    DDebug(this,DebugAll,"YateSIPConnection::createSDP('%s',%p) [%p]",
	addr,mediaList,this);
    if (!mediaList)
	mediaList = m_rtpMedia;
    // if we got no media descriptors we simply create no SDP
    if (!mediaList)
	return 0;
    if (m_sdpSession)
	++m_sdpVersion;
    else
	m_sdpVersion = m_sdpSession = Time::secNow();

    // no address means on hold or muted
    String origin;
    origin << "yate " << m_sdpSession << " " << m_sdpVersion << " IN IP4 " << (addr ? addr : m_host.safe());
    String conn;
    conn << "IN IP4 " << (addr ? addr : "0.0.0.0");

    SDPBody* sdp = new SDPBody;
    sdp->addLine("v","0");
    sdp->addLine("o",origin);
    sdp->addLine("s","SIP Call");
    sdp->addLine("c",conn);
    sdp->addLine("t","0 0");

    bool defcodecs = s_cfg.getBoolValue("codecs","default",true);
    for (ObjList* ml = mediaList->skipNull(); ml; ml = ml->skipNext()) {
	RtpMedia* m = static_cast<RtpMedia*>(ml->get());

	String frm(m->fmtList());
	ObjList* l = frm.split(',',false);
	frm = *m;
	frm << " " << (m->localPort() ? m->localPort().c_str() : "0") << " RTP/AVP";
	ObjList rtpmap;
	int ptime = 0;
	ObjList* f = l;
	for (; f; f = f->next()) {
	    String* s = static_cast<String*>(f->get());
	    if (s) {
		int mode = 0;
		if (*s == "ilbc20")
		    ptime = mode = 20;
		else if (*s == "ilbc30")
		    ptime = mode = 30;
		int payload = s->toInteger(dict_payloads,-1);
		if (payload >= 0) {
		    const char* map = lookup(payload,dict_rtpmap);
		    if (map && s_cfg.getBoolValue("codecs",*s,defcodecs && DataTranslator::canConvert(*s))) {
			frm << " " << payload;
			String* temp = new String("rtpmap:");
			*temp << payload << " " << map;
			rtpmap.append(temp);
			if (mode) {
			    temp = new String("fmtp:");
			    *temp << payload << " mode=" << mode;
			    rtpmap.append(temp);
			}
		    }
		}
	    }
	}
	delete l;

	if (*m == "audio") {
	    // always claim to support telephone events
	    frm << " 101";
	    rtpmap.append(new String("rtpmap:101 telephone-event/8000"));
	}

	if (ptime) {
	    String* temp = new String("ptime:");
	    *temp << ptime;
	    rtpmap.append(temp);
	}

	sdp->addLine("m",frm);
	for (f = rtpmap.skipNull(); f; f = f->skipNext()) {
	    String* s = static_cast<String*>(f->get());
	    if (s)
		sdp->addLine("a",*s);
	}
    }

    return sdp;
}

// Add raw SDP forwarding parameter to a message
bool YateSIPConnection::addSdpParams(Message& msg, const SIPBody* body)
{
    if (m_sdpForward && body && body->isSDP()) {
	const DataBlock& raw = body->getBody();
	String tmp((const char*)raw.data(),raw.length());
	msg.setParam("rtp_forward","yes");
	msg.addParam("sdp_raw",tmp);
	return true;
    }
    return false;
}

// Add RTP forwarding parameters to a message
bool YateSIPConnection::addRtpParams(Message& msg, const String& natAddr, const SIPBody* body)
{
    if (!(m_rtpMedia && m_rtpAddr))
	return false;
    ObjList* l = m_rtpMedia->skipNull();
    for (; l; l = l->skipNext()) {
	RtpMedia* m = static_cast<RtpMedia*>(l->get());
	msg.addParam("formats"+m->suffix(),m->formats());
	msg.addParam("media"+m->suffix(),"yes");
    }
    if (!startRtp() && m_rtpForward) {
	if (natAddr)
	    msg.addParam("rtp_nat_addr",natAddr);
	msg.addParam("rtp_forward","yes");
	msg.addParam("rtp_addr",m_rtpAddr);
	l = m_rtpMedia->skipNull();
	for (; l; l = l->skipNext()) {
	    RtpMedia* m = static_cast<RtpMedia*>(l->get());
	    msg.addParam("rtp_port"+m->suffix(),m->remotePort());
	}
	addSdpParams(msg,body);
	return true;
    }
    return false;
}

// Process SIP events belonging to this connection
bool YateSIPConnection::process(SIPEvent* ev)
{
    DDebug(this,DebugInfo,"YateSIPConnection::process(%p) %s [%p]",
	ev,SIPTransaction::stateName(ev->getState()),this);
    const SIPMessage* msg = ev->getMessage();
    int code = ev->getTransaction()->getResponseCode();
    if (ev->getTransaction() == m_tr2) {
	// reINVITE transaction
	if (ev->getState() == SIPTransaction::Cleared) {
	    detachTransaction2();
	    Message* m = message("call.update");
	    m->addParam("operation","reject");
	    m->addParam("error","timeout");
	    Engine::enqueue(m);
	    return false;
	}
	if (!msg || msg->isOutgoing() || !msg->isAnswer())
	    return false;
	if (code < 200)
	    return false;
	Message* m = message("call.update");
	if (code < 300) {
	    m->addParam("operation","notify");
	    String natAddr;
	    if (msg->body && msg->body->isSDP()) {
		DDebug(this,DebugInfo,"YateSIPConnection got SDP [%p]",this);
		setMedia(parseSDP(static_cast<SDPBody*>(msg->body),m_rtpAddr,m_rtpMedia));
		// guess if the call comes from behind a NAT
		if (s_auto_nat && isNatBetween(m_rtpAddr,m_host)) {
		    Debug(this,DebugInfo,"RTP NAT detected: private '%s' public '%s'",
			m_rtpAddr.c_str(),m_host.c_str());
		    natAddr = m_rtpAddr;
		    m_rtpAddr = m_host;
		}
		DDebug(this,DebugAll,"RTP addr '%s' [%p]",m_rtpAddr.c_str(),this);
	    }
	    if (!addRtpParams(*m,natAddr,msg->body))
		addSdpParams(*m,msg->body);
	    Engine::enqueue(m);
	}
	else {
	    m->addParam("operation","reject");
	    m->addParam("error",lookup(code,dict_errors,"failure"));
	    m->addParam("reason",msg->reason);
	}
	detachTransaction2();
	Engine::enqueue(m);
	return false;
    }
    m_dialog = *ev->getTransaction()->recentMessage();
    if (msg && !msg->isOutgoing() && msg->isAnswer() && (code >= 300)) {
	m_cancel = false;
	m_byebye = false;
	setReason(msg->reason,code);
	hangup();
    }
    if (!ev->isActive()) {
	Lock lock(driver());
	if (m_tr) {
	    DDebug(this,DebugInfo,"YateSIPConnection clearing transaction %p [%p]",
		m_tr,this);
	    m_tr->setUserData(0);
	    m_tr->deref();
	    m_tr = 0;
	}
	if (m_state != Established)
	    hangup();
	return false;
    }
    if (!msg || msg->isOutgoing())
	return false;
    String natAddr;
    if (msg->body && msg->body->isSDP()) {
	DDebug(this,DebugInfo,"YateSIPConnection got SDP [%p]",this);
	setMedia(parseSDP(static_cast<SDPBody*>(msg->body),m_rtpAddr,m_rtpMedia));
	// guess if the call comes from behind a NAT
	if (s_auto_nat && isNatBetween(m_rtpAddr,m_host)) {
	    Debug(this,DebugInfo,"RTP NAT detected: private '%s' public '%s'",
		m_rtpAddr.c_str(),m_host.c_str());
	    natAddr = m_rtpAddr;
	    m_rtpAddr = m_host;
	}
	DDebug(this,DebugAll,"RTP addr '%s' [%p]",m_rtpAddr.c_str(),this);
    }
    if ((!m_routes) && msg->isAnswer() && (msg->code > 100) && (msg->code < 300))
	m_routes = msg->getRoutes();

    if (msg->isAnswer() && m_externalAddr.null() && m_line) {
	// see if we should detect our external address
	const YateSIPLine* line = plugin.findLine(m_line);
	if (line && line->localDetect()) {
	    SIPHeaderLine* hl = const_cast<SIPHeaderLine*>(msg->getHeader("Via"));
	    if (hl) {
		const NamedString* par = hl->getParam("received");
		if (par && *par) {
		    m_externalAddr = *par;
		    Debug(this,DebugInfo,"Detected local address '%s' [%p]",
			m_externalAddr.c_str(),this);
		}
	    }
	}
    }

    if (msg->isAnswer() && ((msg->code / 100) == 2)) {
	m_cancel = false;
	Lock lock(driver());
	const SIPMessage* ack = m_tr ? m_tr->latestMessage() : 0;
	if (ack && ack->isACK()) {
	    // accept any URI change caused by a Contact: header in the 2xx
	    m_uri = ack->uri;
	    m_uri.parse();
	}
	lock.drop();
	setReason("",0);
	setStatus("answered",Established);
	maxcall(0);
	Message *m = message("call.answered");
	addRtpParams(*m,natAddr,msg->body);
	Engine::enqueue(m);
    }
    if ((m_state < Ringing) && msg->isAnswer()) {
	if (msg->code == 180) {
	    setStatus("ringing",Ringing);
	    Message *m = message("call.ringing");
	    addRtpParams(*m,natAddr,msg->body);
	    if (m_rtpAddr.null())
		m->addParam("earlymedia","false");
	    Engine::enqueue(m);
	}
	if (msg->code == 183) {
	    setStatus("progressing");
	    Message *m = message("call.progress");
	    addRtpParams(*m,natAddr,msg->body);
	    if (m_rtpAddr.null())
		m->addParam("earlymedia","false");
	    Engine::enqueue(m);
	}
	if ((msg->code > 100) && (msg->code < 200))
	    emitPRACK(msg);
    }
    if (msg->isACK()) {
	DDebug(this,DebugInfo,"YateSIPConnection got ACK [%p]",this);
	startRtp();
    }
    return false;
}

void YateSIPConnection::reInvite(SIPTransaction* t)
{
    if (!checkUser(t))
	return;
    DDebug(this,DebugAll,"YateSIPConnection::reInvite(%p) [%p]",t,this);
    if (m_tr || m_tr2) {
	// another request pending - refuse this one
	t->setResponse(491);
	return;
    }
    // hack: use a while instead of if so we can return or break out of it
    while (t->initialMessage()->body && t->initialMessage()->body->isSDP()) {
	// for pass-trough RTP we need support from our peer
	if (m_rtpForward) {
	    String addr;
	    String natAddr;
	    ObjList* lst = parseSDP(static_cast<SDPBody*>(t->initialMessage()->body),addr);
	    if (!lst)
		break;
	    // guess if the call comes from behind a NAT
	    if (s_auto_nat && isNatBetween(addr,m_host)) {
		Debug(this,DebugInfo,"RTP NAT detected: private '%s' public '%s'",
		    addr.c_str(),m_host.c_str());
		natAddr = addr;
		addr = m_host;
	    }
	    Debug(this,DebugAll,"reINVITE RTP addr '%s'",addr.c_str());

	    Message msg("call.update");
	    complete(msg);
	    msg.addParam("operation","request");
	    copySipHeaders(msg,*t->initialMessage());
	    msg.addParam("rtp_forward","yes");
	    msg.addParam("rtp_addr",addr);
	    if (natAddr)
		msg.addParam("rtp_nat_addr",natAddr);
	    ObjList* l = lst->skipNull();
	    for (; l; l = l->skipNext()) {
		RtpMedia* r = static_cast<RtpMedia*>(l->get());
		msg.addParam("media"+r->suffix(),"yes");
		msg.addParam("rtp_port"+r->suffix(),r->remotePort());
		msg.addParam("formats"+r->suffix(),r->formats());
	    }
	    if (m_sdpForward) {
		const DataBlock& raw = t->initialMessage()->body->getBody();
		String tmp((const char*)raw.data(),raw.length());
		msg.addParam("sdp_raw",tmp);
	    }
	    // if peer doesn't support updates fail the reINVITE
	    if (!Engine::dispatch(msg)) {
		t->setResponse(msg.getIntValue("error",dict_errors,488),msg.getValue("reason"));
		return;
	    }
	    // we remember the request and leave it pending
	    t->ref();
	    t->setUserData(this);
	    m_tr2 = t;
	    return;
	}
	// refuse request if we had no media at all before
	if (m_mediaStatus == MediaMissing)
	    break;
	String addr;
	ObjList* lst = parseSDP(static_cast<SDPBody*>(t->initialMessage()->body),addr);
	if (!lst)
	    break;
	// guess if the call comes from behind a NAT
	if (s_auto_nat && isNatBetween(addr,m_host)) {
	    Debug(this,DebugInfo,"RTP NAT detected: private '%s' public '%s'",
		addr.c_str(),m_host.c_str());
	    addr = m_host;
	}
	m_rtpAddr = addr;
	setMedia(lst);
	Debug(this,DebugAll,"New RTP addr '%s'",m_rtpAddr.c_str());

	m_mediaStatus = MediaMissing;
	// let RTP guess again the local interface
	m_rtpLocalAddr.clear();
	// clear all data endpoints - createRtpSDP will build new ones
	clearEndpoint();

	SIPMessage* m = new SIPMessage(t->initialMessage(), 200);
	SDPBody* sdp = createRtpSDP(true);
	m->setBody(sdp);
	t->setResponse(m);
	m->deref();
	Message* msg = message("call.update");
	msg->addParam("operation","notify");
	msg->addParam("mandatory","false");
	msg->addParam("mute",String::boolText(MediaStarted != m_mediaStatus));
	Engine::enqueue(msg);
	return;
    }
    t->setResponse(488);
}

bool YateSIPConnection::checkUser(SIPTransaction* t, bool refuse)
{
    // don't try to authenticate requests from server
    if (m_user.null() || m_line)
	return true;
    int age = t->authUser(m_user);
    if ((age >= 0) && (age <= 10))
	return true;
    DDebug(this,DebugAll,"YateSIPConnection::checkUser(%p) failed, age %d [%p]",t,age,this);
    if (refuse)
	t->requestAuth(s_realm,"",age >= 0);
    return false;
}

void YateSIPConnection::doBye(SIPTransaction* t)
{
    if (m_authBye && !checkUser(t))
	return;
    DDebug(this,DebugAll,"YateSIPConnection::doBye(%p) [%p]",t,this);
    const SIPHeaderLine* hl = t->initialMessage()->getHeader("Reason");
    if (hl) {
	const NamedString* text = hl->getParam("text");
	if (text)
	    m_reason = *text;
	// FIXME: add SIP and Q.850 cause codes
    }
    t->setResponse(200);
    m_byebye = false;
    hangup();
}

void YateSIPConnection::doCancel(SIPTransaction* t)
{
#ifdef DEBUG
    // CANCEL cannot be challenged but it may (should?) be authenticated with
    //  an old nonce from the transaction that is being cancelled
    if (m_user && (t->authUser(m_user) < 0))
	Debug(&plugin,DebugMild,"User authentication failed for user '%s' but CANCELing anyway [%p]",
	    m_user.c_str(),this);
#endif
    DDebug(this,DebugAll,"YateSIPConnection::doCancel(%p) [%p]",t,this);
    if (m_tr) {
	t->setResponse(200);
	m_byebye = false;
	clearTransaction();
	disconnect("Cancelled");
    }
    else
	t->setResponse(481);
}

void YateSIPConnection::doInfo(SIPTransaction* t)
{
    if (m_authBye && !checkUser(t))
	return;
    DDebug(this,DebugAll,"YateSIPConnection::doInfo(%p) [%p]",t,this);
    int sig = -1;
    const SIPLinesBody* lb = YOBJECT(SIPLinesBody,t->initialMessage()->body);
    const SIPStringBody* sb = YOBJECT(SIPStringBody,t->initialMessage()->body);
    if (lb && (lb->getType() == "application/dtmf-relay")) {
	const ObjList* l = lb->lines().skipNull();
	for (; l; l = l->skipNext()) {
	    String tmp = static_cast<String*>(l->get());
	    tmp.toLower();
	    if (tmp.startSkip("signal=",false)) {
		sig = tmp.toInteger(-1);
		break;
	    }
	}
    }
    else if (sb && (sb->getType() == "application/dtmf"))
	sig = sb->text().toInteger(-1);
    else {
	t->setResponse(415);
	return;
    }
    t->setResponse(200);
    if ((sig >= 0) && (sig <= 16)) {
	char tmp[2];
	tmp[0] = s_dtmfs[sig];
	tmp[1] = '\0';
	Message* msg = message("chan.dtmf");
	msg->addParam("text",tmp);
	Engine::enqueue(msg);
    }
}

void YateSIPConnection::doRefer(SIPTransaction* t)
{
    if (m_authBye && !checkUser(t))
	return;
    DDebug(this,DebugAll,"YateSIPConnection::doRefer(%p) [%p]",t,this);
    if (m_referring) {
	DDebug(this,DebugAll,"YateSIPConnection::doRefer(%p) [%p].  Already referring.",t,this);
	t->setResponse(491);           // Request Pending
	return;
    }
    m_referring = true;
    const SIPHeaderLine* refHdr = t->initialMessage()->getHeader("Refer-To");
    if (!(refHdr && refHdr->length())) {
	DDebug(this,DebugAll,"YateSIPConnection::doRefer(%p) [%p]. Empty or missing 'Refer-To' header.",t,this);
	t->setResponse(400);           // Bad request
	m_referring = false;
	return;
    }
    bool unattended = (refHdr->find("?") == -1);
    if (unattended) {
	Message* msg = 0;
	SIPMessage* sipNotify = 0;
	if (initUnattendedTransfer(msg,sipNotify,t->initialMessage(),refHdr)) {
	    Channel* ch = YOBJECT(Channel,getPeer());
	    if (ch && ch->driver()) {
		t->setResponse(202);   // Accept
		(new YateSIPRefer(id(),getPeer()->id(),ch->driver(),msg,sipNotify))->startup();
		return;
	    }
	    DDebug(this,DebugAll,"YateSIPConnection::doRefer(%p) [%p]. The transferred party has no driver!",t,this);
	} 
	t->setResponse(503);           // Service Unavailable
    }
    else {
	DDebug(this,DebugAll,"YateSIPConnection::doRefer(%p) [%p]. Received attended transfer request. Not implemented.",t,this);
	t->setResponse(501);           // Not implemented
    }
    m_referring = false;
}

void YateSIPConnection::disconnected(bool final, const char *reason)
{
    Debug(this,DebugAll,"YateSIPConnection::disconnected() '%s' [%p]",reason,this);
    if (reason) {
	int code = lookup(reason,dict_errors);
	if (code)
	    setReason(lookup(code,SIPResponses,reason),code);
	else
	    setReason(reason);
    }
    Channel::disconnected(final,reason);
}

bool YateSIPConnection::msgProgress(Message& msg)
{
    Channel::msgProgress(msg);
    Lock lock(driver());
    if (m_tr && (m_tr->getState() == SIPTransaction::Process)) {
	SIPMessage* m = new SIPMessage(m_tr->initialMessage(), 183);
	m->setBody(createProvisionalSDP(msg));
	m_tr->setResponse(m);
	m->deref();
    }
    setStatus("progressing");
    return true;
}

bool YateSIPConnection::msgRinging(Message& msg)
{
    Channel::msgRinging(msg);
    Lock lock(driver());
    if (m_tr && (m_tr->getState() == SIPTransaction::Process)) {
	SIPMessage* m = new SIPMessage(m_tr->initialMessage(), 180);
	m->setBody(createProvisionalSDP(msg));
	m_tr->setResponse(m);
	m->deref();
    }
    setStatus("ringing");
    return true;
}

bool YateSIPConnection::msgAnswered(Message& msg)
{
    Lock lock(driver());
    if (m_tr && (m_tr->getState() == SIPTransaction::Process)) {
	SIPMessage* m = new SIPMessage(m_tr->initialMessage(), 200);
	SDPBody* sdp = createPasstroughSDP(msg);
	if (!sdp) {
	    m_rtpForward = false;
	    // don't start RTP yet, only when we get the ACK
	    sdp = createRtpSDP(false);
	}
	m->setBody(sdp);

	const SIPHeaderLine* co = m_tr->initialMessage()->getHeader("Contact");
	if (co) {
	    // INVITE had a Contact: header - time to change remote URI
	    m_uri = *co;
	    m_uri.parse();
	}

	// and finally send the answer, transaction will finish soon afterwards
	m_tr->setResponse(m);
	m->deref();
    }
    setReason("",0);
    setStatus("answered",Established);
    return true;
}

bool YateSIPConnection::msgTone(Message& msg, const char* tone)
{
    if (m_info) {
	for (; tone && *tone; tone++) {
	    char c = *tone;
	    for (int i = 0; i <= 16; i++) {
		if (s_dtmfs[i] == c) {
		    SIPMessage* m = createDlgMsg("INFO");
		    if (m) {
			String tmp;
			tmp << "Signal=" << i << "\r\n";
			m->setBody(new SIPStringBody("application/dtmf-relay",tmp));
			plugin.ep()->engine()->addMessage(m);
			m->deref();
		    }
		    break;
		}
	    }
	}
	return true;
    }
    if (m_rtpMedia && (m_mediaStatus == MediaStarted)) {
	ObjList* l = m_rtpMedia->find("audio");
	const RtpMedia* m = static_cast<const RtpMedia*>(l ? l->get() : 0);
	if (m) {
	    if (m_inband && dtmfInband(tone))
		return true;
	    msg.setParam("targetid",m->id());
	    return false;
	}
    }
    return false;
}

bool YateSIPConnection::msgText(Message& msg, const char* text)
{
    if (null(text))
	return false;
    SIPMessage* m = createDlgMsg("MESSAGE");
    if (m) {
	m->setBody(new SIPStringBody("text/plain",text));
	plugin.ep()->engine()->addMessage(m);
	m->deref();
	return true;
    }
    return false;
}

bool YateSIPConnection::msgUpdate(Message& msg)
{
    String* oper = msg.getParam("operation");
    if (!oper || oper->null())
	return false;
    Lock lock(driver());
    if (*oper == "request") {
	if (m_tr || m_tr2) {
	    msg.setParam("error","pending");
	    msg.setParam("reason","Another INVITE Pending");
	    return false;
	}
	SDPBody* sdp = createPasstroughSDP(msg,false);
	if (!sdp) {
	    msg.setParam("error","failure");
	    msg.setParam("reason","Could not build the SDP");
	    return false;
	}
	SIPMessage* m = createDlgMsg("INVITE");
	copySipHeaders(*m,msg,"osip_");
	if (s_privacy)
	    copyPrivacy(*m,msg);
	m->setBody(sdp);
	m_tr2 = plugin.ep()->engine()->addMessage(m);
	if (m_tr2) {
	    m_tr2->ref();
	    m_tr2->setUserData(this);
	}
	m->deref();
	return true;
    }
    if (!m_tr2) {
	msg.setParam("error","nocall");
	return false;
    }
    if (!(m_tr2->isIncoming() && (m_tr2->getState() == SIPTransaction::Process))) {
	msg.setParam("error","failure");
	msg.setParam("reason","Incompatible Transaction State");
	return false;
    }
    if (*oper == "notify") {
	SDPBody* sdp = createPasstroughSDP(msg);
	if (!sdp) {
	    m_tr2->setResponse(500,"Server failed to build the SDP");
	    detachTransaction2();
	    return false;
	}
	SIPMessage* m = new SIPMessage(m_tr2->initialMessage(), 200);
	m->setBody(sdp);
	m_tr2->setResponse(m);
	detachTransaction2();
	m->deref();
	return true;
    }
    else if (*oper == "reject") {
	m_tr2->setResponse(msg.getIntValue("error",dict_errors,488),msg.getValue("reason"));
	detachTransaction2();
	return true;
    }
    return false;
}

void YateSIPConnection::statusParams(String& str)
{
    Channel::statusParams(str);
    if (m_line)
	str << ",line=" << m_line;
    if (m_user)
	str << ",user=" << m_user;
    if (m_rtpForward)
	str << ",forward=" << (m_sdpForward ? "sdp" : "rtp");
    str << ",inviting=" << (m_tr != 0);
}

bool YateSIPConnection::callRouted(Message& msg)
{
    Channel::callRouted(msg);
    Lock lock(driver());
    if (m_tr && (m_tr->getState() == SIPTransaction::Process)) {
	String s(msg.retValue());
	if (s.startSkip("sip/",false) && s && msg.getBoolValue("redirect")) {
	    Debug(this,DebugAll,"YateSIPConnection redirecting to '%s' [%p]",s.c_str(),this);
	    String tmp(msg.getValue("calledname"));
	    if (tmp)
		tmp = "\"" + tmp + "\" ";
	    s = tmp + "<" + s + ">";
	    SIPMessage* m = new SIPMessage(m_tr->initialMessage(),302);
	    m->addHeader("Contact",s);
	    m_tr->setResponse(m);
	    m->deref();
	    m_byebye = false;
	    setReason("Redirected",302);
	    setStatus("redirected");
	    return false;
	}
	if (msg.getBoolValue("progress",s_cfg.getBoolValue("general","progress",false)))
	    m_tr->setResponse(183);
    }
    return true;
}

void YateSIPConnection::callAccept(Message& msg)
{
    m_user = msg.getValue("username");
    if (m_authBye)
	m_authBye = msg.getBoolValue("xsip_auth_bye",true);
    if (m_rtpForward) {
	String tmp(msg.getValue("rtp_forward"));
	if (tmp != "accepted")
	    m_rtpForward = false;
    }
    Channel::callAccept(msg);
}

void YateSIPConnection::callRejected(const char* error, const char* reason, const Message* msg)
{
    Channel::callRejected(error,reason,msg);
    int code = lookup(error,dict_errors,500);
    Lock lock(driver());
    if (m_tr && (m_tr->getState() == SIPTransaction::Process)) {
	if (code == 401)
	    m_tr->requestAuth(s_realm,"",false);
	else
	    m_tr->setResponse(code,reason);
    }
    setReason(reason,code);
}

// msg: 'call.route' message to create & fill
// sipNotify: NOTIFY message to create & fill
// sipRefer: received REFER message, refHdr: 'Refer-To' header
// If return false, msg and sipNotify are 0
bool YateSIPConnection::initUnattendedTransfer(Message*& msg, SIPMessage*& sipNotify,
    const SIPMessage* sipRefer, const SIPHeaderLine* refHdr)
{
    // call.route
    msg = new Message("call.route");
    msg->addParam("id",getPeer()->id());
    const SIPHeaderLine* sh = sipRefer->getHeader("To");                   // caller
    if (sh) {
	URI uriCaller(*sh);
	uriCaller.parse();
	msg->addParam("caller",uriCaller.getUser());
	msg->addParam("callername",uriCaller.getDescription());
    }
    URI referTo(*refHdr);                                                  // called
    referTo.parse();
    msg->addParam("called",referTo.getUser());
    msg->addParam("calledname",referTo.getDescription());
    sh = sipRefer->getHeader("Referred-By");                               // diverter
    if (sh) {
	URI referBy(*sh);
	referBy.parse();
	msg->addParam("diverter",referBy.getUser());
	msg->addParam("divertername",referBy.getDescription());
    }
    msg->addParam("reason","transfer");                                    // reason
    // NOTIFY
    String tmp;
    const SIPHeaderLine* co = sipRefer->getHeader("Contact");
    if (co) {
	tmp = *co;
	Regexp r("^[^<]*<\\([^>]*\\)>.*$");
	if (tmp.matches(r))
	    tmp = tmp.matchString(1);
    }
    sipNotify = createDlgMsg("NOTIFY",tmp);
    plugin.ep()->buildParty(sipNotify);
    if (!sipNotify->getParty()) {
	DDebug(&plugin,DebugAll,"YateSIPConnection::initUnattendedTransfer. Could not create party to send NOTIFY");
	sipNotify->destruct();
	sipNotify = 0;
	delete msg;
	msg = 0;
	return false;
    }
    sipNotify->complete(plugin.ep()->engine());
    sipNotify->addHeader("Event","refer");
    sipNotify->addHeader("Subscription-State","terminated;reason=noresource");
    sipNotify->addHeader("Contact",sipRefer->uri);
    return true;
}

YateSIPLine::YateSIPLine(const String& name)
    : String(name), m_resend(0), m_keepalive(0), m_interval(0), m_alive(0),
      m_tr(0), m_marked(false), m_valid(false),
      m_localPort(0), m_partyPort(0), m_localDetect(false)
{
    DDebug(&plugin,DebugInfo,"YateSIPLine::YateSIPLine('%s') [%p]",c_str(),this);
    s_lines.append(this);
}

YateSIPLine::~YateSIPLine()
{
    DDebug(&plugin,DebugInfo,"YateSIPLine::~YateSIPLine() '%s' [%p]",c_str(),this);
    s_lines.remove(this,false);
    logout();
}

void YateSIPLine::setupAuth(SIPMessage* msg) const
{
    if (msg)
	msg->setAutoAuth(getAuthName(),m_password);
}

void YateSIPLine::setValid(bool valid, const char* reason)
{
    if ((m_valid == valid) && !reason)
	return;
    m_valid = valid;
    if (m_registrar && m_username) {
	Message* m = new Message("user.notify");
	m->addParam("account",*this);
	m->addParam("protocol","sip");
	m->addParam("username",m_username);
	m->addParam("registered",String::boolText(valid));
	if (reason)
	    m->addParam("reason",reason);
	Engine::enqueue(m);
    }
}

SIPMessage* YateSIPLine::buildRegister(int expires) const
{
    String exp(expires);
    String tmp;
    tmp << "sip:" << m_registrar;
    SIPMessage* m = new SIPMessage("REGISTER",tmp);
    plugin.ep()->buildParty(m,0,0,this);
    if (!m->getParty()) {
	Debug(&plugin,DebugWarn,"Could not create party for '%s' [%p]",
	    m_registrar.c_str(),this);
	m->destruct();
	return 0;
    }
    tmp = "\"";
    tmp << (m_display.null() ? m_username : m_display);
    tmp << "\" <sip:";
    tmp << m_username << "@";
    tmp << m->getParty()->getLocalAddr() << ":";
    tmp << m->getParty()->getLocalPort() << ">";
    m->addHeader("Contact",tmp);
    m->addHeader("Expires",exp);
    tmp = "<sip:";
    tmp << m_username << "@" << domain() << ">";
    m->addHeader("To",tmp);
    m->complete(plugin.ep()->engine(),m_username,domain());
    return m;
}

void YateSIPLine::login()
{
    m_keepalive = 0;
    if (m_registrar.null() || m_username.null()) {
	logout();
	setValid(true);
	return;
    }
    DDebug(&plugin,DebugInfo,"YateSIPLine '%s' logging in [%p]",c_str(),this);
    clearTransaction();

    SIPMessage* m = buildRegister(m_interval);
    if (!m) {
	setValid(false);
	return;
    }
    DDebug(&plugin,DebugInfo,"YateSIPLine '%s' emiting %p [%p]",
	c_str(),m,this);
    m_tr = plugin.ep()->engine()->addMessage(m);
    if (m_tr) {
	m_tr->ref();
	m_tr->setUserData(this);
    }
    m->deref();
}

void YateSIPLine::logout()
{
    m_resend = 0;
    m_keepalive = 0;
    bool sendLogout = m_valid && m_registrar && m_username;
    clearTransaction();
    setValid(false);
    if (sendLogout) {
	DDebug(&plugin,DebugInfo,"YateSIPLine '%s' logging out [%p]",c_str(),this);
	SIPMessage* m = buildRegister(0);
	m_partyAddr.clear();
	m_partyPort = 0;
	if (!m)
	    return;
	plugin.ep()->engine()->addMessage(m);
	m->deref();
    }
}

bool YateSIPLine::process(SIPEvent* ev)
{
    DDebug(&plugin,DebugInfo,"YateSIPLine::process(%p) %s [%p]",
	ev,SIPTransaction::stateName(ev->getState()),this);
    if (ev->getTransaction() != m_tr)
	return false;
    if (ev->getState() == SIPTransaction::Cleared) {
	clearTransaction();
	setValid(false,"timeout");
	m_resend = m_interval*(int64_t)1000000 + Time::now();
	m_keepalive = 0;
	return false;
    }
    const SIPMessage* msg = ev->getMessage();
    if (!(msg && msg->isAnswer()))
	return false;
    if (ev->getState() != SIPTransaction::Process)
	return false;
    clearTransaction();
    DDebug(&plugin,DebugAll,"YateSIPLine '%s' got answer %d [%p]",
	c_str(),msg->code,this);
    switch (msg->code) {
	case 200:
	    // re-register at 3/4 of the expire interval
	    m_resend = m_interval*(int64_t)750000 + Time::now();
	    m_keepalive = m_alive ? m_alive*(int64_t)1000000 + Time::now() : 0;
	    detectLocal(msg);
	    if (msg->getParty()) {
		m_partyAddr = msg->getParty()->getPartyAddr();
		m_partyPort = msg->getParty()->getPartyPort();
	    }
	    setValid(true);
	    Debug(&plugin,DebugCall,"SIP line '%s' logon success to %s:%d",
		c_str(),m_partyAddr.c_str(),m_partyPort);
	    break;
	default:
	    // detect local address even from failed attempts - helps next time
	    detectLocal(msg);
	    setValid(false,msg->reason);
	    Debug(&plugin,DebugWarn,"SIP line '%s' logon failure %d: %s",
		c_str(),msg->code,msg->reason.safe());
    }
    return false;
}

void YateSIPLine::detectLocal(const SIPMessage* msg)
{
    if (!(m_localDetect && msg->getParty()))
	return;
    String laddr = m_localAddr;
    int lport = m_localPort;
    SIPHeaderLine* hl = const_cast<SIPHeaderLine*>(msg->getHeader("Via"));
    if (hl) {
	const NamedString* par = hl->getParam("received");
	if (par && *par)
	    laddr = *par;
	par = hl->getParam("rport");
	if (par) {
	    int port = par->toInteger(0,10);
	    if (port > 0)
		lport = port;
	}
    }
    if (laddr.null())
	laddr = msg->getParty()->getLocalAddr();
    if (!lport)
	lport = msg->getParty()->getLocalPort();
    if ((laddr != m_localAddr) || (lport != m_localPort)) {
	Debug(&plugin,DebugInfo,"Detected local address %s:%d for SIP line '%s'",
	    laddr.c_str(),lport,c_str());
	m_localAddr = laddr;
	m_localPort = lport;
	// since local address changed register again in 2 seconds
	m_resend = 2000000 + Time::now();
    }
}

void YateSIPLine::keepalive()
{
    Socket* sock = plugin.ep() ? plugin.ep()->socket() : 0;
    if (sock && m_partyPort && m_partyAddr) {
	SocketAddr addr(PF_INET);
	if (addr.host(m_partyAddr) && addr.port(m_partyPort) && addr.valid()) {
	    Debug(&plugin,DebugAll,"Sending UDP keepalive to %s:%d for '%s'",
		m_partyAddr.c_str(),m_partyPort,c_str());
	    sock->sendTo("\r\n",2,addr);
	}
    }
    m_keepalive = m_alive ? m_alive*(int64_t)1000000 + Time::now() : 0;
}

void YateSIPLine::timer(const Time& when)
{
    if (!m_resend || (m_resend > when)) {
	if (m_keepalive && (m_keepalive <= when))
	    keepalive();
	return;
    }
    m_resend = m_interval*(int64_t)1000000 + when;
    login();
}

void YateSIPLine::clearTransaction()
{
    if (m_tr) {
	DDebug(&plugin,DebugInfo,"YateSIPLine clearing transaction %p [%p]",
	    m_tr,this);
	m_tr->setUserData(0);
	m_tr->deref();
	m_tr = 0;
    }
}

bool YateSIPLine::change(String& dest, const String& src)
{
    if (dest == src)
	return false;
    // we need to log out before any parameter changes
    logout();
    dest = src;
    return true;
}

bool YateSIPLine::change(int& dest, int src)
{
    if (dest == src)
	return false;
    // we need to log out before any parameter changes
    logout();
    dest = src;
    return true;
}

bool YateSIPLine::update(const Message& msg)
{
    DDebug(&plugin,DebugInfo,"YateSIPLine::update() '%s' [%p]",c_str(),this);
    String oper(msg.getValue("operation"));
    if (oper == "logout") {
	logout();
	return true;
    }
    bool chg = false;
    chg = change(m_registrar,msg.getValue("registrar",msg.getValue("server"))) || chg;
    chg = change(m_outbound,msg.getValue("outbound")) || chg;
    chg = change(m_username,msg.getValue("username")) || chg;
    chg = change(m_authname,msg.getValue("authname")) || chg;
    chg = change(m_password,msg.getValue("password")) || chg;
    chg = change(m_domain,msg.getValue("domain")) || chg;
    m_display = msg.getValue("description");
    m_interval = msg.getIntValue("interval",600);
    String tmp(msg.getValue("localaddress",s_auto_nat ? "auto" : ""));
    // "auto", "yes", "enable" or "true" to autodetect local address
    m_localDetect = (tmp == "auto") || tmp.toBoolean(false);
    if (!m_localDetect) {
	// "no", "disable" or "false" to just disable detection
	if (!tmp.toBoolean(true))
	    tmp.clear();
	int port = 0;
	if (tmp) {
	    int sep = tmp.find(':');
	    if (sep > 0) {
		port = tmp.substr(sep+1).toInteger(5060);
		tmp = tmp.substr(0,sep);
	    }
	    else if (sep < 0)
		port = 5060;
	}
	chg = change(m_localAddr,tmp) || chg;
	chg = change(m_localPort,port) || chg;
    }
    m_alive = msg.getIntValue("keepalive",(m_localDetect ? 25 : 0));
    tmp = msg.getValue("operation");
    // if something changed we logged out so try to climb back
    if (chg || (oper == "login"))
	login();
    return chg;
}

YateSIPGenerate::YateSIPGenerate(SIPMessage* m)
    : m_tr(0), m_code(0)
{
    m_tr = plugin.ep()->engine()->addMessage(m);
    if (m_tr) {
	m_tr->ref();
	m_tr->setUserData(this);
    }
    m->deref();
}

YateSIPGenerate::~YateSIPGenerate()
{
    clearTransaction();
}

bool YateSIPGenerate::process(SIPEvent* ev)
{
    DDebug(&plugin,DebugInfo,"YateSIPGenerate::process(%p) %s [%p]",
	ev,SIPTransaction::stateName(ev->getState()),this);
    if (ev->getTransaction() != m_tr)
	return false;
    if (ev->getState() == SIPTransaction::Cleared) {
	clearTransaction();
	return false;
    }
    const SIPMessage* msg = ev->getMessage();
    if (!(msg && msg->isAnswer()))
	return false;
    if (ev->getState() != SIPTransaction::Process)
	return false;
    m_code = msg->code;
    clearTransaction();
    Debug(&plugin,DebugAll,"YateSIPGenerate got answer %d [%p]",
	m_code,this);
    return false;
}

void YateSIPGenerate::clearTransaction()
{
    if (m_tr) {
	DDebug(&plugin,DebugInfo,"YateSIPGenerate clearing transaction %p [%p]",
	    m_tr,this);
	m_tr->setUserData(0);
	m_tr->deref();
	m_tr = 0;
    }
}

bool UserHandler::received(Message &msg)
{
    String tmp(msg.getValue("protocol"));
    if (tmp != "sip")
	return false;
    tmp = msg.getValue("account");
    if (tmp.null())
	return false;
    YateSIPLine* line = plugin.findLine(tmp);
    if (!line)
	line = new YateSIPLine(tmp);
    line->update(msg);
    return true;
}

bool SipHandler::received(Message &msg)
{
    Debug(&plugin,DebugInfo,"SipHandler::received() [%p]",this);
    const char* method = msg.getValue("method");
    String uri(msg.getValue("uri"));
    Regexp r("<\\([^>]\\+\\)>");
    if (uri.matches(r))
	uri = uri.matchString(1);
    if (!(method && uri))
	return false;
    YateSIPLine* line = plugin.findLine(msg.getValue("line"));
    if (line && !line->valid()) {
	msg.setParam("error","offline");
	return false;
    }
    SIPMessage* sip = new SIPMessage(method,uri);
    plugin.ep()->buildParty(sip,msg.getValue("host"),msg.getIntValue("port"),line);
    copySipHeaders(*sip,msg);
    const char* type = msg.getValue("xsip_type");
    const char* body = msg.getValue("xsip_body");
    if (type && body)
	sip->setBody(new SIPStringBody(type,body,-1));
    sip->complete(plugin.ep()->engine(),msg.getValue("user"),msg.getValue("domain"));
    if (!msg.getBoolValue("wait")) {
	// no answer requested - start transaction and forget
	plugin.ep()->engine()->addMessage(sip);
	return true;
    }
    YateSIPGenerate gen(sip);
    while (gen.busy())
	Thread::yield();
    if (gen.code())
	msg.setParam("code",String(gen.code()));
    else
	msg.clearParam("code");
    return true;
}

YateSIPConnection* SIPDriver::findCall(const String& callid)
{
    XDebug(this,DebugAll,"SIPDriver finding call '%s'",callid.c_str());
    Lock mylock(this);
    ObjList* l = channels().skipNull();
    for (; l; l = l->skipNext()) {
	YateSIPConnection* c = static_cast<YateSIPConnection*>(l->get());
	if (c->callid() == callid)
	    return c;
    }
    return 0;
}

YateSIPConnection* SIPDriver::findDialog(const SIPDialog& dialog)
{
    XDebug(this,DebugAll,"SIPDriver finding dialog '%s'",dialog.c_str());
    Lock mylock(this);
    ObjList* l = channels().skipNull();
    for (; l; l = l->skipNext()) {
	YateSIPConnection* c = static_cast<YateSIPConnection*>(l->get());
	if (c->dialog() == dialog)
	    return c;
    }
    return 0;
}

// find line by name
YateSIPLine* SIPDriver::findLine(const String& line)
{
    if (line.null())
	return 0;
    ObjList* l = s_lines.find(line);
    return l ? static_cast<YateSIPLine*>(l->get()) : 0;
}

// find line by party address and port
YateSIPLine* SIPDriver::findLine(const String& addr, int port, const String& user)
{
    if (!(port && addr))
	return 0;
    Lock mylock(this);
    ObjList* l = s_lines.skipNull();
    for (; l; l = l->skipNext()) {
	YateSIPLine* sl = static_cast<YateSIPLine*>(l->get());
	if (sl->getPartyPort() && (sl->getPartyPort() == port) && (sl->getPartyAddr() == addr)) {
	    if (user && (sl->getUserName() != user))
		continue;
	    return sl;
	}
    }
    return 0;
}

// check if a line is either empty or valid (logged in or no registrar)
bool SIPDriver::validLine(const String& line)
{
    if (line.null())
	return true;
    YateSIPLine* l = findLine(line);
    return l && l->valid();
}

bool SIPDriver::received(Message& msg, int id)
{
    if (id == Timer) {
	ObjList* l = s_lines.skipNull();
	for (; l; l = l->skipNext())
	    static_cast<YateSIPLine*>(l->get())->timer(msg.msgTime());
    }
    else if (id == Halt) {
	dropAll(msg);
	channels().clear();
	s_lines.clear();
    }
    return Driver::received(msg,id);
}

bool SIPDriver::msgRoute(Message& msg)
{
    String called = msg.getValue("called");
    if (called.null() || (called.find('@') >= 0))
	return false;
    String line = msg.getValue("line");
    if (line.null())
	line = msg.getValue("account");
    if (line && findLine(line)) {
	// asked to route to a line we have locally
	msg.setParam("line",line);
	msg.retValue() = prefix() + called;
	return true;
    }
    return false;
}

bool SIPDriver::msgExecute(Message& msg, String& dest)
{
    if (!msg.userData()) {
	Debug(this,DebugWarn,"SIP call found but no data channel!");
	return false;
    }
    if (!validLine(msg.getValue("line"))) {
	// asked to use a line but it's not registered
	msg.setParam("error","offline");
	return false;
    }
    YateSIPConnection* conn = new YateSIPConnection(msg,dest,msg.getValue("id"));
    if (conn->getTransaction()) {
	CallEndpoint* ch = static_cast<CallEndpoint*>(msg.userData());
	if (ch && conn->connect(ch,msg.getValue("reason"))) {
	    msg.setParam("peerid",conn->id());
	    msg.setParam("targetid",conn->id());
	    conn->deref();
	    return true;
	}
    }
    conn->destruct();
    return false;
}

SIPDriver::SIPDriver()
    : Driver("sip","varchans"), m_endpoint(0)
{
    Output("Loaded module SIP Channel");
}

SIPDriver::~SIPDriver()
{
    Output("Unloading module SIP Channel");
}

void SIPDriver::initialize()
{
    Output("Initializing module SIP Channel");
    s_cfg = Engine::configFile("ysipchan");
    s_cfg.load();
    s_realm = s_cfg.getValue("general","realm","Yate");
    s_maxForwards = s_cfg.getIntValue("general","maxforwards",20);
    s_privacy = s_cfg.getBoolValue("general","privacy");
    s_auto_nat = s_cfg.getBoolValue("general","nat",true);
    s_inband = s_cfg.getBoolValue("general","dtmfinband",false);
    s_info = s_cfg.getBoolValue("general","dtmfinfo",false);
    s_forward_sdp = s_cfg.getBoolValue("general","forward_sdp",false);
    s_expires_min = s_cfg.getIntValue("registrar","expires_min",EXPIRES_MIN);
    s_expires_def = s_cfg.getIntValue("registrar","expires_def",EXPIRES_DEF);
    s_expires_max = s_cfg.getIntValue("registrar","expires_max",EXPIRES_MAX);
    s_auth_register = s_cfg.getBoolValue("registrar","auth_required",true);
    if (!m_endpoint) {
	m_endpoint = new YateSIPEndPoint();
	if (!(m_endpoint->Init())) {
	    delete m_endpoint;
	    m_endpoint = 0;
	    return;
	}
	m_endpoint->startup();
	setup();
	installRelay(Halt);
	installRelay(Progress);
	installRelay(Update);
	installRelay(Route);
	Engine::install(new UserHandler);
	if (s_cfg.getBoolValue("general","generate"))
	    Engine::install(new SipHandler);
    }
}

}; // anonymous namespace

/* vi: set ts=8 sw=4 sts=4 noet: */
