/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
CNode_SCTrigger - node switching frame stream on base of spacecraft ID

History:
    created: 2015-11 - A.Shumilin.
*/
#include <cstdlib>
#include <sstream>
#include "fpf.h"
#include "ccsds.h"
#include "class_factory.h"
#include "CNode_SCTrigger.h"

#define MY_CLASS_NAME   "CNode_SCTrigger"

// INI tags
#define INI_SC_IDS    "scids"
#define INI_SC_NAMES    "sc_names"
#define INI_CHAINS    "chains"
#define INI_REQ_MATCHES    "required_matches"

CNode_SCTrigger::CNode_SCTrigger()
{
    is_initialized = false;
    is_triggered = false;
    pnext_node = NULL;
    c_counter = 0;
    c_before_match_counter = 0;
    scid_triggered = 0;
    n_required_matches = N_DEF_REQUIRED_MATCHES;
}

CNode_SCTrigger::~CNode_SCTrigger()
{

}

bool CNode_SCTrigger::init(t_ini& ini, string& init_name, CChain* pchain_arg)
{
     *fpf_debug << "~ " MY_CLASS_NAME "::init("<<init_name<<")\n";
    pchain = pchain_arg;
    is_initialized = false;
    is_triggered = false;
    c_before_match_counter = 0;
    scid_triggered = 0;
    ini_copy = ini;
    //
    t_ini_section conf = ini[init_name];
    name = init_name;
    id = name;
    n_required_matches = N_DEF_REQUIRED_MATCHES;
    // -------------- do custom initialization here -------------------
    if (! conf[INI_REQ_MATCHES].empty())
    {
        n_required_matches = strtol(conf[INI_REQ_MATCHES].c_str(),NULL,10);
    }
    if (! conf[INI_SC_IDS].empty())
    {
        parse_to_int_vector(&v_scids,conf[INI_SC_IDS]);
    }else{
        *fpf_error << MY_CLASS_NAME "::init("<<name<<") ERROR: SC IDs list must be provided in the configuration\n";
        return false;
    }
    //
    if (! conf[INI_CHAINS].empty())
    {
        parse_to_string_vector(&v_chains,conf[INI_CHAINS]);
        if (v_chains.size() < v_scids.size())
        {
           *fpf_error << MY_CLASS_NAME "::init("<<name<<") ERROR: too short Chains list, provide a next node for each satellite\n";
            return false;
        }
    }else{
        *fpf_error << MY_CLASS_NAME "::init("<<name<<") ERROR: Chains list must be provided in the configuration\n";
        return false;
    }
    //-Sc names are optional
    if (! conf[INI_SC_NAMES].empty())
    {
        parse_to_string_vector(&v_sc_names,conf[INI_SC_NAMES]);
    }else
    {
        for(vector<int>::size_type i = 0; i != v_scids.size(); i++)
        {
            std::ostringstream  ss;
            ss<< v_scids[i];
            v_sc_names[i] =  ss.str() ;
        }
    }
    //
    *fpf_trace<<"=> " MY_CLASS_NAME "::init("<<name<<") initialized for satellites:";
    for(vector<int>::size_type i = 0; i != v_sc_names.size(); i++)
        { *fpf_trace << v_sc_names[i] << "," ;}
     *fpf_trace<<endl;
    is_initialized = true;
    return true;
}

void CNode_SCTrigger::start(void)
{
}

void CNode_SCTrigger::stop(void)
{
}

void CNode_SCTrigger::close(void)
{
    if (pnext_node != NULL)   {  pnext_node->close();   delete pnext_node;   pnext_node = NULL;   }
    is_initialized = false;
    *fpf_trace<<"<= " MY_CLASS_NAME "e("<<name<<") closed, "<<c_counter<<" frames passed\n";
}

void CNode_SCTrigger::take_frame(CFrame* pf)
{
    FPF_ASSERT( (pf!= NULL),"NULL frame pointer")
    FPF_ASSERT(is_initialized,"Node not initialized")
    //
    c_counter++;
    //
    if (! is_triggered) //check the frame to make SC decision
    {
        check_frame(pf);
    }
    // pass to next chain, starting from the first triggered frame,
    // pnext_node is valid only after triggering
    if( pnext_node != NULL)
    {
        pnext_node->take_frame(pf);
    }


}

void CNode_SCTrigger::check_frame(CFrame* pf)
{
    c_before_match_counter++;
    static int prev_sc = 0;
    static int c_matches = 0; //counter of matched scid
    #
    int scid =  (int)(CADU_GET_SPACECRAFT(pf->pdata));
    if (scid == prev_sc)
    {
        c_matches++;
        if (c_matches >= n_required_matches) //then match acquired
        {
            scid_triggered = scid;
            is_triggered = true;
            // check if this is a configured satellite
            for(vector<int>::size_type isat = 0; isat != v_scids.size(); isat++)
            {
                if (scid == v_scids[isat])
                {
                    *fpf_info<<"= " MY_CLASS_NAME "("<<name<<") triggered on "<<c_before_match_counter<<"th frame to "<<v_sc_names[isat]<<" ("<<scid<<")\n";
                    init_further_chain(isat);
                    return;
                }
            }
            // triggered satellite is not configured
            *fpf_info<<"= " MY_CLASS_NAME "("<<name<<") triggered on unconfigured Spacecraft: "<<scid<<"\n";
        }
    }else{ c_matches = 0; }
    prev_sc = scid;

}

void CNode_SCTrigger::init_further_chain(int isat)
{
    FPF_ASSERT(isat < v_chains.size(),"CNode_SCTrigger(): isat > v_chains.size");
    string next_node_name = v_chains[isat];
    *fpf_trace<<"= " MY_CLASS_NAME "("<<name<<") build chain, next node: "<<next_node_name<<"\n";
    pnext_node = new_node(ini_copy, next_node_name);
	if (pnext_node == NULL)
	{
	    *fpf_error << "ERROR:  " MY_CLASS_NAME "::init_further_chain("<<next_node_name<<") failed to create next node ["<< next_node_name <<"]\n";
	    return;
    }
    else
    {
        if (! pnext_node->init(ini_copy,next_node_name,pchain))
        {
            delete pnext_node;
            pnext_node = NULL;
            return ;
        }
    }
    //else, proceed with initialized pnext_node
    return;
}
