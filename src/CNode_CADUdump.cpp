/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
CNode_CADUdump - dumper of TF (CADU) frame structures

History:
    created: 2015-11 - A.Shumilin.
*/

#include <iostream>
#include <cstdlib> /* atoi */
#include <iomanip>

#include "ini.h"
#include "CNode_CADUdump.h"
#include "class_factory.h"
#include "ccsds.h"


#define MY_CLASS_NAME   "CNode_CADUdump"

// INI tags
#define INI_DUMP_EVERY  "dump_every"
#define INI_FORMAT   "format"
#define INI_FORMAT_VAL_LNSHORT  "line_short"
#define INI_FORMAT_VAL_LNLONG  "line_long"
#define INI_FORMAT_VAL_BLOCK  "block"
#define INI_USEHEX "use_hex"
#define INI_DUMP_TIMES  "dump_times"
#define INI_SAVE_TO "save_to"

CNode_CADUdump::CNode_CADUdump()
{
    is_initialized = false;
    pnext_node = NULL;
    output = &cout;
    dump_format = FORMAT_LINES_LONG;
    use_hex = false;
    c_counter = 0; c_dumped = 0;
    dump_times = false;
    trace_every = 1;
    tf_version = 0;
}

CNode_CADUdump::~CNode_CADUdump()
{

}

bool CNode_CADUdump::init(t_ini& ini, string& init_name, CChain* pchain_arg)
{
     *fpf_debug << "~ " MY_CLASS_NAME "::init("<<init_name<<")\n";
    pchain = pchain_arg;
    is_initialized = false;
    //
    t_ini_section conf = ini[init_name];
    name = init_name;
    id = name;
    c_counter = 0; c_dumped = 0;
    // -------------- do custom initialization here -------------------
    if (conf.find(INI_DUMP_EVERY) != conf.end())
    { trace_every = atoi(conf[INI_DUMP_EVERY].c_str()); }
    if (conf[INI_FORMAT] == INI_FORMAT_VAL_LNSHORT) {dump_format = FORMAT_LINES_SHORT;}
    else if (conf[INI_FORMAT] == INI_FORMAT_VAL_LNLONG) {dump_format = FORMAT_LINES_LONG;}
    else if (conf[INI_FORMAT] == INI_FORMAT_VAL_BLOCK) {dump_format = FORMAT_BLOCK;}
    else if (!conf[INI_FORMAT].empty()) { *fpf_warn << "!! WARN: " MY_CLASS_NAME "("<<name<<")::init unknown format value: "<<conf[INI_FORMAT]<<"\n";}
    use_hex = false;
    if (conf[INI_USEHEX] ==INI_COMMON_VAL_YES ) {use_hex = true;}
    if (conf[INI_DUMP_TIMES] ==INI_COMMON_VAL_YES ) {dump_times = true;}
    //
    if (!conf[INI_SAVE_TO].empty())
    { //setup output stream
        outfile_name = conf[INI_SAVE_TO];
        outfile.open(outfile_name.c_str());
        if (outfile.is_open())
        {
            output = &outfile;
        }else{
            *fpf_error<<"ERROR: " MY_CLASS_NAME "("<<name<<")::init failed to open output file ["<<outfile_name<<"\n";

        }
    }
    //build next node
    bool no_next_node;  pnext_node = get_next_node(ini,name,&no_next_node);
	if (pnext_node == NULL)
	{if (!no_next_node) {*fpf_error << "ERROR:  "MY_CLASS_NAME"::init("<<init_name<<") failed to create next node ["<< conf[INI_COMMON_NEXT_NODE] <<"]\n";	return false;	}}
    else  {  if (! pnext_node->init(ini,conf[INI_COMMON_NEXT_NODE],pchain_arg))  { return false;}   }
    //
    *fpf_trace<<"=> " MY_CLASS_NAME "::init("<<name<<") initialized, output to "<<outfile_name<<"\n";
    is_initialized = true;
    return true;
}

void CNode_CADUdump::start(void)
{
}

void CNode_CADUdump::stop(void)
{
}

void CNode_CADUdump::close(void)
{
    if (pnext_node != NULL)   {  pnext_node->close();   delete pnext_node;   pnext_node = NULL;   }
    if ((output != &cout) && outfile.is_open()) { outfile.close(); output = &cout; }
    is_initialized = false;
    *fpf_trace<<"<= " MY_CLASS_NAME "("<<name<<") closed, dumped "<<c_dumped<<"  frames from "<<c_counter<<"\n";
}

void CNode_CADUdump::take_frame(CFrame* pf)
{
    FPF_ASSERT( (pf!= NULL),"NULL frame pointer")
    FPF_ASSERT(is_initialized,"Node not initialized")
    // custom node actions
    do_frame_processing(pf);
    //pass the frame further by chain
    if (pnext_node != NULL) { pnext_node->take_frame(pf); }
}



void CNode_CADUdump::do_frame_processing(CFrame* pf)
{
    c_counter++;
    if (trace_every ==0)  {return; }
    if (trace_every>1 && (c_counter % trace_every != 1)) {return; }
    c_dumped ++;
    //
    CADU* cadu = (CADU*) pf->pdata;
    BYTE* pcadu =  pf->pdata;

    //--determine TF version

    unsigned int sc;
    unsigned int vcid;
    unsigned int vccount;
    unsigned int mccount;
    int frame_tf_version = CADU_GET_TF_VERSION(pcadu); //TODO - add explicit setup
    if (tf_version>0) {frame_tf_version = tf_version;}
    switch (frame_tf_version)
    {
        case 0:
            sc = TFV1_GET_SPACECRAFT(pcadu);
            vcid = TFV1_GET_VCID(pcadu);
            vccount = TFV1_GET_VCCOUNTER(pcadu);
            mccount = TFV1_GET_MCCOUNTER(pcadu);
            break;
        case 1:
            sc = CADU_GET_SPACECRAFT(pcadu);
            vcid = CADU_GET_VCID(pcadu);
            vccount = TFV1_GET_VCCOUNTER(pcadu);
             mccount = 0;
            break;
        default:
            FPF_ASSERT( false,"Invalid tf_version")
    }

    // -- prepare values
    string spacecraft = SPACECRAFT_NAMES[(int)(CADU_GET_SPACECRAFT(pcadu))];
    string vcname = VCID_NAMES[vcid];
    string crc = "NA";
    if (pf->crc_ok == FRAME_CRC_OK) {crc = "OK";}
    else if (pf->crc_ok >= FRAME_CRC_FAILED ) {crc = "FAIL";}
    char sz[40];tm* ptm;
    string wc_time="-                 -";
    if (pf->wctime > 0)
    { ptm = gmtime(&(pf->wctime)); strftime (sz,sizeof(sz),"%Y-%m-%d %H:%M:%S",ptm);wc_time=sz;}
    string ac_time="-                 -";
    if (pf->actime > 0)
    { ptm = gmtime(&(pf->actime)); strftime (sz,sizeof(sz),"%Y-%m-%d %H:%M:%S",ptm);wc_time=sz;}
    string ob_time="-                 -";
    if (pf->obtime > 0)
    { ptm = gmtime(&(pf->obtime));
    strftime (sz,sizeof(sz),"%Y-%m-%d %H:%M:%S",ptm);
    ob_time=sz;}


    //
    switch (dump_format )
    {
    case FORMAT_BLOCK:
        *output<<name<<"-CADU ["<<setw(8)<<right<<c_counter<<"]["<<setw(8)<<right<<pf->cframe<<"] at ["<<pf->stream_pos<<"]\n";
        *output<<setw(8)<<right;
        *output<<"  Sync Marker:\t"<< hex<<cadu->ASM<<endl;
        *output<<"      version:\t"<<dec<<setw(8)<<right<< (int)(cadu->version) <<" [0x"<<hex<< (int)(cadu->version)<<"]"<<endl;
        *output<<"   spacecraft:\t"<<dec<<setw(8)<<right<< sc  <<" [0x"<<hex<< (int)(sc)<<"]"  <<"  ("<<spacecraft<<")"<<endl;
        *output<<"         VCID:\t"<<dec<<setw(8)<<right<<vcid <<" [0x"<<hex<<vcid<<"]"<<" ("<<vcname<<")"<<endl;
        *output<<"   VCDU count:\t"<<dec<<setw(9)<<right<< vccount <<" [0x"<<hex<<vccount<<"]"<<endl;
        *output<<"          CRC:\t"<<crc<< ", err.bits: "<<dec<<setw(8)<<right<< pf->bit_errors <<endl;
        if (dump_times)
        {
        *output<<"   board time:\t"<<ob_time<< " [ "<<pf->obtime<<"]\n";
        *output<<"    acq. time:\t"<<ac_time<< " [ "<<pf->actime<<"]\n";
        *output<<"   wall clock:\t"<<wc_time<< " [ "<<pf->wctime<<"]\n";
        }
        break;
    case FORMAT_LINES_SHORT:
        if (use_hex) { *output << hex;}
        *output<<name<<"-CADU ["<<setw(8)<<right<<c_counter<<"] at ["<<setw(10)<<pf->stream_pos<<"]  sc:"<<setw(4)<<right<< sc
                <<"\tVCID:"<<setw(4)<<right<< vcid
                <<"\tcount:"<<setw(9)<<right<< vccount
                <<"\tCRC:"<<crc<< ", err: "<<dec<<setw(3)<<right<< pf->bit_errors;
        if (dump_times)
        {
             *output<<"\tactime:"<<ac_time
                <<"\tobtime:"<<ob_time;
        }
               *output <<endl;
                break;
    case FORMAT_LINES_LONG:
        *output<<name<<"-CADU ["<<setw(8)<<right<<c_counter<<"]["<<setw(8)<<right<<pf->cframe<<"] at ["<<setw(10)<<pf->stream_pos<<"]  sc:"<<setw(4)<<right<< sc << "("<<spacecraft<<")"
                <<"\tVCID:"<<setw(4)<<right<< vcid<<"("<<vcname<<")";
        if (frame_tf_version == 0) { *output<<"\tmccount:"<<setw(9)<<right<< mccount ;}
         *output<<"\tcount:"<<setw(9)<<right<< vccount
                <<"\tCRC:"<<crc<< ", err: "<<dec<<setw(3)<<right<< pf->bit_errors;
        if (dump_times)
        {
           *output<<"\tactime:"<<ac_time
                 <<"\tobtime:"<<ob_time
                 <<"\twctime:"<<wc_time;
        }
                *output<<endl;
                break;
    }// switch (dump_format
    //restore default formats
    *output<<dec<<left;
}



