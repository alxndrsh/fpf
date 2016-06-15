/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
ini.h -  ini and configuration related utilities
History:
    created: 2016-06 - A.Shumilin.
*/


#ifndef INI_H
#define INI_H

#include <vector>
#include <string>
#include <map>

using namespace std;

typedef std::map<string, string> map_of_strings;
typedef std::map<string, string>::iterator mos_iterator;
typedef std::map<string, map_of_strings> map_of_maps;
typedef std::map<string, map_of_strings>::iterator mom_iterator;

typedef map_of_strings t_ini_section;
typedef mos_iterator t_ini_section_iterator;
typedef map_of_maps t_ini;
typedef mom_iterator t_ini_iterator;

bool read_config_file(string inifile,t_ini& ini,map<string,string>* pv_substitutions=NULL);
void dump_map_of_maps(const map_of_maps& mmap,ostream& os);
void dump_map_of_strings(const map_of_strings& smap,ostream& os);
t_ini_section& get_section(t_ini& ini,string& section_name);

// some parsing utilities
void parse_to_int_vector(vector<int> *V, string& str);
void parse_to_string_vector(vector<string> *V, string& str);

void make_substitutions(string &S, map<string,string>* pv_substitutions,char marker_char);


#endif // INI_H
