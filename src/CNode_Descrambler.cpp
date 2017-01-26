/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
CNode_Descrambler - descrambling (derandomizing) node, XORs frame content with given PNS

History:
    created: 2015-11 - A.Shumilin.
*/
#include <cstdlib> /* atoi */
#include <cstring>  /* memset */

#include "ini.h"
#include "ccsds.h"
#include "CNode_Descrambler.h"
#include "class_factory.h"


#define MY_CLASS_NAME   "CNode_Descrambler"

// INI tags
#define INI_PNS_TYPE    "pns_type"

/* Offset to a fist byte to start descrambling, starting from the cadu CADU beginning, including ASM, so should be >=4 */
#define INI_OFFSET_IN_FRAME     "offset_in_frame"
/* How many bytes to descramble */
#define INI_DESCRAMBLE_BYTES   "descramble_bytes"

#define PNS_CCSDS_CADU_ID  "ccsds_cadu"
#define PNS_CCSDS_CADU_SIZE     1020


CNode_Descrambler::CNode_Descrambler()
{
    is_initialized = false;
    pnext_node = NULL;
    pns = NULL;
    pns_type = PNS_CCSDS_CADU_ID;// default CCSDS PNS
    offset_in_frame = 4; //just skip the ASM
    descramble_bytes = PNS_CCSDS_CADU_SIZE;
    c_counter = 0;
}

CNode_Descrambler::~CNode_Descrambler()
{
    if (pnext_node != NULL) { *fpf_warn<<"!! CNode_Descrambler::~CNode_Descrambler(): not detached next node"; }
    if (pns != NULL) { delete [] pns; }
}

bool CNode_Descrambler::init(t_ini& ini, string& init_name, CChain* pchain_arg)
{
     *fpf_debug << "~ " MY_CLASS_NAME "::init("<<init_name<<")\n";
    pchain = pchain_arg;
    is_initialized = false;
    //
    t_ini_section conf = ini[init_name];
    name = init_name;
    id = name;
    c_counter = 0;
    //
    if (conf.find(INI_OFFSET_IN_FRAME) != conf.end())
    {
            offset_in_frame = strtoul(conf[INI_OFFSET_IN_FRAME].c_str(),NULL,10);
    }
    //
    if (conf.find(INI_DESCRAMBLE_BYTES) != conf.end())
    {
            descramble_bytes = strtoul(conf[INI_DESCRAMBLE_BYTES].c_str(),NULL,10);
    }
    //-- prepare PNS,must be done after set of descramble_bytes
    if (conf.find(INI_PNS_TYPE) != conf.end())  {  pns_type = conf[INI_PNS_TYPE];  }
    if (! generate_PNS(pns_type))
    {
            *fpf_error << "ERROR: "MY_CLASS_NAME"::init("<<init_name<<") filed to prepare PNS ["<< pns_type <<"]\n";
			return false;
    }else{
       // *fpf_trace << "=> "MY_CLASS_NAME"::init("<<init_name<<") using PNS ["<< pns_type <<"]\n";
    }
    //build next node
    bool no_next_node;
    pnext_node = get_next_node(ini,name,&no_next_node);
	if (pnext_node == NULL)
	{
		if (!no_next_node) //look like a config error
		{
			*fpf_error << "ERROR:  "MY_CLASS_NAME"::init("<< init_name <<") failed to create next node ["<< conf[INI_COMMON_NEXT_NODE] <<"]\n";
			return false;
		}
	}
    else //init next node
    {
            if (! pnext_node->init(ini,conf[INI_COMMON_NEXT_NODE],pchain_arg))  { return false;}
    }
    //
    *fpf_trace<<"=> " MY_CLASS_NAME "::init("<<name<<") initialized\n";
    is_initialized = true;
    return true;
}

void CNode_Descrambler::start(void)
{
}

void CNode_Descrambler::stop(void)
{
}

void CNode_Descrambler::close(void)
{
    if (pnext_node != NULL)   {  pnext_node->close();   delete pnext_node;   pnext_node = NULL;   }
    is_initialized = false;
    *fpf_trace<<"<= " MY_CLASS_NAME "("<<name<<") closed, "<<c_counter<<" frames passed\n";
}

void CNode_Descrambler::take_frame(CFrame* pf)
{
    FPF_ASSERT( (pf!= NULL),"NULL frame pointer")
    FPF_ASSERT(is_initialized,"Node not initialized")
    // custom node actions
    do_frame_processing(pf);
    //pass the frame further by chain
    if (pnext_node != NULL) { pnext_node->take_frame(pf); }
}



void CNode_Descrambler::do_frame_processing(CFrame* pf)
{
    c_counter++;
    FPF_ASSERT((offset_in_frame < pf->frame_size), "CNode_Descrambler:: offset_in_frame points ot of the frame");
    if (offset_in_frame+descramble_bytes > pf->frame_size)
    {
        *fpf_warn<<"!! CNode_Descrambler::do_frame_processing() adjusts descramble_bytes to fit in frame size "<<pf->frame_size<<endl;
        descramble_bytes = pf->frame_size - offset_in_frame;
    }
    //
    for(size_t i = 0; i < descramble_bytes; i++)
    {
        pf->pdata[offset_in_frame+i] = pf->pdata[offset_in_frame+i] ^ pns[i];
    }

    // post processing.
    if (pf->frame_type == FT_CCSDS_CADU)
    {
        // uodate with unscrambled values
        pf->scid = CADU_GET_SPACECRAFT(pf->pdata);
        pf->vcid = CADU_GET_VCID(pf->pdata);
    }

}

//// ------ helpers to fill PN sequenses
void fill_pns_ccsdscadu(BYTE* pns, int pns_size)
/*
    Generates PNS using  standard CCSDS polynomial: x^8 + x^7 + x^5 + x^3 + 1.
    Code simulates LFSR
*/
{
    memset(pns,0,pns_size);//clean buffer

    int R[8]; // software LFSR, 8 bit register
    for(int ir=0; ir<8; ir++) { R[ir] = 1; }

    // iterate through 1020*8 bits
    int nbits = pns_size * 8;
    for(int i=0; i<nbits; i++) // bit cycle
    {
		if (R[0] == 1)  //output of the LFSR, set pns bit to 1
		{  pns[i/8] |= (1<< ( (7-(i%8)) )); }

		// MSBit, sum modulo2
		int msbit = ((R[0] + R[3] + R[5]+ R[7]) % 2);
		// do shift
		for(int r=0; r<=6; r++)  {	R[r] = R[r+1]; 	}
		// fill the MSB
		R[7] = msbit;
    }
};

bool CNode_Descrambler::generate_PNS(string& pns_id)
{
    if (pns_id == PNS_CCSDS_CADU_ID )
    {
        pns_size = PNS_CCSDS_CADU_SIZE;
        size_t buff_size = pns_size;
        if (buff_size < descramble_bytes) {buff_size = descramble_bytes;} ; //need to extend buffer and fill it with duplicates of the PNS
        pns = new BYTE[pns_size];
        fill_pns_ccsdscadu(pns,pns_size);
        //
        if (pns_size<buff_size)
        {
            for (size_t j=pns_size; j<buff_size; j++)
            { pns[j] = pns[j % pns_size];}
        }
    }
    else
    {
        *fpf_error<<"ERROR: invalid PNS name:"<<pns_id<<endl;
        return false;
    }
    return true;
}

