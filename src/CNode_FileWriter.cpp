/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
CNode_FileWriter - node  writing frame sequence to output file

History:
    created: 2015-11 - A.Shumilin.
*/
#include <stdio.h>
#include <cerrno>
#include <cstdlib> /* strtol */
#include <cstring> /* strerror */
#include "ini.h"
#include "CNode_FileWriter.h"
#include "class_factory.h"


#define MY_CLASS_NAME   "CNode_FileWriter"

// INI tags
#define INI_LAZY_CREATE     "lazy_create"
// split output into fixed size chunks (this is number of frames, not bytes!)
#define INI_SPLIT_CHUNKS     "split_chunks"
#define INI_FILE_NAME       "file_name"

#define  INI_PLACEHOLDER_OBT   "%OBT%"
#define INI_STREAM_URL  "STREAMURL"
#define INI_STREAM_ID   "STREAMID"
#define INI_STREAM_OBT  "STREAMOBT"

CNode_FileWriter::CNode_FileWriter()
{
    is_initialized = false;
    pnext_node = NULL;
    lazy_create = false;
    file = NULL;
    total_written = 0;
    frames_in_chunk = 0;
    chunk_size = 0;
    c_chunk = 0;
}

CNode_FileWriter::~CNode_FileWriter()
{
    if ((file != NULL) && (file != stdout) ) { fclose(file); }
}

bool CNode_FileWriter::open_file()
{
    string file_name_00 = file_name;
    if (file_name == "-") { file = stdout; return true; }
    else
    {
        if (chunk_size>0) // appent chunk number
        {
            file_name_00 = file_name;
            char s00[8];
            sprintf(s00,"_n%02d",c_chunk);
            file_name_00.insert(file_name_00.find_last_of("."),string(s00));
        }
        //
        file = fopen(file_name_00.c_str(), "wb");
        if (file == NULL)
        {
            *fpf_error<<"ERROR: CNode_FileWriter::open_file("<<name<<") output file open failed\n   File:"<<file_name_00<<"\n   Error ["<<errno<<"]:"<<strerror(errno)<<endl;
            return false;
        }
    }
    if (ferror(file))
    {
        *fpf_error<<"ERROR: CNode_FileWriter::open_file("<<name<<") ferror on File:"<<file_name_00<<"\n   Error ["<<errno<<"]:"<<strerror(errno)<<endl;
        return false;
    }
    *fpf_info<<"=> CNode_FileWriter("<<name<<") opened output to ["<<file_name_00<<"]\n";
    id = file_name;
    return true;
}

bool CNode_FileWriter::init(t_ini& ini, string& init_name, CChain* pchain_arg)
{
    *fpf_debug << "~ " MY_CLASS_NAME "::init("<<init_name<<")\n";
    pchain = pchain_arg;
    is_initialized = false;
    //
    t_ini_section conf = ini[init_name];
    name = init_name;
    id = name;
    // --------------
    total_written = 0;
    frames_in_chunk = 0;
    c_chunk =0;
    //
    chunk_size = 0; //defult - no chunk split
     if (conf.find(INI_SPLIT_CHUNKS) != conf.end())
    {
        chunk_size  = strtol(conf[INI_SPLIT_CHUNKS].c_str(),NULL,10); //interpret as decimal
    }
    //-- prepare the file
    if ( conf[INI_LAZY_CREATE] == INI_COMMON_VAL_YES) {lazy_create = true;}
    if (conf.find(INI_FILE_NAME) != conf.end())
    {
        file_name = conf[INI_FILE_NAME];
    }
    if (! lazy_create) { if (! open_file() ) { return false;} }

    //build next node
    bool no_next_node;  pnext_node = get_next_node(ini,name,&no_next_node);
	if (pnext_node == NULL)
	{if (!no_next_node) {*fpf_error << "ERROR:  " MY_CLASS_NAME "::init("<<init_name<<") failed to create next node ["<< conf[INI_COMMON_NEXT_NODE] <<"]\n";	return false;	}}
    else  {  if (! pnext_node->init(ini,conf[INI_COMMON_NEXT_NODE],pchain_arg))  { return false;}   }
    //
    *fpf_trace<<"=> " MY_CLASS_NAME "::init("<<name<<") initialized\n";
    is_initialized = true;
    return true;
}

void CNode_FileWriter::start(void)
{
}

void CNode_FileWriter::stop(void)
{
}

void CNode_FileWriter::close(void)
{
    if (pnext_node != NULL)   {  pnext_node->close();   delete pnext_node;   pnext_node = NULL;   }
    is_initialized = false;
    if ((file != NULL) && (file != stdout) ) { fclose(file); file = NULL;}
    size_t kb  = total_written / 1024;
    *fpf_trace<<"<= " MY_CLASS_NAME "("<<name<<") closed, "<<kb<< "kiB written to "<<file_name<<"\n";
}

void CNode_FileWriter::take_frame(CFrame* pf)
{
    FPF_ASSERT( (pf!= NULL),"NULL frame pointer")
    FPF_ASSERT(is_initialized,"Node not initialized")
    // custom node actions
    do_frame_processing(pf);
    //pass the frame further by chain
    if (pnext_node != NULL) { pnext_node->take_frame(pf); }
}



void CNode_FileWriter::do_frame_processing(CFrame* pf)
{
    c_counter++;
    if (file == NULL)
    {
        if (lazy_create)
        {
            //fix file name
            map<string,string> map_subs;
            tm* timeinfo = gmtime(&(pf->pstream->obt_base));
            char stm[80]; strftime(stm, sizeof(stm), "%Y%m%d%H%M%S", timeinfo);
            map_subs[INI_STREAM_OBT] = string(stm);
            map_subs[INI_STREAM_ID] = pf->pstream->id;
            map_subs[INI_STREAM_URL] = pf->pstream->url;
            make_substitutions(file_name, &map_subs,'%');
            //
            if (!open_file())
            {
                *fpf_error<<"ERROR: CNode_FileWriter::take_frame("<<name<<") failed to lazy create file. Output blocked\n";
                lazy_create = false; //no more attempts
                file = NULL;
                return;
            }

        }else { return; } //do nothing
    }
    //
    size_t to_write = pf->frame_size;
    size_t written = fwrite(pf->pdata,1,to_write,file);
    total_written += written;
    if (written != to_write)
    {
        *fpf_error<<"ERROR: CNode_FileWriter("<<name<<") file write failed,Error ["<<errno<<"]:"<<strerror(errno)<<endl;
        fclose(file);
        file = NULL;
        lazy_create = false;
    }
    //-splitting into chunks
    frames_in_chunk ++;
    if ((chunk_size>0) && (frames_in_chunk > chunk_size)) // reopen next chunk file
    {
        if ((file != NULL) && (file != stdout) ) { fclose(file); file = NULL;}
        c_chunk++;
        open_file();
        frames_in_chunk = 0;

    }

}
