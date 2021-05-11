/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
CNode_Counter - simple counter of passing through frames

History:
    created: 2015-11 - A.Shumilin.
*/
#include <cstdlib> /* atoi */
#include <iomanip>  /* setw */

#include "ini.h"
#include "CNode_Counter.h"
#include "class_factory.h"


#define MY_CLASS_NAME   "CNode_Counter"

// INI tags
#define INI_TRACE_EVERY     "trace_every"

CNode_Counter::CNode_Counter()
{
    is_initialized = false;
    pnext_node = NULL;
    c_counter = 0;
    trace_every = 0; //no tracing
    done_warnings_init = false;
}

CNode_Counter::~CNode_Counter()
{
    if (pnext_node != NULL) { *fpf_warn<<"!! CNode_Counter::~CNode_Counter(): not detached next node"; }
}

bool CNode_Counter::init(t_ini& ini, string& init_name, CChain* pchain_arg)
{
     *fpf_debug << "~ " MY_CLASS_NAME "::init("<<init_name<<")\n";
    c_counter = 0;
    done_warnings_init = false;
    pchain = pchain_arg;
    is_initialized = false;
    //
    t_ini_section conf = ini[init_name];
    name = init_name;
    id = name;
    //build next not
    bool no_next_node;
    pnext_node = get_next_node(ini,name,&no_next_node);
	if (pnext_node == NULL)
	{
		if (!no_next_node) //look like a config error
		{
			*fpf_error << "ERROR: CNode_Counter::init("<<init_name<<") failed to create next node ["<< conf[INI_COMMON_NEXT_NODE] <<"]\n";
			return false;
		}
	}
    else //init next node
    {
            if (! pnext_node->init(ini,conf[INI_COMMON_NEXT_NODE],pchain_arg))  { return false;}
    }
    //
    trace_every = atoi(conf[INI_TRACE_EVERY].c_str());
    //
    *fpf_trace<<"=> " MY_CLASS_NAME "::init("<<name<<") initialized\n";
    is_initialized = true;
    return true;
}

void CNode_Counter::start(void)
{
}

void CNode_Counter::stop(void)
{
}

void CNode_Counter::close(void)
{
    if (pnext_node != NULL)
    {
        pnext_node->close();
        delete pnext_node;
        pnext_node = NULL;
    }
    is_initialized = false;
    done_warnings_init = false;
    *fpf_trace<<"<= " MY_CLASS_NAME "("<<name<<") closed.  Counted frames: "<<c_counter<<"\n";
}

void CNode_Counter::take_frame(CFrame* pf)
{
    FPF_ASSERT( (pf!= NULL),"NULL frame pointer")
    FPF_ASSERT(is_initialized,"Node not initialized")
    // custom node actions
    do_frame_processing(pf);

    //pass the frame further by chain
    if (pnext_node != NULL) { pnext_node->take_frame(pf); }
}



void CNode_Counter::do_frame_processing(CFrame* pf)
{
    //increment the counter
    c_counter++;
    //-- do tracing
    if (trace_every == 0) { return;} //no tracing at all
    if ((trace_every == 1)
        || (c_counter % trace_every == 0))
    {
        cout<<"Counter::"<<name<<" ="<<setw(8)<<right<<c_counter<<endl;
    }
}
