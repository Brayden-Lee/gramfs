#ifndef GRAMFS_SUPER_H
#define GRAMFS_SUPER_H

#include <iostream>
#include <string>
#include <kcpolydb.h>
#include "../tools/logging.h"

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
	string get_edgekv_name();
	string get_nodekv_name();
	string get_datakv_name();
	Logging* GetLog();
	int64_t get_curr_unique_id();
	int64_t generate_unique_id();
	PolyDB edge_db;
	PolyDB node_db;
	PolyDB sf_db;

private:
	Logging* logs;
	string edge_name;
	string node_name;
	string sf_name;
	uint64_t curr_unique_id;
};

#endif