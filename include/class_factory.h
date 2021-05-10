/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
class_factory - instrumental functions to help instantiate dynamic class objects

History:
    created: 2015-11 - A.Shumilin.
*/
#ifndef CLASS_FACTORY_H
#define CLASS_FACTORY_H
#include <string>
#include "fpf.h"
#include "ini.h"


#define FACTORY_ADD_CLASS(needclass,cname)  if (needclass == #cname) { return (T*) new cname(); }

#define FACTORY_ADD_FRAMESOURCE(needclass,cname)  if (needclass == #cname) { return (IFrameSource*) new cname(); }
#define FACTORY_ADD_NODE(needclass,cname)  if (needclass == #cname) { return (INode*) new cname(); }
#define FACTORY_ADD_INPUTSTREAM(needclass,cname)  if (needclass == #cname) { return (IInputStream*) new cname(); }

// this number limits how many objects may be created, prevents infinite chaining loops
#define MAX_NUMBER_OF_OBJECTS   99


IFrameSource* new_frame_source(std::string& classname);
IInputStream * new_input_stream(std::string& classname);
INode* new_node(std::string& classname);
INode* new_node(t_ini& ini, string& obj_name); //helps to call new_node from object name
INode* get_next_node(t_ini& ini, string& my_name, bool* no_next_node) ;// returns next node after the object with given name

#endif // CLASS_FACTORY_H
