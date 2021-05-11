/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
CNode_template -  this is just a course code template to help initiate new processing node classes

*/

#include "ini.h"
#include "CNode_template.h"
#include "class_factory.h"


#define MY_CLASS_NAME   "CNode_Template"

// INI tags



CNode_Template::CNode_Template()
{
    is_initialized = false;
    pnext_node = NULL;
    c_counter = 0;
}

CNode_Template::~CNode_Template()
{

}

bool CNode_Template::init(t_ini& ini, string& init_name, CChain* pchain_arg)
{
     *fpf_debug << "~ " MY_CLASS_NAME "::init("<<init_name<<")\n";
    pchain = pchain_arg;
    is_initialized = false;
    //
    t_ini_section conf = ini[init_name];
    name = init_name;
    id = name;
    // -------------- do custom initialization here -------------------
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

void CNode_Template::start(void)
{
}

void CNode_Template::stop(void)
{
}

void CNode_Template::close(void)
{
    if (pnext_node != NULL)   {  pnext_node->close();   delete pnext_node;   pnext_node = NULL;   }
    is_initialized = false;
    *fpf_trace<<"<= " MY_CLASS_NAME "e("<<name<<") closed, "<<c_counter<<" frames passed\n";
}

void CNode_Template::take_frame(CFrame* pf)
{
    FPF_ASSERT( (pf!= NULL),"NULL frame pointer")
    FPF_ASSERT(is_initialized,"Node not initialized")
    // custom node actions
    do_frame_processing(pf);
    //pass the frame further by chain
    if (pnext_node != NULL) { pnext_node->take_frame(pf); }
}



void CNode_Template::do_frame_processing(CFrame* pf)
{
    c_counter++;


}



