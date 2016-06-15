/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
CNode_Splitter - node routing different frames to different branches using some filter criteria

History:
    created: 2015-11 - A.Shumilin.
*/

#include "ini.h"
#include "CNode_Splitter.h"
#include "class_factory.h"


#define MY_CLASS_NAME   "CNode_Splitter"

// INI tags
#define INI_SIDE_NODE   "side_node"
#define INI_EXCLUSIVE   "exclusive"
#define INI_FILTER_SCID  "filter_scid"
#define INI_FILTER_VCID  "filter_vcid"

CNode_Splitter::CNode_Splitter()
{
    is_initialized = false;
    pnext_node = NULL;
    side_node = NULL;
    is_exclusive = false;
    c_counter = 0UL;
    c_to_side_chain = 0UL;
    c_pass_through = 0UL;
    //
}

CNode_Splitter::~CNode_Splitter()
{

}

bool CNode_Splitter::init(t_ini& ini, string& init_name, CChain* pchain_arg)
{
     *fpf_debug << "~ " MY_CLASS_NAME "::init("<<init_name<<")\n";
    pchain = pchain_arg;
    is_initialized = false;
    //
    t_ini_section conf = ini[init_name];
    name = init_name;
    id = name;
    //-- filteres
    if (! conf[INI_FILTER_VCID].empty())
    {
        parse_to_int_vector(&filter_vcid,conf[INI_FILTER_VCID]);
    }
    if (! conf[INI_FILTER_SCID].empty())
    {
        parse_to_int_vector(&filter_scid,conf[INI_FILTER_SCID]);
    }
    //
     c_counter = 0UL;
    c_to_side_chain = 0UL;
    c_pass_through = 0UL;
    //
    if ( conf[INI_EXCLUSIVE] == INI_COMMON_VAL_YES) {is_exclusive = true;}
    //build next node
    bool no_next_node;  pnext_node = get_next_node(ini,name,&no_next_node);
	if (pnext_node == NULL)
	{if (!no_next_node) {*fpf_error << "ERROR:  "MY_CLASS_NAME"::init("<<init_name<<") failed to create next node ["<< conf[INI_COMMON_NEXT_NODE] <<"]\n";	return false;	}}
    else  {  if (! pnext_node->init(ini,conf[INI_COMMON_NEXT_NODE],pchain_arg))  { return false;}   }
     // build side node
    string side_node_name = conf[INI_SIDE_NODE];
    if (!side_node_name.empty())
    { //this is not an error
         side_node = new_node(ini,side_node_name);
         if (side_node == NULL)
         {
             *fpf_error << "ERROR:"MY_CLASS_NAME"::init("<<init_name<<") failed to create side node ["<< side_node_name <<"]\n";
             return false;
         }else
         {
            if (! side_node->init(ini,conf[INI_SIDE_NODE],pchain_arg))  { return false;}
         }
    }//else - no side node was given
    //
    *fpf_trace<<"=> " MY_CLASS_NAME "::init("<<name<<") initialized\n";
    is_initialized = true;
    return true;
}

void CNode_Splitter::start(void)
{
}

void CNode_Splitter::stop(void)
{
}

void CNode_Splitter::close(void)
{
    if (side_node != NULL)   {  side_node->close();   delete side_node;   side_node = NULL;   }
    if (pnext_node != NULL)   {  pnext_node->close();   delete pnext_node;   pnext_node = NULL;   }
    is_initialized = false;
    *fpf_trace<<"<= " MY_CLASS_NAME "("<<name<<") closed, "<<c_counter<< " input frames," <<c_to_side_chain<< " to side chain + "<<c_pass_through<<" passed straight\n";
}

void CNode_Splitter::take_frame(CFrame* pf)
{
    FPF_ASSERT( (pf!= NULL),"NULL frame pointer")
    FPF_ASSERT(is_initialized,"Node not initialized")
    // custom node actions
    c_counter++;
    //
    if (meets_conditions(pf))
    {
        //first - straight pass
        if (!is_exclusive && (pnext_node != NULL)) { pnext_node->take_frame(pf); c_pass_through++;}
        // after that - side chain
        if (side_node != NULL) { side_node->take_frame(pf); c_to_side_chain++;}
    }else
    {
        if (pnext_node != NULL) { pnext_node->take_frame(pf); c_pass_through++;}
    }
    // split the processing

}



bool CNode_Splitter::meets_conditions(CFrame* pf)
{
    bool vcid_ok = false;
    for(vector<int>::size_type i = 0; i != filter_vcid.size(); i++)
    {
        if (pf->vcid == filter_vcid[i])
        {
            vcid_ok = true;
            break;
        }
    }
    if (!vcid_ok && (filter_vcid.size() > 0)) { return false; }
    //
     bool scid_ok = false;
    for(vector<int>::size_type i = 0; i != filter_scid.size(); i++)
    {
        if (pf->scid == filter_scid[i]) {scid_ok = true; break;}
    }
    if (!scid_ok && (filter_scid.size() > 0)) { return false; }
    //
    return true; // if all the checks have been  passed
}
