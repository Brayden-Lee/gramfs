#include <string>
#include <iostream>
#include <vector>
#include <kcpolydb.h>
#include "gramfs.h"
#include "gramfs_super.h"

using namespace std;
using namespace kyotocabinet;

GramfsSuper *gramfs_super = NULL;

void split_path(const string& src, const string& separator, vector<string>& dest)
{
	string str = src;
	string substring;
	string::size_type start = 0, index;
	dest.clear();
	index = str.find_first_of(separator, start);
	do
	{
		if(index != string::npos)
		{
			substring = str.substr(start, index - start);
			dest.push_back(substring);
			start = index + separator.size();
			index = str.find(separator, start);
			if (start == string::npos) break;
		}
	} while(index != string::npos);

	// the last part
	substring = str.substr(start);
	dest.push_back(substring);
}

struct dentry* dentry_match(vector<string> all_dentry)
{
	struct dentry* dentry = (struct dentry*)malloc(sizeof(struct dentry));
	for(int i = 0; i < all_dentry.size(); i++)
	{
		dentry = (struct dentry)all_dentry[i];


	}
	return dentry;
}

// user api
int fs_init(const char *edgepath, const char *nodepath)
{
	gramfs_super = new GramfsSuper(edgepath, nodepath);
	return gramfs_super->Open();
}

void fs_destroy(const char *edgepath, const char *nodepath)
{
	gramfs_super->Close();
	delete gramfs_super;
}

int fs_lookup(const char *path, struct dentry *dentry)
{
	vector<string> str_vector;
	vector<string> all_key;
	vector<string> tmp_key;
	string pre_str;
	string str_dentry;
	split_path(path, "/", str_vector);
	struct part_list * plist = (struct part_list*)malloc(sizeof(struct part_list));
	struct part_list *p = NULL;
	struct part_list *q = NULL;

	pre_str = "/" + pre_str;
	plist->list_name = pre_str.data();
	plist->part_bucket = new vector<string>();
	plist->next = NULL;
	gramfs_super->get_edgekv.get(pre_str, &str_dentry)
	plist->part_bucket.push_back(str_dentry);
	q = plist;

	for (int i = 2; i < str_vector.size(); i++)
	{
		p = (struct part_list*)malloc(sizeof(struct part_list));
		pre_str = str_vector[i - 1] + str_vector[i];
		p->list_name = pre_str.data();
		p->part_bucket = new vector<string>();
		p->next = NULL;

		gramfs_super->get_edgekv().match_prefix(pre_str, &tmp_dentry, -1, NULL);
		for(int j = 0; j < tmp_dentry.size(); j++)
		{
			gramfs_super->get_edgekv().get(tmp_dentry[j], &pre_str);
			p->part_bucket.push_back(pre_str);
		}
		tmp_dentry.clear();
		q->next = p;
		p = NULL;
	}
	dentry = dentry_match(plist);
}

int fs_mkdir(const char *path, mode_t mode)
{

}

int fs_rmdir(const char *path, bool rmall)
{

}

int fs_readdir(const char *path)
{

}

int fs_releasedir(const char *path)
{

}

int fs_getattr(const char *path, struct stat *st)
{

}

int fs_rename(const char *path, const char *newpath)
{

}

int fs_open(const char *path, mode_t mode)
{

}

int fs_release(const char *path)
{

}

int fs_read(const char *path, char *buf, size_t size, off_t offset)
{

}

int fs_write(const char *path, const char *buf, size_t size, off_t offset)
{

}
