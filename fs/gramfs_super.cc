#include "gramfs_super.h"

GramfsSuper::~GramfsSuper()
{
	cout<<"GramfsSuper has been destroy!"<<endl;
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
	return 0;
}

int GramfsSuper::Close()
{
	if (!edge_db.close())
	{
	  cerr << "close error: " << edge_db.error().name() << endl;
	  return -1;
	}
	if (!node_db.close())
	{
		cerr << "close error: " << node_db.error().name() << endl;
		return -1;
	}
	return 0;
}

PolyDB GramfsSuper::get_edgekv()
{
	return edge_db;
}

PolyDB GramfsSuper::get_nodekv()
{
	return node_db;
}