/*
FPF - Frame Processing Framework
See the file COPYING for copying permission.
*/
/*
ini.h -  ini and configuration related utilities
History:
    created: 2016-06 - A.Shumilin.
*/

#include <iostream>
#include <fstream>
#include <cerrno>
#include <cstring>
#include <cstdlib>

#include <vector>
using namespace std;
//
#include "ini.h"
#include "fpf.h"

void dump_map_of_strings(const map_of_strings& smap,ostream& os)
{
    for (map_of_strings::const_iterator it=smap.begin();it!=smap.end(); ++it)
	{
		os<<"\t"<< (*it).first <<" = "<< (*it).second <<endl;
	}
}

void dump_map_of_maps(const map_of_maps& mmap,ostream& os)
{
    for (map_of_maps::const_iterator it=mmap.begin();it!=mmap.end(); ++it)
	{
		os<<"["<< (*it).first <<"]"<<endl;
		dump_map_of_strings((*it).second,os);
	}
}

void parse_to_int_vector(vector<int> *V, string& str)
{  // parse comma separated list of integers to given vector
    char *dup = strdup(str.c_str());
    char* pch = strtok (dup," ,");
    while (pch != NULL)
    {
        V->push_back( atoi(pch) );
        pch = strtok (NULL, " ,.-");
    }
    free(dup);
}

void parse_to_string_vector(vector<string> *V, string& str)
{
    char *dup = strdup(str.c_str());
    char* pch = strtok (dup," ,");
    while (pch != NULL)
    {
        V->push_back(string(pch));
        pch = strtok (NULL, " ,");
    }
    free(dup);
}

void make_substitutions(string &S, map<string,string>* pv_substitutions,char marker_char)
{
    size_t marker0 =  S.find(marker_char);

    while (string::npos != marker0)
    {
         size_t marker1 =  S.find(marker_char,marker0+1);
         if (string::npos != marker1)
         {
             string key = S.substr(marker0+1,(marker1-marker0-1));
             if ((pv_substitutions != NULL) && !(*pv_substitutions)[key].empty())
             {
                    S.replace(marker0,key.length()+2,(*pv_substitutions)[key]);
             }else
             {
                 //try environment
                char* psz_env = getenv (key.c_str());
                if (psz_env != NULL)
                {
                    S.replace(marker0,key.length()+2,string(psz_env));
                }else
                { // nothing to substitute here, but leave the key string
                    S.replace(marker0,key.length()+2,key);
                }
             }
         }
         //search for remaining markers
         marker0 =  S.find(marker_char);
    }
}

bool read_config_file(string inifile,t_ini& ini,map<string,string>* pv_substitutions)
{
    ifstream f(inifile.c_str());
    if (! f.is_open())
    {
        cerr << "ERROR: Failed to open INI file [" << inifile << "]:"<< strerror(errno) <<endl;
        return false;
    }
    //
    int cln = 0;//line counter
    size_t pos;size_t rpos;
    string ln;
    string curr_section_name;
//x    map_of_strings curr_section ;//= *new map_of_strings;
    while ( getline (f,ln) )
    {
        cln++;

        if (string::npos != (pos = ln.find_first_of(";#")) ) { ln.erase(pos); }//strip everything after first ';' or '#' being a comment
        pos = ln.find_first_not_of(" \t\r\n");//get first not space
        if (pos == string::npos) { continue; }//skip empty lines
        if (ln[pos] == '[')
        {
            //finalize previous section
            curr_section_name.clear(); //??? empty();
            //open new section collection
            pos = ln.find_first_not_of(" \t\r\n",pos+1);
            if (string::npos != pos)
            {
                rpos = ln.find_last_not_of("] \t\r\n");
                if (rpos > pos)
                {
                    curr_section_name = ln.substr(pos,rpos-pos+1);
                    map_of_strings smap;
                    ini.insert(pair<string,map_of_strings>(curr_section_name,smap));
                    ini[curr_section_name][INI_COMMON_CONFIGID] = curr_section_name;
                }// else warning
            }//else -warning - empty section
        }else  //not a section line
        {

            size_t eqpos = ln.find_first_of("=");
            if (string::npos != eqpos) // P=V line
            {
                string p = ln.substr(0,eqpos);
                if(string::npos != (pos = p.find_first_not_of(" \t\r\n")) && pos > 0) {p.erase(0,pos-1);} ;
                if(string::npos != (rpos = p.find_last_not_of(" \t\r\n"))) {p.erase(rpos+1);};
                //
                pos = ln.find_first_not_of(" \t\r\n",eqpos+1);
                rpos = ln.find_last_not_of(" \t\r\n");
                if (rpos >= pos)
                {
                    string v = ln.substr(pos,rpos-pos+1);
                    make_substitutions(v,pv_substitutions,'$');
                    ini[curr_section_name].insert(pair<string,string>(p,v));
                }
            }
        }
    }
    f.close();
    //dump_map_of_maps(ini,cout);
    t_ini_section argvsection = ini[INI_COMMON_SECTION_ARGV]; //this assures ARGV section allocation
    return true;
}
