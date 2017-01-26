/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
CNode_RSSPipe - Upload chunks of frame stream to HTTP server (like RSS-Pipe.com)

History:
    created: 2017-01 - A.Shumilin.
*/

#ifndef CNode_RSSPipe_H
#define CNode_RSSPipe_H

#include <fstream>
#include "fpf.h"

#ifdef USE_CURL
extern "C" {
#include <curl/curl.h>
}
#endif

#define  FACTORY_CNODE_RSSPIPE(c) FACTORY_ADD_NODE(c,CNode_RSSPipe)

class CNode_RSSPipe : public INode
{

    public:
        CNode_RSSPipe();
        virtual ~CNode_RSSPipe();
        //INode
        bool init(t_ini& ini, string& name, CChain* chain);
        void start(void);
        void stop(void);
        void close(void);
        void take_frame(CFrame* pf);
    protected:
    private:
        unsigned long c_counter;
        int base_apid;
        int packets_in_slice;
        int c_missing_packets;
        int obt_epoch;
        string slice_id_prefix;
        string inv_report_header;
        string inv_report_datatype;
        string inv_report_url;
        string inv_report_satellite;
        //
        int prev_pcount;
    int c_slice_missing;
    int c_slice_packets;
    int slice_base;
    size_t slice_start_pos;
    int c_slices;
    string slice_start_time;
    string slice_start_time_long;

    BYTE* pBuff_base;
    BYTE* pBuff_end;
    //
    void do_frame_processing(CFrame* pf);
    bool online_post(string str_report,string url);

    string post_to_url;
    string  full_report;
    string  report_header;

#ifdef USE_CURL
    CURL *curl;
#else
    void *curl; //dummy pointer to be used without CURL
#endif


};
#endif // CNode_RSSPipe_H
