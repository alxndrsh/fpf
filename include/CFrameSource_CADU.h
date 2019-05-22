/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
FrameSource_CADU.h - CADU (CCSDS transfer frame) framer object

History:
    created: 2015-11 - A.Shumilin.
*/
#ifndef CFRAMESOURCE_CADU_H
#define CFRAMESOURCE_CADU_H

#include "fpf.h"
#include "class_factory.h"
#include "ccsds.h"

#define  FACTORY_CFRAMESOURCE_CADU(c) FACTORY_ADD_FRAMESOURCE(c,CFrameSource_CADU)

//if (c == "CFrameSource_CADU") { return (CFrameSource*) new CFrameSource_CADU(); }

class CFrameSource_CADU : public IFrameSource
{
    public:
        CFrameSource_CADU();
        virtual ~CFrameSource_CADU();
        //IFrameSource implementation
        bool init(t_ini& ini, string& name);
        void start(void);
        void close(void);
    protected:
    private:
        t_uint32 sync_marker;
        size_t   frame_size;
        int bit_shift;
        size_t  buff_size;
        int sync_max_badasm;
        int count_lost_syncs; //count resync events
        int count_frames;
        int c_fillers;
        uint64_t count_read;
        t_file_pos sync_pos_first;
        t_file_pos sync_pos_last;
        bool discard_fill;
        bool trace_sync;
        bool fix_iq;
        int ambiguity_case;

        BYTE* input_buffer;
        IInputStream* input_stream;
        //
        BYTE* acquire_sync(BYTE* buff,size_t bsize);
        BYTE* acquire_sync(BYTE* buff,BYTE* buff_end);
        void run_framing(void);

        int BUFF_PREFIX ; //bytes reserved at the beginning of the buffer, not included in buff_size

};

#endif // CFRAMESOURCE_CADU_H
