/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
class_factory - instrumental functions to help instantiate dynamic class objects

History:
    created: 2015-11 - A.Shumilin.
*/


#include "class_factory.h"
#include "fpf.h"


/// INCLUDE ALL CLASS HEADERS HERE
// frame sources
#include "CFrameSource_Simulator.h"
#include "CFrameSource_CADU.h"
#include "CFrameSource_PDS.h"


//Nodes
#include "CNode_Counter.h"
#include "CNode_Descrambler.h"
#include "CNode_CADUdump.h"
#include "CNode_PacketExtractor.h"
#include "CNode_FileWriter.h"
#include "CNode_EOSPdump.h"
#include "CNode_RS.h"
#include "CNode_EOSinv.h"
#include "CNode_Splitter.h"
#include "CNode_SCTrigger.h"
#include "CNode_TFstat.h"
// inputs
#include "CInputStream_File.h"
#include "CInputStream_Socket.h"


//
// --------------------------------------------------------
int c_objects_number = 0;

IFrameSource* new_frame_source(std::string& classname)
{
        if (classname.empty())
        {
            *fpf_error << "ERROR: new_frame_source(): invalid Frame sorce class name [" << classname <<"]\n";
            return NULL;
        }

        /////  - add new classes here --------------
        FACTORY_CFRAMESOURCE_CADU(classname);
        FACTORY_CFRAMESOURCE_SIMULATOR(classname);
        FACTORY_CFRAMECOURCE_PDS(classname);

        //protect against infinite loop chaining
        if (++c_objects_number > MAX_NUMBER_OF_OBJECTS)
        {
            *fpf_error<<"ERROR: too many objects created, looks like you have a object reference loop.\n";
            throw std::runtime_error(string("new_node() - MAX_NUMBER_OF_OBJECTS reached"));
        }
        // invalid class requested if reached this point
        *fpf_error << "ERROR: invalid Frame sorce class name [" << classname <<"]\n";
        return NULL;
}

INode* new_node(std::string& classname)
{
        if (classname.empty())
        {
            *fpf_warn << "!! new_node(): call with an empty classname\n";
            return NULL;
        }
        //// ----- add new node classes here-------------
        FACTORY_CNODE_COUNTER(classname);
        FACTORY_CNODE_DESCRAMBLER(classname);
        FACTORY_CNODE_CADUDUMP(classname);
        FACTORY_CNODE_PACKETEXTRACTOR(classname);
        FACTORY_CNODE_FILEWRITER(classname);
        FACTORY_CNODE_EOSPDUMP(classname);
        FACTORY_CNODE_RS(classname);
        FACTORY_CNODE_EOSINV(classname);
        FACTORY_CNODE_SPLITTER(classname);
        FACTORY_CNODE_SCTRIGGER(classname);
        FACTORY_CNODE_TFSTAT(classname);
        //protect against infinite loop chaining
        if (++c_objects_number > MAX_NUMBER_OF_OBJECTS)
        {
            *fpf_error<<"ERROR: too many objects created, looks like you have a object reference loop.\n";
            throw std::runtime_error(string("new_node() - MAX_NUMBER_OF_OBJECTS reached"));
        }

        // invalid class requested if reached this point
        *fpf_error << "ERROR: invalid Node class name [" << classname <<"]\n";
        return NULL;
}

INode* new_node(t_ini& ini, string& obj_name)
{
        string classname = ini[obj_name][INI_COMMON_CLASS];

        if (classname.empty())
        {
           *fpf_error << "ERROR: new_node() failed to init from section conf\n";
           return NULL;
        }
        return new_node(classname);
}




INode* get_next_node(t_ini& ini, string& my_name, bool* no_next_node)
{

     //get the prev object section
    if (ini.find(my_name) == ini.end())
    {
        *fpf_error << "ERROR: get_next_node() failes to find ini section ["<<my_name<<"]\n";
        return NULL;
    }
    string next_node_name = ini[my_name][INI_COMMON_NEXT_NODE];
    if (next_node_name.empty())
    { //this is not an error
      // *fpf_trace << "- get_next_node() no next node after ["<<my_name<<"]. The chain terminates here\n";
		*no_next_node = true;
        return NULL;
    }
    if (ini.find(next_node_name) == ini.end())
    {
        *fpf_error << "ERROR: get_next_node() no ini section with name ["<<next_node_name<<"]\n";
        return NULL;
    }
//x    t_ini_section section = ini[next_node_name];
    if (ini[next_node_name].find(INI_COMMON_CLASS) == ini[next_node_name].end())
    {
        *fpf_error << "ERROR: get_next_node() no 'class' tag in ini section ["<<next_node_name<<"]\n";
        return NULL;
    }
    string classname = ini[next_node_name][INI_COMMON_CLASS];
    INode* pnext_node = new_node(classname);
    if (pnext_node == NULL)
    {
       *fpf_error << "ERROR: get_next_node("<<my_name<<") failed to create next node ["<<classname<<"::"<< next_node_name <<"]\n";
       return NULL;
    }
    //ok, we have got the next node
    //*fpf_trace<<"- get_next_node() attaching new node "<<classname<<"::"<< next_node_name <<" after \""<<my_name<<"\"\n";
    return pnext_node;


}

IInputStream* new_input_stream(std::string& classname)
{
        if (classname.empty())
        {
            *fpf_error << "ERROR: new_frame_source(): invalid Frame source class name [" << classname <<"]\n";
            return NULL;
        }

        //// ----- add new node classes here-------------
        FACTORY_CINPUTSTREAM_FILE(classname)
        FACTORY_CINPUTSTREAM_SOCKET(classname)

        // invalid class requested if reached this point
        *fpf_error << "ERROR: invalid InputStream class name [" << classname <<"]\n";
        return NULL;
}



