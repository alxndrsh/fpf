/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
CNode_SCTrigger - node switching frame stream on base of spacecraft ID

History:
    created: 2015-11 - A.Shumilin.
*/
#ifndef CNODE_SCTRIGGER_H
#define CNODE_SCTRIGGER_H

#include "fpf.h"

#define  FACTORY_CNODE_SCTRIGGER(c) FACTORY_ADD_NODE(c,CNode_SCTrigger)

#define N_DEF_REQUIRED_MATCHES  3

class CNode_SCTrigger : public INode
{
    public:
        CNode_SCTrigger();
        virtual ~CNode_SCTrigger();
        //INode
        bool init(t_ini& ini, string& name, CChain* chain);
        void start(void);
        void stop(void);
        void close(void);
        void take_frame(CFrame* pf);

        INode* pnext_node;
    protected:
    private:
        unsigned long c_counter;
        unsigned long c_before_match_counter;
        bool is_triggered;
        int n_required_matches;
        int scid_triggered;

        vector<int> v_scids;
        vector<string> v_sc_names;
        vector<string> v_chains;
        t_ini ini_copy ; //need a copy for delayed chain building
        //
        void check_frame(CFrame* pf);
        void init_further_chain(int isat);

};
#endif // CNODE_SCTRIGGER_H
