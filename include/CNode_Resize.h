/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
CNode_Resize - change size (cut or extend) of passes frame

History:
    created: 2019-04 - A.Shumilin.
*/

#ifndef CNODE_RESIZE_H
#define CNODE_RESIZE_H

#include "fpf.h"

#define  FACTORY_CNODE_RESIZE(c) FACTORY_ADD_NODE(c,CNode_Resize)

class CNode_Resize : public INode
{
    public:
        CNode_Resize();
        virtual ~CNode_Resize();
        //INode
        bool init(t_ini& ini, string& name, CChain* chain);
        void start(void);
        void stop(void);
        void close(void);
        void take_frame(CFrame* pf);
    protected:
		
    private:
        unsigned int new_frame_size;
        bool done_warnings_init;
        int c_counter;
		BYTE* p_frame_buff;
        //
        void do_frame_processing(CFrame* pf);
};

#endif // CNODE_RESIZE_H
