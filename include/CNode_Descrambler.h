/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
CNode_Descrambler - descrambling (derandomizing) node, XORs frame content with given PNS

History:
    created: 2015-11 - A.Shumilin.
*/

#ifndef CNODE_DESCRAMBLER_H
#define CNODE_DESCRAMBLER_H

#include "fpf.h"

#define  FACTORY_CNODE_DESCRAMBLER(c) FACTORY_ADD_NODE(c,CNode_Descrambler)

class CNode_Descrambler : public INode
{
    public:
        CNode_Descrambler();
        virtual ~CNode_Descrambler();
        //INode
        bool init(t_ini& ini, string& name, CChain* chain);
        void start(void);
        void stop(void);
        void close(void);
        void take_frame(CFrame* pf);
    protected:
    private:
        unsigned long c_counter;
        BYTE* pns;
        size_t pns_size;
        string pns_type;
        size_t offset_in_frame;
        size_t descramble_bytes;
        //
        void do_frame_processing(CFrame* pf);
        bool generate_PNS(string& pns_id);
};

#endif // CNODE_DESCRAMBLER_H
