#include "gramfs_super.h"

GramfsSuper::~GramfsSuper()
{
	cout<<"GramfsSuper has been destroy!"<<endl;
}

GramfsSuper::GramfsSuper() : edge_name("/tmp/edge.kch"), node_name("/tmp/node.kct"), sf_name("/tmp/data.kct")
{
	curr_unique_id = 0;
	maxedgesize = 16384;
	maxnodesize = 16384 * 2;
	logs = new Logging();
	logs->Open();
}

GramfsSuper::GramfsSuper(const char *edgepath, const char*nodepath, const char *sfpath) : edge_name(edgepath), node_name(nodepath), sf_name(sfpath)
{
	curr_unique_id = 0;
	maxedgesize = 16384;
	maxnodesize = 16384 * 2;
	logs = new Logging();
	logs->Open();
}

GramfsSuper::GramfsSuper(const string edgepath, const string nodepath, const string sfpath) : edge_name(edgepath), node_name(nodepath), sf_name(sfpath)
{
	curr_unique_id = 0;
	maxedgesize = 16384;
	maxnodesize = 16384 * 2;
	logs = new Logging();
	logs->Open();
}

int GramfsSuper::Open()
{
	if (!edge_db.open(edge_name, PolyDB::OWRITER | PolyDB::OCREATE))
	{
		cerr<<"open edge kv error : "<< edge_db.error().name()<<endl;
		return -1;
	}
	if (!node_db.open(node_name, PolyDB::OWRITER | PolyDB::OCREATE))
	{
		cerr<<"open node kv error : "<< node_db.error().name()<<endl;
		return -1;
	}
	if (!sf_db.open(sf_name, PolyDB::OWRITER | PolyDB::OCREATE))
	{
		cerr<<"open small file kv error : "<< sf_db.error().name()<<endl;
		return -1;
	}
	return 0;
}

int GramfsSuper::Close()
{
	if (!edge_db.close())
	{
	  cerr << "close edge kv error: " << edge_db.error().name() << endl;
	  return -1;
	}
	if (!node_db.close())
	{
		cerr << "close node kv error: " << node_db.error().name() << endl;
		return -1;
	}
	if (!sf_db.close())
	{
	  cerr << "close small file kv error: " << sf_db.error().name() << endl;
	  return -1;
	}
	return 0;
}

string GramfsSuper::get_edgekv_name()
{
	return edge_name;
}

string GramfsSuper::get_nodekv_name()
{
	return node_name;
}

string GramfsSuper::get_datakv_name()
{
	return sf_name;
}

Logging* GramfsSuper::GetLog()
{
	return logs;
}

int64_t GramfsSuper::get_curr_unique_id()
{
	return curr_unique_id;
}

int64_t GramfsSuper::generate_unique_id()
{
	curr_unique_id++;
	return curr_unique_id;
}

bool GramfsSuper::FindEdge(string& key, string& value)
{
	unordered_map<string, string>::iterator it = edge_map.find(key);
	if (it == edge_map.end()) {
		return false;
	} else {
		value = it->second;
		return true;
	}
}

void GramfsSuper::InsertEdge(string& key, string& value)
{
	edge_map[key] = value;
	if (edge_map.size() > maxedgesize) {
		edge_map.erase(key);
		return;
	}
}

void GramfsSuper::EvictEdge(string& key)
{
	unordered_map<string, string>::iterator it = edge_map.find(key);
	if (it != edge_map.end()) {
		edge_map.erase(it);
	}
}

bool GramfsSuper::FindNode(string& key, string& value)
{
	unordered_map<string, string>::iterator it = node_map.find(key);
	if (it == node_map.end()) {
		return false;
	} else {
		value = it->second;
		return true;
	}
}

void GramfsSuper::InsertNode(string& key, string& value)
{
	node_map[key] = value;
	if (node_map.size() > maxnodesize) {
		node_map.erase(key);
		return;
	}
}

void GramfsSuper::EvictNode(string& key)
{
	unordered_map<string, string>::iterator it = node_map.find(key);
	if (it != node_map.end()) {
		node_map.erase(it);
	}
}