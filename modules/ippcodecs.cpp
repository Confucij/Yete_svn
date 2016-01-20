/**
 * ippcodecs.cpp
 * This file is part of the YATE Project http://YATE.null.ro
 *
 * Yet Another Telephony Engine - a fully featured software PBX and IVR
 * Copyright (C) 2004-2014 Null Team
 * Copyright (C) 2006 Mikael Magnusson
 *
 * This software is distributed under multiple licenses;
 * see the COPYING file in the main directory for licensing
 * information for this specific distribution.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <map>
#include <string>
#include <cstdio>

#include <yatephone.h>

#include <ippcore.h>
#include <ipps.h>
#include <usc.h>
#include <usc_objects_decl.h>

#define CODEC_DIRECTION_ENCODE 0
#define CODEC_DIRECTION_DECODE 1
#define CODEC_DIRECTION_TRANSCODE 2

using namespace std;
using namespace TelEngine;

namespace { // anonymous

static Mutex s_cmutex(false, "ippcodecs");
static unsigned int s_codec_cnt;

bool isBusy() {
    bool busy;
    s_cmutex.lock();
    busy = s_codec_cnt ? true : false;
    s_cmutex.unlock();
    return busy;
}

void registerCodecObj(bool reg) {
    s_cmutex.lock();
    if (reg) {
        s_codec_cnt++;
    } else {
        s_codec_cnt--;
    }
    s_cmutex.unlock();
}

/*############################# Class declaration #############################*/

class IPPProxyCodec : public DataTranslator {
    public:
        IPPProxyCodec(const DataFormat& sFormat, const DataFormat& dFormat, USC_Fxns *codec_ptr, USC_Direction direction, int frametype, int framesize);
        ~IPPProxyCodec();
        virtual unsigned long Consume(const DataBlock& data, unsigned long tStamp, unsigned long flags);

        bool isValid() {
            return valid;
        }

    private:
        bool valid;

        USC_Fxns *m_codec_ptr;
        USC_CodecInfo* codec_info_ptr;
        USC_MemBank* mem_banks_ptr;
        USC_Handle handle;
        USC_Direction m_direction;
        int mem_banks_num;

        int m_frametype;
        int m_compressed_frame_size;
        int m_pcm_frame_size;

        DataBlock m_data;
};

class IPPCodecFactory : public TranslatorFactory {

    public:

        IPPCodecFactory(string codec_name, USC_Fxns *codec_ptr, int frametype, int framesize) :
                         TranslatorFactory(codec_name.c_str()), m_codec_ptr(codec_ptr), m_codec_name(codec_name), m_frametype(frametype), m_compressed_frame_size(framesize)
        {
            Debug("IPPCodecFactory", DebugAll, "Creating IPPCodecFactory for %s codec", m_codec_name.c_str());
            m_caps = new TranslatorCaps[3];
            m_caps[0].src  = m_caps[1].dest = FormatRepository::getFormat(m_codec_name.c_str());
            m_caps[0].dest = m_caps[1].src  = FormatRepository::getFormat("slin");
            m_caps[2].src  = m_caps[2].dest = 0;
        }

        inline ~IPPCodecFactory() {
           Debug("IPPCodecFactory", DebugAll, "Clearing %s factory", m_codec_name.c_str());
            delete [] m_caps;
        }

        virtual const TranslatorCaps* getCapabilities() const {
            return m_caps;
        }


        DataTranslator* create(const DataFormat& sFormat, const DataFormat& dFormat) {
            if (sFormat.c_str() == m_codec_name && dFormat.c_str() == string("slin")) {
                IPPProxyCodec *decoder = new IPPProxyCodec(sFormat, dFormat, m_codec_ptr, USC_DECODE, m_frametype, m_compressed_frame_size);
                return decoder->isValid() ? decoder : 0;
            } else if (sFormat.c_str() == string("slin") && dFormat.c_str() == m_codec_name){
                IPPProxyCodec *encoder = new  IPPProxyCodec(sFormat, dFormat, m_codec_ptr, USC_ENCODE, m_frametype, m_compressed_frame_size);
                return encoder->isValid() ? encoder : 0;
            } else {
                return 0;
            }
        }



    private:

        USC_Fxns *m_codec_ptr;
        string m_codec_name;

        TranslatorCaps* m_caps;
        int m_frametype;
        int m_compressed_frame_size;

};


class IPPCodecsPlugin : public Plugin {
    public:
        IPPCodecsPlugin();
        ~IPPCodecsPlugin();
        virtual void initialize();
        virtual bool isBusy() const;

    private:
        IPPCodecFactory *m_g729_factory_ptr;
        IPPCodecFactory *m_g723_factory_ptr;
};

/*############################## Codecs implementation ##############################*/

IPPProxyCodec::IPPProxyCodec(const DataFormat& sFormat, const DataFormat& dFormat, USC_Fxns *codec_ptr, USC_Direction direction, int frametype, int framesize) :
    DataTranslator(sFormat, dFormat), m_codec_ptr(codec_ptr), m_direction(direction), m_frametype(frametype), m_compressed_frame_size(framesize){

    Debug("IPPProxyCodec", DebugAll, "Creating codec from %s to %s", sFormat.c_str(), dFormat.c_str());

    int info_size;
    if(USC_NoError == m_codec_ptr->std.GetInfoSize(&info_size)) {
        codec_info_ptr = new USC_CodecInfo;
        if (USC_NoError == m_codec_ptr->std.GetInfo((USC_Handle) NULL, codec_info_ptr)) {
            codec_info_ptr->params.direction = direction;
            codec_info_ptr->params.modes.vad = 0;
            if(USC_NoError == m_codec_ptr->std.NumAlloc(&codec_info_ptr->params, &mem_banks_num)) {
                mem_banks_ptr = new USC_MemBank[mem_banks_num];
                if (USC_NoError == m_codec_ptr->std.MemAlloc(&codec_info_ptr->params, mem_banks_ptr)) {
                    for (int i = 0; i < mem_banks_num; i++) {
                        mem_banks_ptr[i].pMem = new char[mem_banks_ptr[i].nbytes];
                    }
                    if (USC_NoError == m_codec_ptr->std.Init(&codec_info_ptr->params, mem_banks_ptr, &handle)) {
                        if (USC_NoError == m_codec_ptr->std.GetInfo(handle, codec_info_ptr)) {
                            m_pcm_frame_size = codec_info_ptr->params.framesize;
                            valid = true;
                            registerCodecObj(true);
                            return;
                        }
                    }
                    for (int i = 0; i < mem_banks_num; i++) {
                        delete[] mem_banks_ptr[i].pMem;
                    }
                }
                delete [] mem_banks_ptr;
            }
        }
        delete [] codec_info_ptr;
    }
    valid = false;
    return;
}

IPPProxyCodec::~IPPProxyCodec() {
    Debug("IPPProxyCodec", DebugAll,"IPPProxyCodec::~IPPProxyCodec() [%p]",this);
    for (int i = 0; i < mem_banks_num; i++) {
        delete [] mem_banks_ptr[i].pMem;
    }
    delete [] mem_banks_ptr;
    delete codec_info_ptr;
    registerCodecObj(false);

}

unsigned long IPPProxyCodec::Consume(const DataBlock& data, unsigned long tStamp, unsigned long flags) {

    if (!getTransSource()) {
        return 0;
    }

    if (data.null() || (flags & DataSilent)) {
        return getTransSource()->Forward(data, tStamp, flags);
    }

    if (!ref()) {
        return 0;
    }

    m_data += data;
    DataBlock out_data;

    USC_Status r;
    int frames = 0;
    int consumed = 0;

    if (m_direction == USC_ENCODE) {

        frames = m_data.length() / m_pcm_frame_size;
        consumed = frames * m_pcm_frame_size;

        if (frames) {
            USC_PCMStream in;
            USC_Bitstream out;

            in.bitrate = codec_info_ptr->params.modes.bitrate;
            in.pcmType = codec_info_ptr->params.pcmType;

            out_data.assign(0, frames * m_compressed_frame_size);
            for (int i = 0; i < frames; i++) {
                in.nbytes = codec_info_ptr->params.framesize;
                char *decompressed_buffer = ((char *) (m_data.data()) + (i * m_pcm_frame_size));
                char *compressed_buffer = ((char *) (out_data.data()) + (i * m_compressed_frame_size));
                in.pBuffer = decompressed_buffer;
                out.pBuffer = compressed_buffer;
                r = m_codec_ptr->Encode(handle, &in, &out);
                if (USC_NoError != r) {
                    Debug("IPPCodecs", DebugWarn, "Encode error:%d", r);
                    frames = 0;
                    break;
                }
            }
            XDebug("IPPProxyCodec", DebugAll, "Encode@%lu  in: %dbytes, out: %ubytes", tStamp, out.nbytes, in.nbytes);
        }
    } else {
        frames = m_data.length() / m_compressed_frame_size;
        consumed = frames * m_compressed_frame_size;

        if (frames) {
            USC_Bitstream in;
            USC_PCMStream out;

            out_data.assign(0, frames * codec_info_ptr->params.framesize);
            in.bitrate = codec_info_ptr->params.modes.bitrate;

            for (int i = 0; i < frames; i++) {
                char* compressed_buffer = (char *)((char *)(m_data.data()) + (i * m_compressed_frame_size));
                char* decompressed_buffer = (char *)((char *)(out_data.data()) + (i * m_pcm_frame_size));

                in.pBuffer = compressed_buffer;
                in.nbytes = m_compressed_frame_size;
                in.frametype = m_frametype;
                out.pBuffer = decompressed_buffer;

                r = m_codec_ptr->Decode(handle, &in, &out);
                if (USC_NoError != r) {
                    Debug("IPPCodecs", DebugWarn, "Decode error:%d", r);
                    frames = 0;
                    break;
                } else {
                    XDebug("IPPCodecs", DebugAll, "Decode@%lu  in: %dbytes, out: %ubytes", tStamp, out.nbytes, in.nbytes);
                }
            }
        }
    }

    unsigned long len = 0;

    if (frames) {
        m_data.cut(-consumed);
        len = getTransSource()->Forward(out_data, tStamp, flags);
    }

    deref();
    return len;

}

/*########################### Public plugin class ################################*/
IPPCodecsPlugin::IPPCodecsPlugin(): Plugin("ippcodecs"){

    Output("Loaded module IppCodecs - codecs based on Intel IPP + IPP samples");

    FormatRepository::addFormat("g723", 24, 1000 * 240 / 8);
    m_g723_factory_ptr = new IPPCodecFactory("g723", &USC_G723_Fxns, 0, 24);

    FormatRepository::addFormat("g729", 10, 80 * 1000 / 8);
    m_g729_factory_ptr = new IPPCodecFactory("g729", &USC_G729I_Fxns, 3, 10);

}

IPPCodecsPlugin::~IPPCodecsPlugin() {

    Output("Unloading module IppCodecs");

    TelEngine::destruct(m_g729_factory_ptr);
    TelEngine::destruct(m_g723_factory_ptr);

}

void IPPCodecsPlugin::initialize() {

    Output("Initialize module IppCodecs");

}

bool IPPCodecsPlugin::isBusy() const {

    return isBusy();
}

INIT_PLUGIN(IPPCodecsPlugin);

}; // anonymous namespace

