/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
CNode_RS - RS (Reed-Solomon) decoder node

History:
    created: 2015-11 - A.Shumilin.
*/


#include "ini.h"
#include "CNode_RS.h"
#include "class_factory.h"
#include "ccsds.h"
#include "decode_rs.h"

#define MY_CLASS_NAME   "CNode_RS"

// INI tags
#define INI_FIX_DATA    "fix_data"
#define INI_BLOCK_UNCORRECTED  "block_uncorrected"

CNode_RS::CNode_RS()
{
    is_initialized = false;
    pnext_node = NULL;
    interleaving_n = 4; // RS interleaving for 1024 CADU frames
    c_uncorrectable = 0;
    c_counter = 0;
    c_corrected = 0;
    c_corrected_bits = 0;
    c_clean = 0;
    block_uncorrected_frames = true;
    fix_data = true;
    c_dropped_frames = 0;
}

CNode_RS::~CNode_RS()
{

}

bool CNode_RS::init(t_ini& ini, string& init_name, CChain* pchain_arg)
{
     *fpf_debug << "~ " MY_CLASS_NAME "::init("<<init_name<<")\n";
    pchain = pchain_arg;
    is_initialized = false;
    //
    t_ini_section conf = ini[init_name];
    name = init_name;
    id = name;
    // -------------- do custom initialization here -------------------
    block_uncorrected_frames = true;
    if (!conf[INI_BLOCK_UNCORRECTED].empty())
    {
        if (conf[INI_BLOCK_UNCORRECTED] != INI_COMMON_VAL_YES) {block_uncorrected_frames = false;}
    }
    fix_data = true;
    if (!conf[INI_FIX_DATA].empty())
    {
        if (conf[INI_FIX_DATA] != INI_COMMON_VAL_YES) {fix_data = false;}
    }

    c_uncorrectable = 0;
    c_counter = 0;
    c_corrected = 0;
    c_corrected_bits = 0;
    c_clean = 0;
    c_dropped_frames = 0;
    //build next node
    bool no_next_node;  pnext_node = get_next_node(ini,name,&no_next_node);
	if (pnext_node == NULL)
	{if (!no_next_node) {*fpf_error << "ERROR:  " MY_CLASS_NAME "::init("<<init_name<<") failed to create next node ["<< conf[INI_COMMON_NEXT_NODE] <<"]\n";	return false;	}}
    else  {  if (! pnext_node->init(ini,conf[INI_COMMON_NEXT_NODE],pchain_arg))  { return false;}   }
    //
    *fpf_trace<<"=> " MY_CLASS_NAME "::init("<<name<<") initialized\n";
    is_initialized = true;
    return true;
}

void CNode_RS::start(void)
{
}

void CNode_RS::stop(void)
{
}

void CNode_RS::close(void)
{
    if (pnext_node != NULL)   {  pnext_node->close();   delete pnext_node;   pnext_node = NULL;   }
    is_initialized = false;
    *fpf_trace<<"<= " MY_CLASS_NAME "("<<name<<")closed, RS has fixed "<<c_corrected<<" frames of "<<c_counter<<", "<<c_uncorrectable<< " were uncorrectable, "<< c_dropped_frames<<" dropped\n";
}

void CNode_RS::take_frame(CFrame* pf)
{
    FPF_ASSERT( (pf!= NULL),"NULL frame pointer")
    FPF_ASSERT(is_initialized,"Node not initialized")
    // custom node actions
    bool ok = do_frame_processing(pf);
    //pass the frame further by chain
    if (!ok && block_uncorrected_frames) { c_dropped_frames++; return;}
    else{
        if (pnext_node != NULL) { pnext_node->take_frame(pf); }
    }
}

void CNode_RS::UpdateFrameAttributes(CFrame* pf)
{
    // post processing.
    if (pf->frame_type == FT_CCSDS_CADU)
    {
        // uodate with unscrambled values
        pf->scid = CADU_GET_SPACECRAFT(pf->pdata);
        pf->vcid = CADU_GET_VCID(pf->pdata);
    }
}

bool CNode_RS::do_frame_processing(CFrame* pf)
{
    c_counter++;
    //
    BYTE* pdata= pf->pdata + CCSDS_CADU_ASM_SIZE;

	int fixed_bits = 0;
	// collect interleaved words into plain code blocks
	BYTE blocks[interleaving_n][255];

	for(int i=0; i< interleaving_n ; i++)
	{
		for( int j=0; j<255; j++)
		{
				blocks[i][j] =  pdata[i+j*4];
		}
	}
	for(int i=0; i<interleaving_n; i++)
	{
			int fix = decode_rs_ccsds((data_t*)blocks[i], NULL, 0, 0);
			if (fix == -1)
			{
				c_uncorrectable++;
				pf->crc_ok = FRAME_CRC_FAILED;
				pf->bit_errors = -1;
				return false; //TODO - how to add to bit errors in this case ?
			}
			fixed_bits += fix;
	}
    //
	if (0==fixed_bits)
	{
		c_clean++;
		pf->crc_ok = FRAME_CRC_OK;
		pf->bit_errors = 0;
		return true;
	}
	//skip filler frames
	if (VCID_FILL == CADU_GET_VCID(pf->pdata)) {return true;}

	//else some bits have been corrected
    c_corrected++;
    c_corrected_bits += fixed_bits;
    pf->bit_errors += fixed_bits;
    pf->crc_ok = FRAME_CRC_FAILED;
    if ( fix_data )//interleave data back into the frame
    {
        for(int i=0; i< interleaving_n ; i++)
        {
            for( int j=0; j<255; j++)
            {
				 pdata[i+j*4] = blocks[i][j];
            }
        }
        pf->crc_ok = FRAME_CRC_OK; //now frame should be OK
        UpdateFrameAttributes(pf);
        return true;
    }//else, leave frame with errors
    return false;

}
