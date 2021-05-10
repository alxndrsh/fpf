/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
CNode_Resize - change size of  passed frames

History:
    created: 2019-04 - A.Shumilin.
*/

#include "ini.h"

#include "class_factory.h"
#include "CNode_Resize.h"

#include <string.h>  //memcpy,memset

#define MY_CLASS_NAME   "CNode_Resize"

// INI tags
#define INI_FRAME_SIZE     "frame_size"

CNode_Resize::CNode_Resize()
{
    is_initialized = false;
    pnext_node = NULL;
    new_frame_size = 0;
    c_counter = 0;
    p_frame_buff = NULL;
    done_warnings_init = false;
}

CNode_Resize::~CNode_Resize()
{
	if (p_frame_buff != NULL) { delete p_frame_buff; p_frame_buff = NULL; }
    if (pnext_node != NULL) { *fpf_warn<<"!! CNode_Resize::~CNode_Resize(): not detached next node"; }
}

bool CNode_Resize::init(t_ini& ini, string& init_name, CChain* pchain_arg)
{
     *fpf_debug << "~ " MY_CLASS_NAME "::init("<<init_name<<")\n";
    done_warnings_init = false;
    pchain = pchain_arg;
    is_initialized = false;
    c_counter = 0;
    //
    t_ini_section conf = ini[init_name];
    name = init_name;
    id = name;
    //build next node
    bool no_next_node;
    pnext_node = get_next_node(ini,name,&no_next_node);
	if (pnext_node == NULL)
	{
		if (!no_next_node) //look like a config error
		{
			*fpf_error << "ERROR: CNode_Resize::init("<<init_name<<") failed to create next node ["<< conf[INI_COMMON_NEXT_NODE] <<"]\n";
			return false;
		}
	}
    else //init next node
    {
            if (! pnext_node->init(ini,conf[INI_COMMON_NEXT_NODE],pchain_arg))  { return false;}
    }
    //
    new_frame_size = atoi(conf[INI_FRAME_SIZE].c_str());
    //
    *fpf_trace<<"=> " MY_CLASS_NAME "::init("<<name<<") initialized with frame_size="<<new_frame_size<<"\n";
    is_initialized = true;
    return true;
}

void CNode_Resize::start(void)
{
}

void CNode_Resize::stop(void)
{
}

void CNode_Resize::close(void)
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

void CNode_Resize::take_frame(CFrame* pf)
{
    FPF_ASSERT( (pf!= NULL),"NULL frame pointer")
    FPF_ASSERT(is_initialized,"Node not initialized")
    // custom node actions
    do_frame_processing(pf);

    //pass the frame further by chain
    if (pnext_node != NULL) { pnext_node->take_frame(pf); }
}



void CNode_Resize::do_frame_processing(CFrame* pf)
{
    //increment the counter
    c_counter++;
    //-- do tracing
	if (pf->frame_size < new_frame_size)  // need to copy content to own buffer
	{
		if (p_frame_buff == NULL)
		{//allocate extended frame buffer
			p_frame_buff = new BYTE[new_frame_size];
			memset(p_frame_buff,0,new_frame_size);
		}
		memcpy(pf->pdata,p_frame_buff,pf->frame_size);
		pf->pdata = p_frame_buff;
	}
	pf->frame_size = new_frame_size; // regardless size relation 
}
