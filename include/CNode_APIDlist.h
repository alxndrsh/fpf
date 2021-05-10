/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
CNode_APIDlist -  assemble a unique list of apids and their counts found in a stream.

*/

#ifndef CNODE_APIDLIST_H
#define CNODE_APIDLIST_H

#include "fpf.h"

#define  FACTORY_CNODE_APIDLIST(c) FACTORY_ADD_NODE(c,CNode_APIDlist)

struct apid_linked_t {
    int apid;
    unsigned long count;
    apid_linked_t* pnext_apid;
};

class CNode_APIDlist : public INode
{
    public:
        CNode_APIDlist();
        virtual ~CNode_APIDlist();
        //INode
        bool init(t_ini& ini, string& name, CChain* chain);
        void start(void);
        void stop(void);
        void close(void);
        void take_frame(CFrame* pf);
    protected:
    private:
        unsigned long c_counter;
        apid_linked_t *apid_linked_head;

        //
        void do_frame_processing(CFrame* pf);

};
#endif // CNODE_APIDLIST_H
