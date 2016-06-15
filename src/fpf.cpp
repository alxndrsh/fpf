/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
fpf.h - primary FPF definitions.
 Contains definitions for base classes and structures.

History:
    created: 2016-06 - A.Shumilin.
*/
#include <fpf.h>
using namespace std;


// common output tools
const unsigned short FPF_OUT_ERROR = 0x0001;
const unsigned short FPF_OUT_WARNING = 0x0002;
const unsigned short FPF_OUT_INFO = 0x0004;
const unsigned short FPF_OUT_ALL = 0xFFFF;
const unsigned short FPF_OUT_NOTHING = 0x0000;

unsigned short fpf_out_mask = FPF_OUT_ALL;

ostream* fpf_error = &cerr;
ostream* fpf_warn = &cerr;
ostream* fpf_info = &cout;
ostream* fpf_debug = &cerr;
ostream* fpf_trace = &cerr;

string fpf_last_error;

// == IFrameSource dummy implementation ===
IFrameSource::IFrameSource()
{
    pchain = NULL;
    pnext_node = NULL;
};
IFrameSource::~IFrameSource() {};
bool IFrameSource::init(t_ini& ini, string& name) { return false;}
void IFrameSource::start(void) {};
void IFrameSource::close(void) {};

//== INode dummy implementation ===
INode::INode()
{
    pnext_node = NULL;
    is_initialized = false;
    pchain = NULL;
};
INode::~INode()
{
    if (pnext_node != NULL) { *fpf_warn<<"!! INode: not detached next node"; }
};
bool INode::init(t_ini& ini, string& name, CChain* pchain) {return false;};
void INode::start(void) {};
void INode::stop(void) {};
void INode::close(void) {};
void INode::take_frame(CFrame* pf) {};

//== IInputStream ==
IInputStream::IInputStream () {};
IInputStream::~IInputStream() {};
bool IInputStream::init(t_ini& ini, string& name) { return false; };
void IInputStream::start(void) {};
void IInputStream::stop(void) {};
void IInputStream::close(void) {};
unsigned int IInputStream::read(BYTE* pbuff, size_t bytes_to_read,int& ierr) { return 0; };

CStream::CStream()
{
    curr_pos = 0;
    wct_base= 0;
    wct_base_usec= 0;
    acqt_base= 0;
    acqt_base_usec= 0;
    obt_base= 0;
    obt_base_usec= 0;
    c_frames = 0;
}

CFrame::CFrame()
{
    pdata = NULL;
    pstream = NULL; //pointer to the Stream
    frame_size = 0;
    frame_type = FT_NOTYPE;
    stream_pos = 0;
    vcid = 0;
    scid = 0;
    cframe = 0; // frame counter as issued by framer
    crc_ok = FRAME_CRC_NOTCHECKED;
    bit_errors = FRAME_CRC_NOTCHECKED;
    wctime = 0;    wctime_usec = 0;
    actime = 0;    actime_usec = 0;
    obtime = 0;    obtime_usec = 0;
}

///
void fpf_swap4bytes(BYTE* pb)
{
    BYTE b0 = pb[0];
    BYTE b1 = pb[1];
    pb[0] = pb[3];
    pb[1] = pb[2];
    pb[3] = b0;
    pb[2] = b1;
}
