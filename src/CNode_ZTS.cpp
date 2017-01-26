/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
CNode_ZTS -  node to append Zodiac timestamps to the TF

*/
#include <cstring> /* memcpy */
#include <cstdlib> /* atoi */
#include <cstdio>  /* sscanf */
#include <ctime>   /* tm */
#include <iomanip> /*setw */
#include <time.h>
#include "ini.h"
#include "CNode_ZTS.h"
#include "class_factory.h"


#define MY_CLASS_NAME   "CNode_ZTS"

// INI tags
#define INI_FRAME_PERIOD   "frame_period_nanosec"
#define INI_BASE_MKSEC      "base_mkseconds"
#define INI_BASE_TIME       "base_time"  /* ISO format time of the first frame */
#define INI_SET_TIMESTAMP      "set_timestamp"  /* yes/no, set calculated timestamp values */
#define INI_DUMP_EVERY  "dump_every"
#define INI_ZTS_OFFSET  "zts_offset" /* start of ZTS in the frame, frame will be extended if this position is out of input frame */



const int NEW_FRAME_DATA_SIZE = 2048 + 16; //? what is the maximum CCSDS size ?
const time_t  EPOCH_2000 = 946684800; //2000-01-01


CNode_ZTS::CNode_ZTS()
{
    is_initialized = false;
    pnext_node = NULL;
    c_counter = 0;
    pframe_data = NULL;
    pframe_data = new uint8_t[NEW_FRAME_DATA_SIZE];
    frame_period = 100;
    base_t = 0;
    base_utime= 0;
    base_mksec = 0;
    set_timestamp = false;
    dump_every = 0;
    base_year = 2000;
    zts_offset = 0;
    f_update_actime = true;
}

CNode_ZTS::~CNode_ZTS()
{
    if (pframe_data) { delete[] pframe_data; }
}

bool CNode_ZTS::init(t_ini& ini, string& init_name, CChain* pchain_arg)
{
     *fpf_debug << "~ " MY_CLASS_NAME "::init("<<init_name<<")\n";
    pchain = pchain_arg;
    is_initialized = false;
    base_t = 0;
    base_mksec = 0;
    set_timestamp = false;
    dump_every = 0;
    setenv("TZ", "", 1); tzset(); //this is required to protect against local time zone correction
    //
    t_ini_section conf = ini[init_name];
    name = init_name;
    id = name;
    // --------------
    if (conf.find(INI_DUMP_EVERY) != conf.end())
    { dump_every = atoi(conf[INI_DUMP_EVERY].c_str()); }
    if (!conf[INI_FRAME_PERIOD].empty())
    {
        frame_period = atoi(conf[INI_FRAME_PERIOD].c_str());
    }
    if (!conf[INI_ZTS_OFFSET].empty())
    {
        zts_offset = atoi(conf[INI_ZTS_OFFSET].c_str());
    }
    if (!conf[INI_BASE_MKSEC].empty())
    {
        base_mksec = atoi(conf[INI_BASE_MKSEC].c_str());
    }
    if (!conf[INI_SET_TIMESTAMP].empty())
    {
        if (conf[INI_SET_TIMESTAMP] == INI_COMMON_VAL_YES) {set_timestamp = true;}
    }
    //we need this to get some year suggestion
    time_t curr_time;
    struct tm tm_base;
    time ( &curr_time );
    tm *ptm = gmtime ( &curr_time );
    base_year = ptm->tm_year + 1900;
    //
    if (!conf[INI_BASE_TIME].empty())
    {
        str_base_time = conf[INI_BASE_TIME];
        int y,M,d,h,m;
        int s;
        int nfields = sscanf(str_base_time.c_str(), "%d-%d-%dT%d:%d:%d", &y, &M, &d, &h, &m, &s);
        if (nfields != 6)
        {
            *fpf_error<<"=> " MY_CLASS_NAME "::init("<<name<<") ERROR, invalid base time string ["<<str_base_time<<"]\n";
            is_initialized = false;
            return false;
        }else
        {
            base_year = y; // always use explicitly defined year
            tm_base.tm_year = y - 1900;
            tm_base.tm_mon = M - 1;
            tm_base.tm_mday = d; //NB: looks like the base day is not 0 or it does not take into account leap year
            tm_base.tm_hour = h;
            tm_base.tm_min = m;
            tm_base.tm_sec = s;
            tm_base.tm_gmtoff = 0;


            base_utime = mktime(&tm_base);
             base_t = base_utime;
            //we need just seconds from start of year
            tm_base.tm_mon = 0; tm_base.tm_mday = 1; tm_base.tm_hour = 0; tm_base.tm_min = 0; tm_base.tm_sec = 0;
            base_t -= mktime(&tm_base);

            }

    }
    //build next node
    bool no_next_node;  pnext_node = get_next_node(ini,name,&no_next_node);
	if (pnext_node == NULL)
	{if (!no_next_node) {*fpf_error << "ERROR:  "MY_CLASS_NAME"::init("<<init_name<<") failed to create next node ["<< conf[INI_COMMON_NEXT_NODE] <<"]\n";	return false;	}}
    else  {  if (! pnext_node->init(ini,conf[INI_COMMON_NEXT_NODE],pchain_arg))  { return false;}   }
    //
    *fpf_trace<<"=> " MY_CLASS_NAME "::init("<<name<<") starting from base time ["<<str_base_time<<"], zts at "<<zts_offset<<"\n";
    is_initialized = true;
    return true;
}

void CNode_ZTS::start(void)
{
}

void CNode_ZTS::stop(void)
{
}

void CNode_ZTS::close(void)
{
    if (pnext_node != NULL)   {  pnext_node->close();   delete pnext_node;   pnext_node = NULL;   }
    is_initialized = false;
    *fpf_trace<<"<= " MY_CLASS_NAME "e("<<name<<") closed, "<<c_counter<<" frames passed\n";
}

void CNode_ZTS::take_frame(CFrame* pf)
{
    FPF_ASSERT( (pf!= NULL),"NULL frame pointer")
    FPF_ASSERT(is_initialized,"Node not initialized")
    // custom node actions
    do_frame_processing(pf);
    //pass the frame further by chain
    if (pnext_node != NULL) { pnext_node->take_frame(p_output_frame); }
}

static int prev_t = 0;


void CNode_ZTS::do_frame_processing(CFrame* pf)
{
    FPF_ASSERT(zts_offset+ sizeof(ZTS) < NEW_FRAME_DATA_SIZE,"too big frame size to fit in CNOde_ZTS buffer");
    //
    c_counter++;
    //
    if (zts_offset == 0) //not explicitly defined yet - use input frame size and just append the ZTS
    {
        if (set_timestamp) {zts_offset = p_output_frame->frame_size;}
        else {zts_offset = pf->frame_size-sizeof(ZTS);}
    }
    //
    ZTS* pzts;
    p_output_frame = pf;
    if (set_timestamp)
    {
        // copy frame buffer and metadata to new structure
        if (zts_offset + sizeof(ZTS) > pf->frame_size ) // need to extend size and create new frame
        {
            new_frame = *(pf);
            new_frame.pdata = pframe_data;
            new_frame.frame_size = zts_offset + sizeof(ZTS);
            memset(pframe_data,0,new_frame.frame_size);
            memcpy(pframe_data,pf->pdata,pf->frame_size);
            p_output_frame = &new_frame;
        }else
        { p_output_frame = pf; }//use the original frame
        //---- format new timestamp
        //ver1-per number of frames
        //time_t mksec = base_mksec +  int ( 0.5+ ( frame_period * pf->cframe) / 1000.0 );
        //ver2 - using absolute offset in the input stream
        time_t mksec = base_mksec +  int ( 0.5+ ( 0.001*frame_period/pf->frame_size) * double(pf->stream_pos) );
        time_t add_sec = int(mksec / 1000000.);
        time_t frame_ts = base_t + add_sec;
        int frame_mksec = mksec % 1000000;
        //- update frame ACTIME for output frame
        if (f_update_actime)
        {
            p_output_frame->actime = base_utime + add_sec;
            p_output_frame->actime_usec = frame_mksec;
        }
        //-- set structure fields
        pzts = (ZTS*) (p_output_frame->pdata + zts_offset);
        pzts->seconds = frame_ts; fpf_swap4bytes((BYTE*)&(pzts->seconds));
        pzts->mkseconds = frame_mksec;fpf_swap4bytes((BYTE*)&(pzts->mkseconds));
        pzts->w1 = 0;
        pzts->w2 = 0;
    }else
    { //check ZTS position within existing frame
        FPF_ASSERT(zts_offset+ sizeof(ZTS) <= pf->frame_size,"ZTS offset is out of the frame size");
        pzts = (ZTS*) (p_output_frame->pdata + zts_offset);
    }
    //
    if (((dump_every >1 ) && (c_counter % dump_every == 1))
        || (dump_every == 1))
    {
        unsigned int s= pzts->seconds;fpf_swap4bytes((BYTE*)&s);
        unsigned int ms = pzts->mkseconds;fpf_swap4bytes((BYTE*)&ms);
        char sz[40];tm* ptm;

        time_t ts = s + EPOCH_2000;
        ptm = gmtime(&ts);
        ptm->tm_year = base_year-1900;
        strftime (sz,sizeof(sz),"%Y-%m-%d %H:%M:%S",ptm);
        string str_ts=sz;
        cout << name <<"-ZTS ["<<setw(7)<< pf->cframe <<"]\t"<< str_ts << "\t"<< s << "s  " << setw(6) << ms << "mks" << endl;
    }

}



