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

#include <iostream>
#include <sstream>
#include <cstring> /* memcpy */
#include <cstdlib> /* atoi */
#include <iomanip>

#ifdef USE_CURL
#include <curl/curl.h>
#else
#warning "WARNING: CNode_EOSinv does not use online features without CURL library"
#endif // USE_CURL

#include "ini.h"
#include "CNode_EOSinv.h"
#include "class_factory.h"
#include "ccsds.h"


#define MY_CLASS_NAME   "CNode_EOSinv"

// INI tags

#define INI_SAVE_TO   "save_to"
#define INI_POST_TO   "post_to"
#define INI_POST_TO_NRT   "post_to_nrt"
#define INI_LAZY_CREATE  "lazy_create"
#define INI_INVREPORT_HEADER  "report_header"
#define INI_SLICEID_PREFIX "slice_prefix"
#define INI_BASE_APID  "apid"
#define INI_SLICE_SIZE  "slice_size"
#define INI_INVREPORT_DATATYPE "data_type"
#define INI_INVREPORT_URL       "data_url"
#define INI_INVREPORT_SATELLITE  "satellite"
#define INI_OBT_EPOCH      "obt_epoch"

// ini substitute tags
#define INI_SUBS_NAME   "NAME"
#define INI_SUBS_ID     "ID"
#define INI_STREAM_URL  "STREAMURL"
#define INI_STREAM_ID   "STREAMID"
#define INI_STREAM_OBT  "STREAMOBT"

//
#define ONLINE_USER_AGENT   "FPF EOSinv,ver.1.0"



CNode_EOSinv::CNode_EOSinv()
{
    is_initialized = false;
    pnext_node = NULL;
    output = NULL;
    lazy_create = false;
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
}

CNode_EOSinv::~CNode_EOSinv()
{
    if (curl)
    {
#ifdef USE_CURL
		curl_easy_cleanup(curl);
#endif
		curl = NULL;
	}
}

bool CNode_EOSinv::init(t_ini& ini, string& init_name, CChain* pchain_arg)
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
    inv_report_header = conf[INI_INVREPORT_HEADER];
    slice_id_prefix = conf[INI_SLICEID_PREFIX];
    inv_report_url = conf[INI_INVREPORT_URL];
    inv_report_datatype = conf[INI_INVREPORT_DATATYPE];
    inv_report_satellite = conf[INI_INVREPORT_SATELLITE];
    base_apid = atoi(conf[INI_BASE_APID].c_str());
    if (base_apid == 0)
    {
        *fpf_error<<"ERROR: " MY_CLASS_NAME "("<<name<<")::init 'apid' parameter must be defined\n";
        return false;
    }
    //
    packets_in_slice = 4096;
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
    lazy_create = false;
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
    }
    if (!conf[INI_POST_TO].empty())
    {
        post_to_url = conf[INI_POST_TO];
    }
    if (!conf[INI_POST_TO_NRT].empty())
    {
        post_to_url_nrt = conf[INI_POST_TO_NRT];
    }
    //build next node
    bool no_next_node;  pnext_node = get_next_node(ini,name,&no_next_node);
	if (pnext_node == NULL)
	{if (!no_next_node) {*fpf_error << "ERROR:  " MY_CLASS_NAME "::init("<<init_name<<") failed to create next node ["<< conf[INI_COMMON_NEXT_NODE] <<"]\n";	return false;	}}
    else  {  if (! pnext_node->init(ini,conf[INI_COMMON_NEXT_NODE],pchain_arg))  { return false;}   }
    //
    string str_lazy = (lazy_create)? "(lazy) " : "";
    *fpf_trace<<"=> " MY_CLASS_NAME "("<<name<<")::init initialized with output to "<<str_lazy<<outfile_name<<"\n";
    is_initialized = true;
    return true;
}

void CNode_EOSinv::start(void)
{
}

void CNode_EOSinv::stop(void)
{
}

void CNode_EOSinv::close(void)
{
    //
    if (outfile.is_open()) { outfile.close(); output = NULL;}
    //
    if (pnext_node != NULL)   {  pnext_node->close();   delete pnext_node;   pnext_node = NULL;   }
    is_initialized = false;
#ifdef USE_CURL
    if (! post_to_url.empty())
    {
        *fpf_trace <<"<= " MY_CLASS_NAME "("<<name<<") online post of inventory report\n";
        online_post(full_report,post_to_url);
    }
    if (curl) { curl_easy_cleanup(curl); curl = NULL;}
#endif
    *fpf_trace<<"<= " MY_CLASS_NAME "("<<name<<") closed, inventoried "<<c_slices<<" slices by "<<packets_in_slice<<" packets\n";
}

void CNode_EOSinv::take_frame(CFrame* pf)
{
    FPF_ASSERT( (pf!= NULL),"NULL frame pointer")
    FPF_ASSERT(is_initialized,"Node not initialized")
    // custom node actions
    do_frame_processing(pf);
    //pass the frame further by chain
    if (pnext_node != NULL) { pnext_node->take_frame(pf); }
}

void CNode_EOSinv::build_file_name(CFrame* pf) //resolve file name, taking data from the frame and its stream
{
    if (string::npos == outfile_name.find('%') ) { return; }//nothing to resolve
    map<string,string> subs_map;
    subs_map[INI_SUBS_NAME] = name;
    subs_map[INI_STREAM_URL] = pf->pstream->url;
    subs_map[INI_STREAM_ID] = pf->pstream->id;
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

void CNode_EOSinv::do_frame_processing(CFrame* pf)
{
    c_counter++;
    //
    BYTE *ph = pf->pdata;
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
    //
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
            *output << ss_slice.str();
            //
            if (! post_to_url_nrt.empty())
            {
                if(!online_post(report_header + ss_slice.str(),post_to_url_nrt))
                {
                    post_to_url_nrt = ""; //no more attempts to post nrt reports
                }
            }
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
        string response;
        hstring_t( string *arg_pstr)
        {
            pstr = arg_pstr;
            len = pstr->length();
            pos = 0;
            response = "";
        };
        size_t read(char* ptr,size_t nsize)
        {
            int to_copy = (int) nsize;
            if (to_copy > int(len)-int(pos)) { to_copy = len-pos;}
            if (to_copy<1) { return 0; }
            memcpy(ptr,pstr->c_str() + pos, to_copy);
            pos += to_copy;
            return (size_t)to_copy;
        };
        void save_responce(char* ptr,size_t nsize) //keep response always \0 terminated
        {
            response += string(ptr,nsize);
        }
} ;

static size_t read_callback(void *ptr, size_t nsize, size_t nmemb, void *data)
{ // CURL read callback
  return ((hstring_t*)data)->read((char*)ptr,nsize*nmemb);
}

static size_t write_callback(char *in, size_t xsize, size_t nmemb, void *data)
{
  size_t rsize;
  rsize = xsize * nmemb;
  ((hstring_t*)data)->save_responce((char*)in,rsize);
  return rsize;
}

bool CNode_EOSinv::online_post(string str_report,string url)
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
    //accept response
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &hstring);
    char errbuf[CURL_ERROR_SIZE];
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    //make the call
    CURLcode res = curl_easy_perform(curl);
    // Check for errors
    if(res != CURLE_OK) {
      *fpf_error << "ERROR: " MY_CLASS_NAME "("<<name<<") online post error:"<<curl_easy_strerror(res)<<"\n";
      return false;
    }
    else {

      long response_code;
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
      if (response_code == 200)
      {
          *fpf_trace << "-inv.report upload, OK, 200\n";
      }else
      {
          *fpf_error << "-inv.report upload failed with code "<<response_code<<"\n"<<hstring.response <<"\n----------\n";
          return false;
      }
      return true;
    }

#endif
}





