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

#define DEFAULT_PACKAGE_BYTES    1024*1024
// INI tags

#define INI_POST_TO   "post_to"
#define INI_BASE_APID  "apid"
//DEFAULT_PACKAGE_BYTES is used as a buffer size, package is sent as soon as the buffer is get full
#define INI_PACKAGE_BYTES  "package_bytes"
#define INI_HEADER_DATATYPE "data_type"
#define INI_HEADER_SATELLITE  "satellite"
#define INI_HEADER_STATION  "station"
#define INI_HEADER_CUSTOM "custom_header"
#define INI_OBT_EPOCH      "obt_epoch"

// ini substitute tags
#define INI_SUBS_NAME   "NAME"
#define INI_SUBS_ID     "ID"
#define INI_STREAM_URL  "STREAMURL"
#define INI_STREAM_ID   "STREAMID"
#define INI_STREAM_OBT  "STREAMOBT"

//
#define ONLINE_USER_AGENT   "FPF RSSPipe,ver.1.0"

CNode_RSSPipe::CNode_RSSPipe()
{
    is_initialized = false;
    pnext_node = NULL;
    c_package_packets = 0;
    base_apid = 0;//MODIS
    c_missing_packets = 0;
    prev_pcount = 0;
    c_slice_missing = 0;
    c_slice_packets = 0;
    slice_base = -1;
    slice_start_pos = 0;
    slice_start_time="";
    slice_start_time_long="";
    package_buff_bytes = DEFAULT_PACKAGE_BYTES;
    c_counter = 0;
    obt_epoch = EOS_TIME_EPOCH;
    //
    pBuff_base = NULL;
    buff_fill = 0L;
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

    buffer_base = NULL;
    // -------------- do custom initialization here -------------------
    c_missing_packets = 0;
    c_counter = 0;
    prev_pcount = 0;
    c_slice_missing = 0;
    c_slice_packets = 0;
    slice_base = -1;
    slice_start_pos = 0;
    slice_start_time="";
    slice_start_time_long="";

    //
    header_datatype = "X-Datatype: "+conf[INI_HEADER_DATATYPE];
    header_satellite = "X-Satellite: "+conf[INI_HEADER_SATELLITE];
    header_station = "X-STation: "+conf[INI_HEADER_STATION];
    header_custom = "X-CustomHeader: "+conf[INI_HEADER_CUSTOM];
    header_status = "X_Status: first";
    //
    package_buff_bytes = DEFAULT_PACKAGE_BYTES;
    if (! conf[INI_PACKAGE_BYTES].empty() )
    {
         string s = conf[INI_PACKAGE_BYTES];
        package_buff_bytes = strtoll(s.c_str(),NULL,10);
        if (s.find_first_of("kK") != string::npos) { package_buff_bytes *= 1024; }
        else { if (s.find_first_of('M') != string::npos) { package_buff_bytes *= 1024*1024; } }
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
    // allocate buffer
    buffer_base = (BYTE*) malloc(package_buff_bytes);
    if (buffer_base == NULL)
    {
        *fpf_error<<"=>ERROR " MY_CLASS_NAME "("<<name<<")::init failes to allocate buffer of size "<<package_buff_bytes<<" bytes\n";
        is_initialized = false;
        return false;
    }
    buff_fill = 0;
    c_chunks = 1; //one-based
    has_post_failure= false;
    //
    *fpf_trace<<"=> " MY_CLASS_NAME "("<<name<<")::init initialized with post to "<<post_to_url<<"\n";
    is_initialized = true;
    return true;
}

void CNode_RSSPipe::constructURL(CFrame* pf) //resolve file name, taking data from the frame and its stream, call it after the streeam supplies the valid packets
{
    if (string::npos == post_to_url.find('%') ) { return; }//nothing to resolve
    map<string,string> subs_map;
    subs_map[INI_SUBS_NAME] = name;
    subs_map[INI_STREAM_ID] = pf->pstream->id;
    subs_map[INI_SUBS_ID] = id;
    char sz[40]; tm* ptm = gmtime(&(pf->pstream->obt_base));
    strftime (sz,sizeof(sz),"%Y%m%d%H%M%S",ptm);
    subs_map[INI_STREAM_OBT] = string(sz);
    //
    make_substitutions(post_to_url, &subs_map,'%');
}

void CNode_RSSPipe::start(void)
{
}

void CNode_RSSPipe::stop(void)
{
}

void CNode_RSSPipe::close(void)
{

#ifdef USE_CURL
    //
    if (! post_to_url.empty())
    {
        // -- post last chunk
        header_status = "X_Status: last";
        if (!post_buffer())
        { // stop working if post fails for any reason
            has_post_failure = true;
        }
        c_chunks++;

    }
    //
    if (curl) { curl_easy_cleanup(curl); curl = NULL;}
#endif
    //
     //
    if (pnext_node != NULL)   {  pnext_node->close();   delete pnext_node;   pnext_node = NULL;   }
    if (buffer_base != NULL)
    {
        free(buffer_base);
        buffer_base = NULL;
    }
    is_initialized = false;

    *fpf_trace<<"<= " MY_CLASS_NAME "("<<name<<") closed, posted "<<(c_chunks-1)<<" slices by "<<package_buff_bytes<<" packets\n";
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

void CNode_RSSPipe::do_frame_processing(CFrame* pf)
{
    c_counter++;
    //
    if (has_post_failure) { return; } //do nothing if post failed
    //
    BYTE *ph = pf->pdata;
    // --
    // save packet to the buffer
    if (pf->frame_size + buff_fill > package_buff_bytes)
    { // the package buffer is full and should be posted
        if (c_chunks == 1)
        { //irst chunk actions
        // - resolve URL for substitution fields
            constructURL(pf);
        }else
        {
            header_status = "X_Status: flow";
        }
        #
        if (!post_buffer())
        { // stop working if post fails for any reason
            has_post_failure = true;
        }
        c_chunks++;
        //and reset package counters and attributes
        buff_fill = 0;
        c_package_packets = 0;
    }
    //append frame to the buffer
    memcpy(buffer_base+buff_fill,pf->pdata,pf->frame_size);
    buff_fill += pf->frame_size;
    c_package_packets++;
    return;


}

class read_buff //helper structure to pass to CURL read callback, keeps track of reading from the buffer
{
    public:
        BYTE* buff;
        size_t len;
        size_t pos;
        string response;
        read_buff( BYTE *pbuff, size_t bsize)
        {
            buff = pbuff;
            len = bsize;
            pos = 0;
        };
        size_t read(char* ptr,size_t nsize)
        {
            int to_copy = (int) nsize;
            if (to_copy > int(len)-int(pos)) { to_copy = len-pos;}
            if (to_copy<1) { return 0; }
            memcpy(ptr,buff + pos, to_copy);
            pos += to_copy;
            return (size_t)to_copy;
        };
        void save_responce(char* ptr,size_t nsize)
        {
            response += string(ptr,nsize);
        }
} ;

static size_t read_callback(void *ptr, size_t nsize, size_t nmemb, void *data)
{ // CURL read callback

  return ((read_buff*)data)->read((char*)ptr,nsize*nmemb);
}

static size_t write_callback(char *in, size_t xsize, size_t nmemb, void *data)
{
  size_t rsize;
  rsize = xsize * nmemb;
  ((read_buff*)data)->save_responce((char*)in,rsize);
  return rsize;
}

bool CNode_RSSPipe::post_buffer(void)
{
    bool print_responce = false;
#ifdef USE_CURL
    //
    if (curl == NULL)
    {
         has_post_failure = false;
         curl = curl_easy_init();
          if (curl == NULL)
          {
                *fpf_error << "ERROR: " MY_CLASS_NAME "("<<name<<") CURL initialization error\n";
                has_post_failure = true;
                return false;
          }
    }
    //setup connection
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, ONLINE_USER_AGENT);
    curl_easy_setopt(curl, CURLOPT_URL, post_to_url.c_str());
    //add headers
    struct curl_slist *chunk = NULL;
    char schunk_no[32]  ;
    sprintf(schunk_no,"X-ChunkNumber: %d",c_chunks);
    char schunk_size[32]  ;
    sprintf(schunk_size,"X-ChunkSize: %ld",buff_fill);
    char schunk_frames[32]  ;
    sprintf(schunk_frames,"X-ChunkFrames: %d",c_package_packets);
    chunk = curl_slist_append(chunk, header_custom.c_str());
    chunk = curl_slist_append(chunk, header_datatype.c_str());
    chunk = curl_slist_append(chunk, header_satellite.c_str());
    chunk = curl_slist_append(chunk, header_station.c_str());
    chunk = curl_slist_append(chunk, header_status.c_str());
    chunk = curl_slist_append(chunk, schunk_no);
    chunk = curl_slist_append(chunk, schunk_size);
    chunk = curl_slist_append(chunk, schunk_frames);
    CURLcode res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
    //setup read callback
    read_buff h_read_buff  = read_buff(buffer_base,buff_fill);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
    curl_easy_setopt(curl, CURLOPT_READDATA, &h_read_buff);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE, buff_fill);//curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, str_report.length());
    //accept response
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &h_read_buff);
    char errbuf[CURL_ERROR_SIZE];
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    //make the call
    res = curl_easy_perform(curl);
    // Check for errors
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    if(res != CURLE_OK) {
      *fpf_error << "= ERROR = " MY_CLASS_NAME "("<<name<<") online post error ("<<response_code<<"):"<<curl_easy_strerror(res)<<"\n";
      return false;
    }
    else {
      if (response_code == 200)
      {
        if(print_responce) //print positive responce only if requested
        {
          *fpf_info << "= " MY_CLASS_NAME "("<<name<<") posted OK: ------\n" <<h_read_buff.response <<"\n----------\n"; ;
        }
      }else
      {
          *fpf_error << "= ERROR = " MY_CLASS_NAME "("<<name<<") package post failed with code "<<response_code<<"\n----\n"<<h_read_buff.response <<"\n----------\n";
          return false;
      }
    }
    return true;
#endif
}





