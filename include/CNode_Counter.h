/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
CNode_Counter - simple counter of passing through frames

History:
    created: 2015-11 - A.Shumilin.
*/

#ifndef CNODE_COUNTER_H
#define CNODE_COUNTER_H

#include "fpf.h"

#define  FACTORY_CNODE_COUNTER(c) FACTORY_ADD_NODE(c,CNode_Counter)

class CNode_Counter : public INode
{
    public:
        CNode_Counter();
        virtual ~CNode_Counter();
        //INode
        bool init(t_ini& ini, string& name, CChain* chain);
        void start(void);
        void stop(void);
        void close(void);
        void take_frame(CFrame* pf);
    protected:
    private:
        unsigned long c_counter;
        unsigned long trace_every;
        bool done_warnings_init;

        //
        void do_frame_processing(CFrame* pf);
};

#endif // CNODE_COUNTER_H
