/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
FrameSource_Simulator. - Trivial framer object emitting simulated frames

History:
    created: 2015-11 - A.Shumilin.
*/
#include <string>
#include <cstdlib>

#include <time.h>       /* clock_t, clock, CLOCKS_PER_SEC */
#include "CFrameSource_Simulator.h"
#include "ccsds.h"
using namespace std;

#define MY_CLASS_NAME   "CFrameSource_Simulator"
// INI config parameter tags

/* INI_FRAME_TYPE - type of frames t osimulate, following constants define supported values */
#define INI_FRAME_TYPE  "frame_type"
const string FTYPE_CCSDS_CADU("ccsds_cadu");
const string FTYPE_MODIS_DAY("modis_day");

/* INI_FRAME_PERIOD - frame to next frame period, in usec, defines simulation frame rate and throttling */
#define INI_FRAME_PERIOD "period"

/**/
#define INI_FRAME_NUMBER    "number_of_frames"

//default number of frames to simulate
#define  DEFAULT_FRAME_NUMBER  100
///////

CFrameSource_Simulator::CFrameSource_Simulator()
{
     *fpf_trace<<"= CFrameSource_Simulator::CFrameSource_Simulator()\n";
     //
     is_initialized = false;
     frame_buffer = NULL;
     frame_size = 0;
     frame_number = DEFAULT_FRAME_NUMBER;
     frame_type = FTYPE_CCSDS_CADU;
     frame_period = 1;
     //
     pnext_node = NULL;
}

CFrameSource_Simulator::~CFrameSource_Simulator()
{
    *fpf_trace<<"= CFrameSource_Simulator::~CFrameSource_Simulator()\n";
    //
    if (frame_buffer != NULL) { delete [] frame_buffer; }
}

bool CFrameSource_Simulator::init(t_ini& ini, string& init_name)
{
     *fpf_debug << "~ " MY_CLASS_NAME "::init("<<init_name<<")\n";
    fpf_last_error.clear();
    is_initialized = false;
    name = init_name;
    t_ini_section conf = ini[init_name];
try
{ // next actions may throw exceptions...

    //initialize parameters
    if (conf.find(INI_FRAME_TYPE) != conf.end()) { frame_type = conf[INI_FRAME_TYPE]; }
    if (conf.find(INI_FRAME_PERIOD) != conf.end())
    {
        frame_period = atoi(conf[INI_FRAME_TYPE].c_str());
    }
    if (conf.find(INI_FRAME_NUMBER) != conf.end())
    {
        frame_number = atoi(conf[INI_FRAME_NUMBER].c_str());
    }else { frame_number = DEFAULT_FRAME_NUMBER; }


    //setup buffer and derived parameters
    if (frame_type == FTYPE_CCSDS_CADU)
    {
        frame_size = sizeof(uCADU);

    }

    // assert that we are ready to start
    if (frame_size < 1)
    {
        *fpf_warn<< "!! CFrameSource_Simulator::init(): frame_size <1, init failes\n";
        return false;
    }
    frame_buffer = new BYTE[frame_size];
    if (frame_buffer == NULL)
    {
        *fpf_warn<< "!! CFrameSource_Simulator::init(): failed allocation of frame buffer of size "<<frame_size<<" bytes\n";
        return false;
    }

    ///// connect and init further cahin
     // connect the first node
    if (conf.find(INI_COMMON_NEXT_NODE) != conf.end())
    {
        pnext_node = new_node(ini[conf[INI_COMMON_NEXT_NODE]][INI_COMMON_CLASS]); // NULL is allowed here
        if ( pnext_node != NULL)
        {
            if (pnext_node->init(ini,conf[INI_COMMON_NEXT_NODE],pchain))
            {
                //NOP
            }else{
                //init failed in the chain
                is_initialized = false;
                return false;
            }
        }
    }else { pnext_node = NULL; }

}catch (const std::exception& ex)
{
        *fpf_error<<"ERROR: CFrameSource_Simulator::ini() failed with exception:\n"<<ex.what()<<endl;
        return false;
}
catch(...)
{
        *fpf_error<<"ERROR: CFrameSource_Simulator::ini() failed with nonstd exception\n";
        return false;
}

    //OK

    *fpf_trace<<"= CFrameSource_Simulator("<<name<<") initialized\n";
    is_initialized = true;
    return true;
}

void CFrameSource_Simulator::start(void)
{
    *fpf_trace<<"= CFrameSource_Simulator.start()\n";
    if (! is_initialized)
    {
        *fpf_error<<"ERROR: CFrameSource_Simulator::start() called on not properly initialized object\n";
        return;
    }
    if (pnext_node == NULL)
    {
        *fpf_warn<<"!! CFrameSource_Simulator::start() - no sense to start with out chain node attached, check next_node= param\n";
        return;
    }
    fpf_last_error.clear();

    // prepare frame structures
    CFrame  frame; //auto allocated here
    frame.frame_size = CCSDS_CADU_SIZE;
    frame.pdata = frame_buffer;
    frame.frame_type  = FT_NOTYPE;
    frame.stream_pos = 0;
    // enter frame generation cycle
    clock_t clock_next_frame;
    clock_t clock_period = frame_period * CLOCKS_PER_SEC / 1000000; //usec to clock units;
    for (size_t iframe=0; iframe <  frame_number; iframe++)
    {
        clock_next_frame = clock() + clock_period;
        //--setup frame current fields
        frame.wctime = time(NULL);
        //
        pnext_node->take_frame(&frame);
        //frame rate throttle
        while (clock() < clock_next_frame) //wait till the next frame time will come
        {
            //NOP. TO DO - yield CPU for long waits
            //cout << clock_next_frame << "+" << clock_period <<"==\n";
        }

    } //frames cycle

    // stop the chain

}

void CFrameSource_Simulator::close(void)
{
     *fpf_trace<<"= CFrameSource_Simulator.close()\n";
     if (pnext_node != NULL)
     {
        pnext_node->close();
        delete pnext_node;
        pnext_node = NULL;
     }
}
