/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
InputStream_File. - Input Stream reading bitstream from a file

History:
    created: 2015-11 - A.Shumilin.
*/
#include <stdio.h>
#include <cerrno>
#include <unistd.h>
#include <cstdlib> /* strtol */
#include <cstring> /* strerror */
#include "CInputStream_File.h"

#define MY_CLASS_NAME   "CInputStream_File"

#define INI_START_OFFSET    "start_offset"
#define INI_RT  "real_time"
#define INI_BYTES_TO_READ   "read_bytes"
#define INI_RT_TIMEOUT   "rt_timeout"

#define RT_SLEEP_AFTER_PARTIAL  100000
#define RT_SLEEP_AFTER_ZERO  1000000
#define RT_SLEEP_RETRIES_AFTER_ZERO  5

CInputStream_File::CInputStream_File()
{
    file = NULL;
    url = "";
    stream_pos = 0;
    start_offset = 0;
    is_rt = false;
    bytes_to_read = 0;
    read_total = 0;
    rt_timeout = 11;

}

CInputStream_File::~CInputStream_File()
{
    if (file != NULL) { fclose(file); file = NULL;}
}

bool CInputStream_File::init(t_ini& ini, string& init_name)
{
     *fpf_debug << "~ " MY_CLASS_NAME "::init("<<init_name<<")\n";
    name = init_name;
    fpf_last_error.clear();
    is_initialized = false;
    start_offset = 0;
    // figure out a file name
    if (ini.find(init_name) == ini.end())
    {
        *fpf_error<<"ERROR: no ini section for InputStream ["<<init_name<<"]\n";
        return false;
    }
    t_ini_section conf = ini[init_name];
    //
    if (conf.find(INI_START_OFFSET) != conf.end())
    {
        string s = conf[INI_START_OFFSET];
        start_offset = strtoll(s.c_str(),NULL,10);
        if (s.find_first_of("kK") != string::npos) { start_offset *= 1024; }
        else { if (s.find_first_of('M') != string::npos) { start_offset *= 1024*1024; } }
    }
    if (conf.find(INI_BYTES_TO_READ) != conf.end())
    {
        string s = conf[INI_BYTES_TO_READ];
        bytes_to_read = strtoll(s.c_str(),NULL,10);
        if (s.find_first_of("kK") != string::npos) { bytes_to_read *= 1024; }
        else { if (s.find_first_of('M') != string::npos) { bytes_to_read *= 1024*1024; } }
    }
    //
    is_rt = false;
    if (conf[INI_RT] == INI_COMMON_VAL_YES) {is_rt = true;}
    if (conf.find(INI_RT_TIMEOUT) != conf.end())
    {
        string s = conf[INI_RT_TIMEOUT];
        rt_timeout = strtoll(s.c_str(),NULL,10);
    }
    // -- choose an input file name
    if (conf.find(INI_COMMON_INPUT_FILE) != conf.end())
    {
        file_name = conf[INI_COMMON_INPUT_FILE];
    }else
    {
        if (ini[INI_COMMON_SECTION_ARGV].find(INI_ARGV_INPUT_FILE) != conf.end())
        {
            file_name = ini[INI_COMMON_SECTION_ARGV][INI_ARGV_INPUT_FILE];
        }else
        {  //use stdin
            file_name = "-";
        }
    }
    //-- open input file
    if (file_name == "-") { file = stdin; }
    else
    {
        file = fopen(file_name.c_str(), "rb");
        if (file == NULL)
        {
            *fpf_error<<"ERROR: CInputStream_File::init("<<name<<")  input file open failed\n   File:"<<file_name<<"\n   Error ["<<errno<<"]:"<<strerror(errno)<<endl;
            return false;
        }
        // adjust initial position
        if (0 == fseek ( file , start_offset , SEEK_SET ))
        {
            *fpf_trace<<"= CInputStream_File::init("<<name<<") opened file "<<file_name<<":"<<start_offset<<"+"<<bytes_to_read<<endl;
        }
    }
    if (ferror(file))
    {
        *fpf_error<<"ERROR: CInputStream_File::init("<<name<<") ferror on File:"<<file_name<<"\n   Error ["<<errno<<"]:"<<strerror(errno)<<endl;
        return false;
    }
    //
    id = file_name; //strip off the path and extension
    size_t s_pos = id.find_last_of("\\/"); if (string::npos != s_pos) {  id.erase(0, s_pos + 1); }
    s_pos = id.find_last_of("."); if (string::npos != s_pos) {  id.erase(s_pos); }
    url = file_name;
    read_total = 0;
    stream_pos = start_offset;
    //
    string str_rt = (is_rt)? "[RT] " : "";
     *fpf_trace<<"=> " MY_CLASS_NAME "::init("<<name<<") on "<<str_rt<<file_name<<"\n";
    is_initialized = true;
    return true;
}

void CInputStream_File::start(void)
{
}

void CInputStream_File::stop(void)
{
}

void CInputStream_File::close(void)
{
    if (file != NULL) { fclose(file); file = NULL;}
    is_initialized = false;
    *fpf_trace<<"<= " MY_CLASS_NAME "("<<name<<") closed, "<<read_total<<" bytes read\n";
}


unsigned int CInputStream_File::read(BYTE* pbuff, size_t read_bytes, int& ierror)
{
    FPF_ASSERT((file != NULL),"CInputStream_File::read, file not opened");
    FPF_ASSERT((pbuff != NULL),"CInputStream_File::read to NULL buffer");
    //
    if ((bytes_to_read > 0) && (read_total > bytes_to_read))
    {
         *fpf_warn<<"\n!! " MY_CLASS_NAME "("<<name<<")::read reached limit of bytes to read ("<<bytes_to_read<<")\n\n";
        return 0;
    }
    //
    size_t readed = fread (pbuff,1,read_bytes,file);
    read_total += readed;
    stream_pos += readed;
    if (readed == read_bytes)
    {// everything ok
        ierror = 0;
        return readed;
    }
    if (is_rt )
    { // wait a bit and retry
       if(readed > 0 ) //partial read, just implement small delay before next read
       {
            usleep(RT_SLEEP_AFTER_PARTIAL); cout<<"."<<endl;
       }else
       {// its a zero read, wait if something will be appended
           for (int attempt =0; attempt < rt_timeout; attempt++)
           {
               usleep(RT_SLEEP_AFTER_ZERO);  cout<<"*"<<endl;
               readed = fread (pbuff,1,bytes_to_read,file);
               if (readed > 0)
               {
                   read_total += readed;
                   ierror = 0;
                   return readed;
               }
               //else, try again
           }
           *fpf_trace<<"= " MY_CLASS_NAME "("<<name<<") zero reads in RT mode timeout reached\n";
           return 0;
       }

    }
    // otherwise
    ierror = 0;
    return readed;
}

