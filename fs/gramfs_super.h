#ifndef GRAMFS_SUPER_H
#define GRAMFS_SUPER_H

#include <iostream>
#include <string>
#include <kcpolydb.h>

using namespace std;
using namespace kyotocabinet;

class GramfsSuper {
public:
	GramfsSuper();
	GramfsSuper(const char *edgepath, const char*nodepath);
	GramfsSuper(const string edgepath, const string nodepath);
	virtual ~GramfsSuper();
	int Open();
	int Close();
	PolyDB get_edgekv();
	PolyDB get_nodekv();
	int64_t get_curr_unique_id() {return curr_unique_id;}
	int64_t generate_unique_id();
	
private:
	PolyDB edge_db;
	PolyDB node_db;
	string edge_name;
	string node_name;
	uint64_t curr_unique_id;
}

#endif