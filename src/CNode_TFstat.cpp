/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
CNode_TFstat - node collecting transfer frame level statistics

History:
    created: 2016-06 - A.Shumilin.
*/

#include <iostream>
#include <sstream>
#include <cstring> /* memcpy */
#include <cstdlib> /* atoi */
#include <iomanip>

#include "ini.h"
#include "CNode_TFstat.h"
#include "class_factory.h"
#include "ccsds.h"


#define MY_CLASS_NAME   "CNode_TFstat"

// INI tags

#define INI_SAVE_TO   "save_to"
#define INI_LAZY_CREATE  "lazy_create"
#define INI_INVREPORT_HEADER  "report_header"
#define INI_BLOCK_FRAMES  "block_frames"
#define INI_TS  "timestamps"
#define INI_TS_VAL_WCT   "wct"
#define INI_TS_VAL_ACT   "act"
#define INI_TS_VAL_OBT   "obt"


#define INI_SUBS_NAME   "NAME"
#define INI_SUBS_ID     "ID"
#define INI_STREAM_URL  "STREAMURL"
#define INI_STREAM_ID   "STREAMID"
#define INI_STREAM_OBT  "STREAMOBT"

CNode_TFstat::CNode_TFstat()
{
    is_initialized = false;
    pnext_node = NULL;
    output = NULL;
    lazy_create = false;
    block_frames = 1000;
    c_counter = 0;
    format_ts = TS_NOT_DEFINED;
}

CNode_TFstat::~CNode_TFstat()
{

}

bool CNode_TFstat::init(t_ini& ini, string& init_name, CChain* pchain_arg)
{
     *fpf_debug << "~ " MY_CLASS_NAME "::init("<<init_name<<")\n";
    pchain = pchain_arg;
    is_initialized = false;
    //
    t_ini_section conf = ini[init_name];
    name = init_name;
    id = name;
    // -------------- do custom initialization here -------------------

    c_counter = 0;
    format_ts = TS_NOT_DEFINED;

    //
    report_header = conf[INI_INVREPORT_HEADER];

    //
    block_frames = 1000;
    if (! conf[INI_BLOCK_FRAMES].empty() )
    {
         block_frames = atoi(conf[INI_BLOCK_FRAMES].c_str());
    }
    //
    if (! conf[INI_TS].empty() )
    {
         if (conf[INI_TS] == INI_TS_VAL_WCT) {format_ts = TS_WCT;}
         else if (conf[INI_TS] == INI_TS_VAL_ACT) {format_ts = TS_ACT;}
         else if (conf[INI_TS] == INI_TS_VAL_OBT) {format_ts = TS_OBT;}
         else { *fpf_error<<"WARNING:" MY_CLASS_NAME "("<<name<<")::init - invalid timestamp format value"; }
    }
    //
    lazy_create = false;
    outfile_name = "-";
    if (conf[INI_LAZY_CREATE] == INI_COMMON_VAL_YES) { lazy_create = true; }
    if (!conf[INI_SAVE_TO].empty())
    { //setup output stream
        outfile_name = conf[INI_SAVE_TO];
        if (outfile_name == "-") { output = &cout; }
        else
        {
            if (!lazy_create)
            {//open it now
                outfile.open(outfile_name.c_str());
                if (outfile.is_open())
                {
                    output = &outfile;
                }else{
                    *fpf_error<<"ERROR: " MY_CLASS_NAME "("<<name<<")::init failed to open output file ["<<outfile_name<<"\n";
                    return false;
                }
            }
        }
    }else { output = &cout; } //default output to screen
    //

    //build next node
    bool no_next_node;  pnext_node = get_next_node(ini,name,&no_next_node);
	if (pnext_node == NULL)
	{if (!no_next_node) {*fpf_error << "ERROR:  " MY_CLASS_NAME "::init("<<init_name<<") failed to create next node ["<< conf[INI_COMMON_NEXT_NODE] <<"]\n";	return false;	}}
    else  {  if (! pnext_node->init(ini,conf[INI_COMMON_NEXT_NODE],pchain_arg))  { return false;}   }
    //
    string str_lazy = (lazy_create)? "(lazy) " : "";
    *fpf_trace<<"=> " MY_CLASS_NAME "("<<name<<")::init initialized with output "<<block_frames<< "block frames to "<<str_lazy<<outfile_name<<"\n";
    is_initialized = true;
    return true;
}

void CNode_TFstat::start(void)
{
}

void CNode_TFstat::stop(void)
{
}

void CNode_TFstat::close(void)
{
    //
    if (outfile.is_open()) { outfile.close(); output = NULL;}
    //
    if (pnext_node != NULL)   {  pnext_node->close();   delete pnext_node;   pnext_node = NULL;   }
    is_initialized = false;
    *fpf_trace<<"<= " MY_CLASS_NAME "("<<name<<") closed, \n";
}

void CNode_TFstat::take_frame(CFrame* pf)
{
    FPF_ASSERT( (pf!= NULL),"NULL frame pointer")
    FPF_ASSERT(is_initialized,"Node not initialized")
    // custom node actions
    do_frame_processing(pf);
    //pass the frame further by chain
    if (pnext_node != NULL) { pnext_node->take_frame(pf); }
}

void CNode_TFstat::build_file_name(CFrame* pf) //resolve file name, taking data from the frame and its stream
{
    if (string::npos == outfile_name.find('%') ) { return; }//nothing to resolve
    map<string,string> subs_map;
    subs_map[INI_SUBS_NAME] = name;
    subs_map[INI_SUBS_ID] = id;
    char sz[40]; tm* ptm = gmtime(&(pf->pstream->obt_base));
    strftime (sz,sizeof(sz),"%Y%m%d%H%M%S",ptm);
    subs_map[INI_STREAM_OBT] = string(sz);
    //
    make_substitutions(outfile_name, &subs_map,'%');
}

static int mod_diff(int x1,int x2, int base) // (x2-x1)%base (0 based numbers)
{
    int d = x2-x1;
    if (d<0)  { d=base+d; }
    return d % base;
}

void CNode_TFstat::do_frame_processing(CFrame* pf)
{
    c_counter++;
    //

    // -- prepare values
    if ((output == NULL) && lazy_create && (c_counter == 1)) //do only once
    {//open it now
            build_file_name(pf);
            outfile.open(outfile_name.c_str());
            if (outfile.is_open())
            {
                output = &outfile;
                *fpf_trace<<"=> " MY_CLASS_NAME "("<<name<<") does lazy open of ["<<outfile_name<<"]\n";
            }else{
                *fpf_error<<"ERROR: " MY_CLASS_NAME "("<<name<<") failed lazy open of ["<<outfile_name<<"]\n";
            }
    }
    if (output == NULL)  { output = &cout; } //output to somewhere anyway
    //
    if (c_counter == 1) //init output header
    {
        std::ostringstream ss_header;
        ss_header << "#"<<report_header <<"\n";
        ss_header << "#Transfer frames stat report\n";
        ss_header << "#data file: "<<pf->pstream->url<<"\n";
        ss_header << "#frames per stat record: "<<block_frames<<"\n";
        time_t wct;time ( &wct );
        struct tm* ptm;
        ptm = gmtime( &wct );
        char szx[40];  strftime (szx,sizeof(szx),"%Y-%m-%dT%H:%M:%SZ",ptm);
        ss_header << "#created at: "<<szx<<"\n";
        ss_header << "#fields: [ frame counter] at [file position], filler frames, failed sync, failed CRC, error bits\n";
        //ss_header << "#fields: slice_id,ref_time,file_pos,slice_size,num_frames,num_errors\n";
        ss_header << "#\n";
        #
        report_header = ss_header.str();
        *output << report_header;

    }
    //
    c_blk_frames++;

    //init block counters and initial values
    if (c_blk_frames == 1)
    {
        c_blk_fillers=0;
        c_blk_failed_crc=0;
        c_blk_biterrors=0;
        c_blk_biterrors_valid = 0;
        c_blk_failed_sync = 0;
        blk_obt = 0;
        blk_start_frame =c_counter;
        blk_start_stream_frame = pf->cframe;
        blk_start_pos = pf->stream_pos;
    }
    //collect statistics
    if (pf->bit_errors >= 0) { c_blk_biterrors += pf->bit_errors; c_blk_biterrors_valid++; }
    if (pf->crc_ok == FRAME_CRC_FAILED) { c_blk_failed_crc ++; }
    if (pf->vcid == VCID_FILL)
    {
            c_blk_fillers ++;
    }
   if (pf->obtime != 0) //take first reasonable OBT (may be present far not in every frame)
   {
       blk_obt = pf->obtime;
   }
    //--dump record

    if (c_blk_frames >= block_frames)
    {
        char sz[40];tm* ptm;

        // dump block record stat
         *output<<"TF ["<<setw(8)<<right<<blk_start_frame<<"] at ["<<setw(10)<<blk_start_pos<<"]"
                <<"\t"<<setw(6)<<right<< c_blk_fillers
                <<"\t"<<setw(6)<<c_blk_failed_sync
                <<"\t"<<setw(6)<<c_blk_failed_crc
                <<"\t"<<dec<<setw(8)<<right<< c_blk_biterrors;
        if (format_ts == TS_OBT)
        {
            string str_time="-";
            if (blk_obt > 0)
            { ptm = gmtime(&(blk_obt));strftime (sz,sizeof(sz),"%Y-%m-%d %H:%M:%S",ptm);  str_time=sz;}
            *output<<"\t"<<str_time;
        }
        //endln
        *output<<"\n";
        //reset block counters
        c_blk_frames=0;
        // other block init actions are done at first frame in block arrival

    }

}






