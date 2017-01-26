/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
CNode_Splitter - node routing different frames to different branches using some filter criteria

History:
    created: 2015-11 - A.Shumilin.
*/

#ifndef CNODE_SPLITTER_H
#define CNODE_SPLITTER_H

#include <vector>
#include "fpf.h"

#define  FACTORY_CNODE_SPLITTER(c) FACTORY_ADD_NODE(c,CNode_Splitter)

class CNode_Splitter : public INode
{
    public:
        CNode_Splitter();
        virtual ~CNode_Splitter();
        //INode
        bool init(t_ini& ini, string& name, CChain* chain);
        void start(void);
        void stop(void);
        void close(void);
        void take_frame(CFrame* pf);
    protected:
    private:
        unsigned long c_counter;
        unsigned long c_to_side_chain;
        unsigned long c_pass_through;
        bool is_exclusive;
        INode* side_node;
        vector<int> filter_vcid;
        vector<int> filter_scid;
        //
        bool meets_conditions(CFrame* pf);

};

#endif // CNODE_SPLITTER_H
