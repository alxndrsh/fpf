/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
CNode_RSSPipe - Upload chunks of frame stream to HTTP server (like RSS-Pipe.com)

History:
    created: 2017-01 - A.Shumilin.
*/

#include <iostream>
#include <sstream>
#include <cstring> /* memcpy */
#include <cstdlib> /* atoi */
#include <iomanip>

#ifdef USE_CURL
// This module signifcantly relies on CURL library
#include <curl/curl.h>
#else
#warning "WARNING: CNode_RSSPipe does not use online features without CURL library"
#endif // USE_CURL

#include "ini.h"
#include "CNode_RSSPipe.h"
#include "class_factory.h"
#include "ccsds.h"


#define MY_CLASS_NAME   "CNode_RSSPipe"

#define DEFAULT_PACKETS_IN_SLICE    4096
// INI tags

#define INI_POST_TO   "post_to"
#define INI_BASE_APID  "apid"
#define INI_SLICE_SIZE  "slice_size"
#define INI_META_DATATYPE "data_type"
#define INI_META_SATELLITE  "satellite"
#define INI_OBT_EPOCH      "obt_epoch"

// ini substitute tags
#define INI_SUBS_NAME   "NAME"
#define INI_SUBS_ID     "ID"

//
#define ONLINE_USER_AGENT   "FPF RSSPipe,ver.1.0"

CNode_RSSPipe::CNode_RSSPipe()
{
    is_initialized = false;
    pnext_node = NULL;
    base_apid = 0;//MODIS
    c_missing_packets = 0;
    prev_pcount = 0;
    c_slice_missing = 0;
    c_slice_packets = 0;
    slice_base = -1;
    slice_start_pos = 0;
    c_slices=0;
    slice_start_time="";
    slice_start_time_long="";
    packets_in_slice = 4096;
    c_counter = 0;
    obt_epoch = EOS_TIME_EPOCH;
    //
    pBuff_base = NULL;
    pBuff_end = NULL;
}

CNode_RSSPipe::~CNode_RSSPipe()
{
    if (curl)
    {
#ifdef USE_CURL
		curl_easy_cleanup(curl);
#endif
		curl = NULL;
	}
	if (pBuff_base)
    {
         free (pBuff_base);
         pBuff_base = NULL;
    }
}

bool CNode_RSSPipe::init(t_ini& ini, string& init_name, CChain* pchain_arg)
{
     *fpf_debug << "~ " MY_CLASS_NAME "::init("<<init_name<<")\n";
    pchain = pchain_arg;
    is_initialized = false;
    //
    t_ini_section conf = ini[init_name];
    name = init_name;
    id = name;
    // -------------- do custom initialization here -------------------
    c_missing_packets = 0;
    c_counter = 0;
    prev_pcount = 0;
    c_slice_missing = 0;
    c_slice_packets = 0;
    slice_base = -1;
    slice_start_pos = 0;
    c_slices=0;
    slice_start_time="";
    slice_start_time_long="";

    //
    inv_report_datatype = conf[INI_META_DATATYPE];
    inv_report_satellite = conf[INI_META_SATELLITE];
    base_apid = atoi(conf[INI_BASE_APID].c_str());
    if (base_apid == 0)
    {
        *fpf_error<<"ERROR: " MY_CLASS_NAME "("<<name<<")::init 'apid' parameter must be defined\n";
        return false;
    }
    //
    packets_in_slice = DEFAULT_PACKETS_IN_SLICE;
    if (! conf[INI_SLICE_SIZE].empty() )
    {
         packets_in_slice = atoi(conf[INI_SLICE_SIZE].c_str());
    }
    //
    obt_epoch = EOS_TIME_EPOCH;
    if (! conf[INI_OBT_EPOCH].empty())
    {
		obt_epoch =  strtol(conf[INI_OBT_EPOCH].c_str(),NULL,10);
	}
    //
    if (!conf[INI_POST_TO].empty())
    {
        post_to_url = conf[INI_POST_TO];
    }else
    {
        *fpf_error<<"ERROR: " MY_CLASS_NAME "("<<name<<")::init: " INI_POST_TO " parameter must be defined\n";
        return false;
    }
    //build next node
    bool no_next_node;  pnext_node = get_next_node(ini,name,&no_next_node);
	if (pnext_node == NULL)
	{if (!no_next_node) {*fpf_error << "ERROR:  " MY_CLASS_NAME "::init("<<init_name<<") failed to create next node ["<< conf[INI_COMMON_NEXT_NODE] <<"]\n";	return false;	}}
    else  {  if (! pnext_node->init(ini,conf[INI_COMMON_NEXT_NODE],pchain_arg))  { return false;}   }
    //
    *fpf_trace<<"=> " MY_CLASS_NAME "("<<name<<")::init initialized with post to "<<post_to_url<<"\n";
    is_initialized = true;
    return true;
}

void CNode_RSSPipe::start(void)
{
}

void CNode_RSSPipe::stop(void)
{
}

void CNode_RSSPipe::close(void)
{
    //
    if (pnext_node != NULL)   {  pnext_node->close();   delete pnext_node;   pnext_node = NULL;   }
    is_initialized = false;
#ifdef USE_CURL
    if (! post_to_url.empty())
    {
        // TODO - post a finish message
    }
    if (curl) { curl_easy_cleanup(curl); curl = NULL;}
#endif
    *fpf_trace<<"<= " MY_CLASS_NAME "("<<name<<") closed, posted "<<c_slices<<" slices by "<<packets_in_slice<<" packets\n";
}

void CNode_RSSPipe::take_frame(CFrame* pf)
{
    FPF_ASSERT( (pf!= NULL),"NULL frame pointer")
    FPF_ASSERT(is_initialized,"Node not initialized")
    // custom node actions
    do_frame_processing(pf);
    //pass the frame further by chain
    if (pnext_node != NULL) { pnext_node->take_frame(pf); }
}


static int mod_diff(int x1,int x2, int base) // (x2-x1)%base (0 based numbers)
{
    int d = x2-x1;
    if (d<0)  { d=base+d; }
    return d % base;
}

void CNode_RSSPipe::do_frame_processing(CFrame* pf)
{
    c_counter++;
    //
    BYTE *ph = pf->pdata;
    // -- prepare values

    /*
    if (c_counter == 1) //init output header
    {
        std::ostringstream ss_header;
        ss_header << "#"<<inv_report_header <<"\n";
        ss_header << "#data type: "<<inv_report_datatype <<"\n";
        ss_header << "#satellite: "<<inv_report_satellite <<"\n";
        ss_header << "#data file: "<<pf->pstream->url<<"\n";
        ss_header << "#data url: "<<inv_report_url<<"\n";
        ss_header << "#ref.APID: "<<base_apid<< "\n";
        ss_header << "#packets per slice: "<<packets_in_slice<<"\n";
        time_t wct;time ( &wct );
        struct tm* ptm;
        ptm = gmtime( &wct );
        char szx[40];  strftime (szx,sizeof(szx),"%Y-%m-%dT%H:%M:%SZ",ptm);
        ss_header << "#created at: "<<szx<<"\n";
        ss_header << "#fields: slice_id,ref_time,file_pos,slice_size,num_frames,num_errors\n";
        ss_header << "#\n";
        #
        report_header = ss_header.str();
        full_report = report_header;
        *output << report_header;
    }
    */
    int tday = SP_GET_EOS_TS_DAYS(ph);
    int tmsec = SP_GET_EOS_TS_MSEC(ph);
    time_t utime = (tday*FPF_TIME_SEC_PER_DAY + obt_epoch) + int(tmsec / 1000);
    char sz[100];
    tm* ptm = gmtime(&utime);
    strftime (sz,sizeof(sz),"%Y%m%d%H%M%S",ptm);
    string stime = sz;
    strftime (sz,sizeof(sz),"%Y-%m-%dT%H:%M:%S",ptm);
    string stime_long = sz;
    // -- check APID
    int apid = SP_GET_APID(ph);
    if (apid != base_apid) { return; }
    //
    int pcount = SP_GET_COUNT(ph);
    //check counter continuity
    if (c_counter ==1) {prev_pcount=pcount-1;}
    int diff_counter = mod_diff(prev_pcount,pcount,CCSDS_MAX_PACKET_COUNTER+1);
    if (diff_counter != 1)
    {
        c_slice_missing += diff_counter-1; //per slice
        c_missing_packets += diff_counter-1; //total per stream
    }
    prev_pcount = pcount;
    //identify the slice
    int base_pcount = packets_in_slice * int(pcount / packets_in_slice);
    if (base_pcount != slice_base)
    {//this means a new base

        if (c_slices>0)//issue a slice report, if
        {
            size_t slice_size = pf->stream_pos - slice_start_pos;//from beginning of this new pack to the first pack of the slice
            //TODO - count gaps at the end of slice
            //output slice
            sprintf(sz,"%s_%s_%05d",slice_id_prefix.c_str(),slice_start_time.c_str(),slice_base);
            string slice_id = sz;
            std::ostringstream ss_slice;
            ss_slice << slice_id<<"\t"<<slice_start_time_long<<"\t"<<slice_start_pos<<"\t"<<slice_size<<"\t"<<c_slice_packets<<"\t"<<c_slice_missing<<"\n";
            full_report += ss_slice.str();
            //*output << ss_slice.str();
            //

              //  if(!online_post(report_header + ss_slice.str(),post_to_url_nrt))
              //  {
              //      post_to_url_nrt = ""; //no more attempts to post nrt reports
              //  }

        }
        c_slices++;//count issued only
        //init for new slice collection
        slice_base = base_pcount;
        c_slice_missing = pcount - slice_base;
        c_slice_packets = 0;
        slice_start_pos = pf->stream_pos;
        sprintf(sz,"%s%03d",stime.c_str(), tmsec%1000);
        slice_start_time = sz;
        sprintf(sz,"%s.%03d",stime_long.c_str(), tmsec%1000);
        slice_start_time_long = sz;
    }
    //for packets of the current slice
    c_slice_packets++;
}

struct hstring_t //helper structure to pass to CURL read callback, keeps track of reading from the string
{
    public:
        string* pstr;
        size_t len;
        size_t pos;
        hstring_t( string *arg_pstr)
        {
            pstr = arg_pstr;
            len = pstr->length();
            pos = 0;
        };
        size_t read(char* ptr,size_t nsize)
        {
            int to_copy = (int) nsize;
            if (to_copy > int(len)-int(pos)) { to_copy = len-pos;}
            if (to_copy<1) { return 0; }
            memcpy(ptr,pstr->c_str() + pos, to_copy);
            pos += to_copy;
            return (size_t)to_copy;
        }
} ;

static size_t read_callback(void *ptr, size_t nsize, size_t nmemb, void *data)
{ // CURL read callback
  return ((hstring_t*)data)->read((char*)ptr,nsize*nmemb);
}

bool CNode_RSSPipe::online_post(string str_report,string url)
{
#ifdef USE_CURL
    //
    if (curl == NULL)
    {
         curl = curl_easy_init();
          if (curl == NULL)
          {
                *fpf_error << "ERROR: " MY_CLASS_NAME "("<<name<<") CURL initialization error\n";
                return false;
          }
    }
    //setup connection
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, ONLINE_USER_AGENT);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    //setup read callback
    hstring_t hstring(&str_report);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
    curl_easy_setopt(curl, CURLOPT_READDATA, &hstring);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE, hstring.len);//curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, str_report.length());
    //make the call
    CURLcode res = curl_easy_perform(curl);
    // Check for errors
    if(res != CURLE_OK) {
      *fpf_error << "ERROR: " MY_CLASS_NAME "("<<name<<") online post error:"<<curl_easy_strerror(res)<<"\n";
      return false;
    }
    else {
      *fpf_trace << "-inv.report upload, OK\n";
      return true;
    }

#endif
}





