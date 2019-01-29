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
	GramfsSuper(const char *edgepath, const char*nodepath, const char *sfpath);
	GramfsSuper(const string edgepath, const string nodepath, const string sfpath);
	virtual ~GramfsSuper();
	int Open();
	int Close();
	PolyDB get_edgekv();
	PolyDB get_nodekv();
	PolyDB get_datakv();
	int64_t get_curr_unique_id();
	int64_t generate_unique_id();
	
private:
	PolyDB edge_db;
	PolyDB node_db;
	PolyDB sf_db;
	string edge_name;
	string node_name;
	string sf_name;
	uint64_t curr_unique_id;
}

#endif