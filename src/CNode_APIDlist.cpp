/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
CNode_APIDlist- assemble a unique list of apids and their counts found in a stream.

*/

#include "ini.h"
#include "CNode_APIDlist.h"
#include "class_factory.h"


#define MY_CLASS_NAME   "CNode_APIDlist"

// INI tags


CNode_APIDlist::CNode_APIDlist()
{
    is_initialized = false;
    pnext_node = NULL;
    c_counter = 0;
    apid_linked_head = NULL;  // linked list of apid_linked_t w/ apid and count
}

CNode_APIDlist::~CNode_APIDlist()
{

}

bool CNode_APIDlist::init(t_ini& ini, string& init_name, CChain* pchain_arg)
{
     *fpf_debug << "~ " MY_CLASS_NAME "::init("<<init_name<<")\n";
    pchain = pchain_arg;
    is_initialized = false;
    //
    t_ini_section conf = ini[init_name];
    name = init_name;
    id = name;
    // -------------- do custom initialization here -------------------
    is_initialized = true;

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

void CNode_APIDlist::start(void)
{
}

void CNode_APIDlist::stop(void)
{
}

void CNode_APIDlist::close(void)
{
    if (pnext_node != NULL)   {  pnext_node->close();   delete pnext_node;   pnext_node = NULL;   }
    is_initialized = false;
    *fpf_trace<<"<= " MY_CLASS_NAME "e("<<name<<") closed, "<<c_counter<<" frames passed\n";
    *fpf_trace<<"<= " MY_CLASS_NAME "e("<<name<<") summary:\n";
    apid_linked_t *current_apid = apid_linked_head, *prev_apid = NULL;
    while (current_apid != NULL) {
        *fpf_trace<<"<= " MY_CLASS_NAME "e("<<name<<") apid "<<current_apid->apid<<": "<<current_apid->count<<":\n";
        prev_apid = current_apid;  
        current_apid = current_apid->pnext_apid;
        delete prev_apid;
    }
}

void CNode_APIDlist::take_frame(CFrame* pf)
{
    FPF_ASSERT( (pf!= NULL),"NULL frame pointer")
    FPF_ASSERT(is_initialized,"Node not initialized")
    // custom node actions
    do_frame_processing(pf);
    //pass the frame further by chain
    if (pnext_node != NULL) { pnext_node->take_frame(pf); }
}



void CNode_APIDlist::do_frame_processing(CFrame* pf)
{
    c_counter++;

    // so called https://github.com/mkirchner/linked-list-good-taste
    apid_linked_t **current_apid = &apid_linked_head;

    while ((*current_apid) != NULL && (*current_apid)->apid != pf->vcid) {
        current_apid = &(*current_apid)->pnext_apid;
    }

    if (current_apid == NULL) {
        apid_linked_t *new_apid = (apid_linked_t*)calloc(1, sizeof(apid_linked_t));
        new_apid->apid = pf->vcid;
        *current_apid = new_apid;
    }

    ((*current_apid)->count)++;

}



