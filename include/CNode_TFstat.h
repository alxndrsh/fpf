/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
CNode_TFstat - node collecting transfer frame level statistics

History:
    created: 2016-06 - A.Shumilin.
*/

#ifndef CNode_TFStat_H
#define CNode_TFStat_H

#include <fstream>
#include "fpf.h"


#define  FACTORY_CNODE_TFSTAT(c) FACTORY_ADD_NODE(c,CNode_TFstat)


class CNode_TFstat : public INode
{
    enum t_dump_formats { FORMAT_LINES_SHORT = 1 , FORMAT_LINES_LONG, FORMAT_BLOCK };
    enum ts_type { TS_NOT_DEFINED = 0, TS_WCT,TS_ACT,TS_OBT };

    public:
        CNode_TFstat();
        virtual ~CNode_TFstat();
        //INode
        bool init(t_ini& ini, string& name, CChain* chain);
        void start(void);
        void stop(void);
        void close(void);
        void take_frame(CFrame* pf);
    protected:
    private:
        ostream* output;
        ofstream outfile;
        unsigned long c_counter;
        string outfile_name;
        bool lazy_create;
        // formatting options
        ts_type format_ts ;
        //
        int block_frames;
        //block counters
        int c_blk_frames;
        int c_blk_fillers;
        int c_blk_failed_crc;
        int c_blk_biterrors;
        int c_blk_biterrors_valid;
        int c_blk_failed_sync;
        size_t blk_start_frame;
        size_t blk_start_stream_frame;
        size_t blk_start_pos;
        time_t blk_obt;
        //
        string report_header; //custom, user defined , report header line
    //
    void do_frame_processing(CFrame* pf);
    void build_file_name(CFrame* pf);


};
#endif // CNode_TFStat_H
