/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
CNode_EOSinv - EOS PDS file )packet level) inventory node
(provides CSAIS service compatible inventory reports)

History:
    created: 2015-11 - A.Shumilin.
*/

#ifndef CNode_EOSinv_H
#define CNode_EOSinv_H

#include <fstream>
#include "fpf.h"

#ifdef USE_CURL
extern "C" {
#include <curl/curl.h>
}
#endif

#define  FACTORY_CNODE_EOSINV(c) FACTORY_ADD_NODE(c,CNode_EOSinv)

class CNode_EOSinv : public INode
{
    enum t_dump_formats { FORMAT_LINES_SHORT = 1 , FORMAT_LINES_LONG, FORMAT_BLOCK };

    public:
        CNode_EOSinv();
        virtual ~CNode_EOSinv();
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
    //
    void do_frame_processing(CFrame* pf);
    void build_file_name(CFrame* pf);
    bool online_post(string str_report,string url);

    string post_to_url;
    string post_to_url_nrt;
    string  full_report;
    string  report_header;

#ifdef USE_CURL
    CURL *curl;
#else
    void *curl; //dummy pointer to be used without CURL
#endif


};
#endif // CNode_EOSinv_H
