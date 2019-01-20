#include <string>
#include <iostream>
#include <vector>
#include <kcpolydb.h>
#include <libgen.h>
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

struct dentry* dentry_match(struct part_list *plist)
{
	struct dentry* dentry = (struct dentry*)malloc(sizeof(struct dentry));
	struct part_list *p;
	string str_dentry = NULL;
	int curr_id = 0;    // root inode
	bool triggle = false;
	if (plist == NULL)
	{
		delete dentry;
		dentry = NULL;
		return dentry;
	}
	while (plist != NULL)
	{
		for (int i = 0; i < plist->part_bucket.size(); i++)
		{
			str_dentry = plist->part_bucket.front();
			plist->part_bucket.pop_back();
			dentry = (struct dentry*) &(str_dentry.data());
			if (dentry->p_inode == curr_id)
			{
				plist->part_bucket.clear();
				curr_id = dentry->o_inode;
				triggle = true;    // if the path exist, must be triggle, or not eixst
				break;
			}
		}
		if (!triggle)
		{
			delete dentry;
			dentry = NULL;
			while (plist != NULL)
			{
				p = plist;
				plist = plist->next;
				delete p;
				p = NULL;
			}
			return dentry;
		}
		triggle = false;
		p = plist;
		plist = plist->next;
		delete p;
		p = NULL;
	}
	return dentry;
}

// full path lookup (serial, need to change to parallel)
int lookup(const char *path, struct dentry *dentry)
{
	vector<string> str_vector;
	vector<string> tmp_key;
	string pre_str;
	string str_dentry;

	split_path(path, "/", str_vector);

	struct part_list * plist = (struct part_list*)malloc(sizeof(struct part_list));
	struct part_list *p = NULL;
	struct part_list *q = NULL;
	pre_str = str_vector[1];
	pre_str = "/" + pre_str;
	plist->list_name = pre_str.data();
	plist->part_bucket = new vector<string>();
	plist->next = NULL;
	gramfs_super->get_edgekv.get(pre_str, &str_dentry) // for root dentry
	plist->part_bucket.push_back(str_dentry);
	q = plist;

	for (int i = 2; i < str_vector.size(); i++)
	{
		p = (struct part_list*)malloc(sizeof(struct part_list));
		pre_str = str_vector[i - 1] + str_vector[i];
		p->list_name = pre_str.data();
		p->part_bucket = new vector<string>();
		p->next = NULL;

		gramfs_super->get_edgekv().match_prefix(pre_str, &tmp_key, -1, NULL);  // get all prefix key without inode
		for(int j = 0; j < tmp_key.size(); j++)
		{
			gramfs_super->get_edgekv().get(tmp_key[j], &pre_str);
			p->part_bucket.push_back(pre_str);  // save all value for this key
		}
		tmp_key.clear();
		q->next = p;
		q = q->next;
		p = NULL;
	}
	dentry = dentry_match(plist);    // allocate value and find value
	if (dentry == NULL)
		return -1;
	else
		return 0;
}

// user api
int gramfs_init(const char *edgepath, const char *nodepath)
{
	gramfs_super = new GramfsSuper(edgepath, nodepath);
	return gramfs_super->Open();
}

void gramfs_destroy(const char *edgepath, const char *nodepath)
{
	gramfs_super->Close();
	delete gramfs_super;
}

int gramfs_mkdir(const char *path, mode_t mode)
{
	string full_path = path;
	int index = full_path.find_last_of(PATH_DELIMIT);
	string p_path = full_path.substr(0, index);
	struct dentry *dentry = NULL;
	lookup(p_path, dentry);
	if (dentry == NULL)
		return -1;
	string child_key;
	string cur_name;
	vector<string> tmp_key;
	cur_name = basename(full_path);
	child_key = cur_name + to_string(dentry->o_inode);
	gramfs_super->get_edgekv().match_prefix(pre_str, &tmp_key, -1, NULL);
	if (!tmp_key.empty())
	{
		return -1; // this path exist;
	}
	string add_key;
	string add_value;
	string p_name;
	p_name = basename(p_path);
	add_key = p_name + child_key;
	struct dentry *add_dentry = (struct dentry *)calloc(1, sizeof(struct dentry));
	strcpy(add_dentry, dentry, sizeof(struct dentry);
	add_dentry->o_inode = gramfs_super->generate_unique_id();
	add_dentry->p_inode = dentry->o_inode;
	add_dentry->dentry_name = cur_name;
	add_value = (string) add_dentry;
	gramfs_super->get_edgekv().set(add_key, add_value);
	add_key = to_string(dentry->o_inode) + dentry->dentry_name + to_string(add_dentry->o_inode);
	add_value = cur_name;
	gramfs_super->get_nodekv().set(add_key, add_value);
	return 0;
}

int gramfs_rmdir(const char *path, bool rmall)
{
	string full_path = path;
	struct dentry *dentry = NULL;
	lookup(full_path, dentry);
	if (dentry == NULL)
		return -1;
	string read_key = to_string(dentry->p_inode) + dentry->dentry_name;
	vector<string> tmp_key;
	gramfs_super->get_nodekv().match_prefix(read_key, &tmp_key, -1, NULL);
	if (tmp_key.empty())
	{
		string p_name = basename(dirname(full_path));
		read_key = p_name + dentry->dentry_name + to_string(dentry->p_inode);
		gramfs_super->get_edgekv().remove(read_key);
		return 0;
	} else {
		if (rmall)
		{
			string p_name = basename(dirname(full_path));
			read_key = p_name + dentry->dentry_name + to_string(dentry->p_inode);
			gramfs_super->get_edgekv().remove(read_key);    // no need to remove the child, because it can't be found
		} else {
			return -1;    // not an empty dir without -r
		}
	}
}

int gramfs_readdir(const char *path)
{
	string full_path = path;
	struct dentry *dentry = NULL;
	lookup(full_path, dentry);
	if (dentry == NULL)
		return -1;
	string read_key = to_string(dentry->p_inode) + dentry->dentry_name;
	vector<string> tmp_key;
	gramfs_super->get_nodekv().match_prefix(read_key, &tmp_key, -1, NULL);
	for (int i = 0; i < tmp_key.size(); i++)
	{
		cout<< "child "<< i << " is :" <<tmp_key[i]<<endl;
	}
	return 0;
}

int gramfs_opendir(const char *path)
{
	return gramfs_super->Open();
}

int gramfs_releasedir(const char *path)
{
	return gramfs_super->Close();
}

int gramfs_getattr(const char *path, struct stat *st)
{
	string full_path = path;
	struct dentry *dentry  = NULL;
	lookup(full_path, dentry);
	if (dentry == NULL)
		return -1;
	st->mtime = dentry->ctime;
	st->ctime = dentry->ctime;
	st->gid = dentry->gid;
	st->uid = dentry->uid;
	st->nlink = dentry->nlink;
	st->size = dentry->size;
	st->mode = dentry->mode;
}

int gramfs_rename(const char *path, const char *newpath)
{

}

int gramfs_create(const char *path, mote_t mode)
{
	string full_path = path;
	int index = full_path.find_last_of(PATH_DELIMIT);
	string p_path = full_path.substr(0, index);
	struct dentry *dentry = NULL;
	lookup(p_path, dentry);
	if (dentry == NULL)
		return -1;
	string add_key;
	string cur_name = full_path.substr(index + 1, full_path.size());
	string p_name = p_path.substr(p_path.find_last_of(PATH_DELIMIT) + 1, p_path.size());
	add_key = p_name + cur_name + dentry->o_inode;
	string tmp_key;
	int check_res = gramfs_super->get_edgekv().check(add_key);
	if (check_res > 0)
		return -1;    // exist
	string add_value;
	struct dentry *add_dentry = (struct dentry *)calloc(1, sizeof(struct dentry));
	strcpy(add_dentry, dentry, sizeof(struct dentry));
	add_dentry->o_inode = gramfs_super->generate_unique_id();
	add_dentry->p_inode = dentry->o_inode;
	add_dentry->dentry_name = cur_name;
	add_value = (string) add_dentry;
	gramfs_super->get_edgekv().set(add_key, add_value);
	add_key = to_string(dentry->o_inode) + dentry->dentry_name + to_string(add_dentry->o_inode);
	add_value = cur_name;
	gramfs_super->get_nodekv().set(add_key, add_value);
	return 0;
	
	
}

int gramfs_unlink(const char *path)
{
	string full_path = path;
	struct dentry *dentry = NULL;
	lookup(full_path, dentry);
	if (dentry == NULL)
		return -1;
	
	int index = full_path.find_last_of(PATH_DELIMIT);
	string p_path = full_path.substr(0, index);
	string cur_name = full_path.substr(index + 1, full_path.size());
	index = p_path.find_last_of(PATH_DELIMIT);
	string p_name = p_path.substr(index + 1, p_path.size());
	string rm_key = p_name + cu
	
}

int gramfs_open(const char *path, mode_t mode)
{
	return gramfs_super->Open();
}

int gramfs_release(const char *path)
{
	return gramfs_super->Close();
}

int gramfs_read(const char *path, char *buf, size_t size, off_t offset)
{

}

int gramfs_write(const char *path, const char *buf, size_t size, off_t offset)
{

}
