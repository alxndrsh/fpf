/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
CNode_template -  this is just a course code template to help initiate new processing node classes

*/

#ifndef CNODE_CADUDUMP_H
#define CNODE_CADUDUMP_H

#include "fpf.h"

#define  FACTORY_CNODE_Template(c) FACTORY_ADD_NODE(c,CNode_Template)

class CNode_Template : public INode
{
    public:
        CNode_Template();
        virtual ~CNode_Template();
        //INode
        bool init(t_ini& ini, string& name, CChain* chain);
        void start(void);
        void stop(void);
        void close(void);
        void take_frame(CFrame* pf);
    protected:
    private:
        unsigned long c_counter;

        //
        void do_frame_processing(CFrame* pf);

};
#endif // CNODE_CADUDUMP_H
