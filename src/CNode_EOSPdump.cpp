/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
CNode_EOSPdump - node  dumping header of source packets (EOS style headers in first place)

History:
    created: 2015-11 - A.Shumilin.
*/

#include <iostream>
#include <cstdlib> /* atoi */
#include <iomanip>

#include "ini.h"
#include "CNode_EOSPdump.h"
#include "class_factory.h"
#include "ccsds.h"


#define MY_CLASS_NAME   "CNode_EOSPdump"

// INI tags
#define INI_DUMP_EVERY  "dump_every"
#define INI_FORMAT   "format"
#define INI_FORMAT_VAL_LNSHORT  "line_short"
#define INI_FORMAT_VAL_LNLONG  "line_long"
#define INI_FORMAT_VAL_BLOCK  "block"
#define INI_SAVE_TO             "save_to"
#define INI_OBT_EPOCH      "obt_epoch"


CNode_EOSPdump::CNode_EOSPdump()
{
    is_initialized = false;
    pnext_node = NULL;
    output = &cout;
    dump_format = FORMAT_LINES_SHORT;
    c_counter = 0;
    c_dumped = 0;
    dump_every = 0;
    obt_epoch = EOS_TIME_EPOCH;
}

CNode_EOSPdump::~CNode_EOSPdump()
{

}

bool CNode_EOSPdump::init(t_ini& ini, string& init_name, CChain* pchain_arg)
{
     *fpf_debug << "~ " MY_CLASS_NAME "::init("<<init_name<<")\n";
    pchain = pchain_arg;
    is_initialized = false;
    //
    t_ini_section conf = ini[init_name];
    name = init_name;
    id = name;

    //build next node
    bool no_next_node;  pnext_node = get_next_node(ini,name,&no_next_node);
	if (pnext_node == NULL)
	{if (!no_next_node) {*fpf_error << "ERROR:  "MY_CLASS_NAME"::init("<<init_name<<") failed to create next node ["<< conf[INI_COMMON_NEXT_NODE] <<"]\n";	return false;	}}
    else  {  if (! pnext_node->init(ini,conf[INI_COMMON_NEXT_NODE],pchain_arg))  { return false;}   }
    //
    c_dumped = 0;
    // -------------- do custom initialization here -------------------
    obt_epoch = EOS_TIME_EPOCH;
    if (! conf[INI_OBT_EPOCH].empty())
    {
		obt_epoch =  strtol(conf[INI_OBT_EPOCH].c_str(),NULL,10);
	}
    if (conf.find(INI_DUMP_EVERY) != conf.end())
    {
        dump_every = atoi(conf[INI_DUMP_EVERY].c_str());
    }
    if (conf[INI_FORMAT] == INI_FORMAT_VAL_LNSHORT) {dump_format = FORMAT_LINES_SHORT;}
    else if (conf[INI_FORMAT] == INI_FORMAT_VAL_LNLONG) {dump_format = FORMAT_LINES_LONG;}
    else if (conf[INI_FORMAT] == INI_FORMAT_VAL_BLOCK) {dump_format = FORMAT_BLOCK;}
    else if (!conf[INI_FORMAT].empty()) { *fpf_warn << "!! WARN: " MY_CLASS_NAME "("<<name<<")::init unknown format value: "<<conf[INI_FORMAT]<<"\n";}
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
    //
    *fpf_trace<<"=> " MY_CLASS_NAME "("<<name<<")::init initialized\n";
    is_initialized = true;
    return true;
}

void CNode_EOSPdump::start(void)
{
}

void CNode_EOSPdump::stop(void)
{
}

void CNode_EOSPdump::close(void)
{
    if (pnext_node != NULL)   {  pnext_node->close();   delete pnext_node;   pnext_node = NULL;   }
    is_initialized = false;
    if ((output != &cout) && outfile.is_open()) { outfile.close(); output = &cout; }
    *fpf_trace<<"<= " MY_CLASS_NAME "("<<name<<") closed, dumped "<<c_dumped<<"  frames from "<<c_counter<<"\n";
}

void CNode_EOSPdump::take_frame(CFrame* pf)
{
    FPF_ASSERT( (pf!= NULL),"NULL frame pointer")
    FPF_ASSERT(is_initialized,"Node not initialized")
    // custom node actions
    do_frame_processing(pf);
    //pass the frame further by chain
    if (pnext_node != NULL) { pnext_node->take_frame(pf); }
}



void CNode_EOSPdump::do_frame_processing(CFrame* pf)
{
    c_counter++;
    if (dump_every == 0) {return; }
    else if (dump_every>1 && (c_counter % dump_every != 1)) {return; }
    c_dumped ++;
    //
    BYTE *ph = pf->pdata;
    // -- prepare values

    //
    t_uint8 ui8; t_uint16 ui16;
    int tday = SP_GET_EOS_TS_DAYS(ph);
    int tmsec = SP_GET_EOS_TS_MSEC(ph);
    time_t utime = (tday*FPF_TIME_SEC_PER_DAY + obt_epoch) + int(tmsec / 1000);
    //int usec = (SP_GET_EOS_TS_MSEC(ph) % 1000) * 1000 + SP_GET_EOS_TS_USEC(ph);
    char sz[40];
    tm* ptm = gmtime(&utime);
    strftime (sz,sizeof(sz),"%Y-%m-%d %H:%M:%S",ptm);
    //
    switch (dump_format)
    {
    case FORMAT_BLOCK:
        *output<<"--- EOS Packet ["<<c_counter<<"] at ["<<setw(9)<<right<<pf->stream_pos<<"]\n";
        ui8 = SP_GET_VER(ph) + 0;
        *output<<"      version:\t"<<dec<<setw(3)<<right<< ui8 <<" [0x"<<hex<<ui8<<"]"<<endl;
        ui8 = SP_GET_SECHEADER(ph);
        *output<<"    has sec.h:\t"<<dec<<setw(3)<<right<< ui8 <<endl;
        ui16 = SP_GET_APID(ph);
        *output<<"         APID:\t"<<dec<<setw(3)<<right<< ui16 <<" [0x"<<hex<<ui16<<"]"<<endl;
        ui16 = SP_GET_COUNT(ph);
        *output<<"      counter:\t"<<dec<<setw(8)<<right<< ui16 <<" [0x"<<hex<<ui16<<"]"<<endl;
        ui16 = SP_GET_LENGTH(ph);
        *output<<"       length:\t"<<dec<<setw(8)<<right<< ui16 <<" [0x"<<hex<<ui16<<"]"<<endl;
        //time
        *output<<"      obtime:\t"<<dec<<tday<<"d "<<tmsec <<"msec of day = "<< sz <<endl;
        break;
    case FORMAT_LINES_SHORT:
        *output<<"-- EOS Packet ["<<setw(6)<<right<<c_counter<<"] at ["<<setw(9)<<right<<pf->stream_pos<<"]";
         ui16 = SP_GET_APID(ph);
        *output<< "\tAPID:"<<dec<<setw(3)<<right<< ui16 ;
        ui16 = SP_GET_COUNT(ph);
        *output<<"\tcounter:"<<dec<<setw(6)<<right<< ui16 ;
        ui16 = SP_GET_LENGTH(ph);
        *output<<"\tlength:"<<dec<<setw(4)<<right<< ui16;
        *output<<"\tobt:"<< sz;
        *output <<endl;
        break;
    case FORMAT_LINES_LONG:
        *output<<"-- EOS Packet ["<<setw(6)<<right<<c_counter<<"] at ["<<setw(9)<<right<<pf->stream_pos<<"]";
         ui16 = SP_GET_APID(ph);
        *output<< "\tAPID:"<<dec<<setw(3)<<right<< ui16 ;
        ui16 = SP_GET_COUNT(ph);
        *output<<"\tcounter:"<<dec<<setw(6)<<right<< ui16 ;
        ui16 = SP_GET_LENGTH(ph);
        *output<<"\tlength:"<<dec<<setw(4)<<right<< ui16;
        *output<<"\tobt:"<< sz<<".";
        output->fill ('0');
        *output<<setw(6)<<right<<pf->obtime_usec;
        output->fill (' ');
        *output <<endl;
        break;
    }
    //restore default formats
    *output<<dec<<left;
}



