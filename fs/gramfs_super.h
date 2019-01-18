#ifndef GRAMFS_SUPER_H
#define GRAMFS_SUPER_H

#include <iostream>
#include <string>
#include <kcpolydb.h>

using namespace std;
using namespace kyotocabinet;

class GramfsSuper {
public:
	GramfsSuper() : edge_name("/tmp/tmp.kch"), node_name("/tmp/tmp.kct") {}
	GramfsSuper(const char *edgepath, const char*nodepath) : edge_name(edgepath), node_name(nodepath) {}
	GramfsSuper(const string edgepath, const string nodepath) : edge_name(edgepath), node_name(nodepath) {}
	virtual ~GramfsSuper();
	int Open();
	int Close();
	PolyDB get_edgekv();
	PolyDB get_nodekv();
private:
	PolyDB edge_db;
	PolyDB node_db;
	string edge_name;
	string node_name
}

#endif