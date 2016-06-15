/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
FrameSource_PDS.h - CCSDS source packet stream ( EOS PDS like format) framer object

History:
    created: 2015-11 - A.Shumilin.
*/
#ifndef CFrameSource_PDS_H
#define CFrameSource_PDS_H

#include "fpf.h"
#include "class_factory.h"
#include "ccsds.h"

#define  FACTORY_CFRAMECOURCE_PDS(c) FACTORY_ADD_FRAMESOURCE(c,CFrameSource_PDS)

//if (c == "CFrameSource_PDS") { return (CFrameSource*) new CFrameSource_PDS(); }

class CFrameSource_PDS : public IFrameSource
{
    public:
        CFrameSource_PDS();
        virtual ~CFrameSource_PDS();
        //IFrameSource implementation
        bool init(t_ini& ini, string& name);
        void start(void);
        void close(void);
    protected:
    private:
        size_t   frame_size;
        size_t  buff_size;
        vector<int> valid_sizes;
        vector<int> valid_apids;
         int obt_epoch;

        int count_lost_syncs; //count resync events
        int count_frames;
        int count_invalid_sizes;
        int count_invalid_apids;
        int count_skipped_packets;
        size_t count_read;
        t_file_pos sync_pos_first;
        t_file_pos sync_pos_last;

        BYTE* input_buffer;
        IInputStream* input_stream;
        //
        void run_framing(void);
        bool good_sp_header(BYTE* phead);


};

#endif // CFrameSource_PDS_H
