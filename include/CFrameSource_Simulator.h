/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
FrameSource_Simulator. - Trivial framer object emitting simulated frames

History:
    created: 2015-11 - A.Shumilin.
*/
#ifndef CFRAMESOURCE_SIMULATOR_H
#define CFRAMESOURCE_SIMULATOR_H

#include "fpf.h"
#include "class_factory.h"

#define  FACTORY_CFRAMESOURCE_SIMULATOR(c) FACTORY_ADD_FRAMESOURCE(c,CFrameSource_Simulator)


class CFrameSource_Simulator : public IFrameSource
{
    public:
        CFrameSource_Simulator();
        virtual ~CFrameSource_Simulator();
        //IFrameSource implementation
        bool init(t_ini& ini, string& name);
        void start(void);
        void close(void);
        bool is_initialized;
    protected:
    private:

        string frame_type;
        size_t  frame_number;
        size_t frame_size;
        int frame_period;
        BYTE* frame_buffer;
};

#endif // CFRAMESOURCE_SIMULATOR_H
