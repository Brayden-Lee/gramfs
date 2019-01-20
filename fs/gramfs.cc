#include <cstring>
#include <iostream>
#include <vector>
#include <kcpolydb.h>
#include <libgen.h>
#include "gramfs.h"
#include "gramfs_super.h"

using namespace std;
using namespace kyotocabinet;

GramfsSuper *gramfs_super = NULL;

void set_dentry_flag(struct dentry *dentry, int flag_type, int val)
{
	if (val == 0)
	{
		dentry->flags &= ~(1UL << flag_type);
	} else {
		dentry->flags |= 1UL << flag_type;
	}
}

int get_dentry_flag(struct dentry *dentry, int flag_type)
{
	if (((dentry->flags >> flag_type) & 1) == 1)
	{
		return 1;
	} else {
		return 0;
	}
}

void serialize_dentry(struct dentry *dentry, char *outBuf)
{
	int pos = 0;
	memcpy(&outBuf[pos], &(dentry->p_inode), sizeof(dentry->p_inode));
	pos += sizeof(dentry->p_inode);
	memcpy(&outBuf[pos], &(dentry->o_inode), sizeof(dentry->o_inode));
	pos += sizeof(dentry->o_inode);
	memcpy(&outBuf[pos], &(dentry->dentry_name), sizeof(char) * DENTRY_NAME_LEN);
	pos += sizeof(char) * DENTRY_NAME_LEN;
	memcpy(&outBuf[pos], &(dentry->flags), sizeof(dentry->flags));
	pos += sizeof(dentry->flags);
	memcpy(&outBuf[pos], &(dentry->mode), sizeof(dentry->mode));
	pos += sizeof(dentry->mode);
	memcpy(&outBuf[pos], &(dentry->ctime), sizeof(dentry->ctime));
	pos += sizeof(dentry->ctime);
	memcpy(&outBuf[pos], &(dentry->mtime), sizeof(dentry->mtime));
	pos += sizeof(dentry->mtime);
	memcpy(&outBuf[pos], &(dentry->atime), sizeof(dentry->atime));
	pos += sizeof(dentry->atime);
	memcpy(&outBuf[pos], &(dentry->size), sizeof(dentry->size));
	pos += sizeof(dentry->size);
	memcpy(&outBuf[pos], &(dentry->uid), sizeof(dentry->uid));
	pos += sizeof(dentry->uid);
	memcpy(&outBuf[pos], &(dentry->gid), sizeof(dentry->gid));
	pos += sizeof(dentry->gid);
	memcpy(&outBuf[pos], &(dentry->nlink), sizeof(dentry->nlink));	
}

void deserialize_dentry(char * buf, struct dentry *dentry)
{
	int pos = 0;
	memcpy(&(dentry->p_inode), &buf[pos], sizeof(dentry->p_inode));
	pos += sizeof(dentry->p_inode);
	memcpy(&(dentry->o_inode), &buf[pos], sizeof(dentry->o_inode));
	pos += sizeof(dentry->o_inode);
	memcpy(&(dentry->dentry_name), &buf[pos], sizeof(char) * DENTRY_NAME_LEN);
	pos += sizeof(char) * DENTRY_NAME_LEN;
	memcpy(&(dentry->flags), &buf[pos], sizeof(dentry->flags));
	pos += sizeof(dentry->flags);
	memcpy(&(dentry->mode), &buf[pos], sizeof(dentry->mode));
	pos += sizeof(dentry->mode);
	memcpy(&(dentry->ctime), &buf[pos], sizeof(dentry->ctime));
	pos += sizeof(dentry->ctime);
	memcpy(&(dentry->mtime), &buf[pos], sizeof(dentry->mtime));
	pos += sizeof(dentry->mtime);
	memcpy(&(dentry->atime), &buf[pos], sizeof(dentry->atime));
	pos += sizeof(dentry->atime);
	memcpy(&(dentry->size), &buf[pos], sizeof(dentry->size));
	pos += sizeof(dentry->size);
	memcpy(&(dentry->uid), &buf[pos], sizeof(dentry->uid));
	pos += sizeof(dentry->uid);
	memcpy(&(dentry->gid), &buf[pos], sizeof(dentry->gid));
	pos += sizeof(dentry->gid);
	memcpy(&(dentry->nlink), &buf[pos], sizeof(dentry->nlink));	
}

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
	char *value = (char *) calloc(1, sizeof(struct dentry));
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
			//value =
			//dentry = (struct dentry*) &(str_dentry.data());  ||||
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
	pre_str = "/" + PATH_DELIMIT + pre_str;
	plist->list_name = pre_str.data();
	plist->part_bucket = new vector<string>();
	plist->next = NULL;
	gramfs_super->get_edgekv.get(pre_str, &str_dentry) // for root dentry
	plist->part_bucket.push_back(str_dentry);
	q = plist;

	for (int i = 2; i < str_vector.size(); i++)
	{
		p = (struct part_list*)malloc(sizeof(struct part_list));
		pre_str = str_vector[i - 1] + PATH_DELIMIT + str_vector[i];
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
	string p_path;
	string cur_name;
	string p_name;
	int index = full_path.find_last_of(PATH_DELIMIT);
	if (index == 0)
	{
		p_path = PATH_DELIMIT;
		p_name = PATH_DELIMIT;
		cur_name = full_path.substr(index + 1, full_path.size());
	} else {
		p_path = full_path.substr(0, index);
		cur_name = full_path.substr(index + 1, full_path.size());
		index = p_path.find_last_of(PATH_DELIMIT);
		p_name = p_path.substr(index + 1, p_path.size());
	}
	struct dentry *dentry = NULL;
	lookup(p_path, dentry);
	if (dentry == NULL)
		return -1;    // parent not exist
	string add_key;
	string add_value;
	add_key = p_name + PATH_DELIMIT + cur_name + PATH_DELIMIT + to_string(dentry->o_inode);
	if (gramfs_super->get_edgekv().get(add_key, &add_value))
	{
		return -1;    // this dir has existed
	}
	struct dentry *add_dentry = (struct dentry *)calloc(1, sizeof(struct dentry));
	add_dentry->o_inode = gramfs_super->generate_unique_id();
	add_dentry->p_inode = dentry->o_inode;
	cur_name.copy(add_dentry->dentry_name, cur_name.size(), 0);
	*(add_dentry->dentry_name + cur_name.size()) = '\0';
	add_dentry->flags = 0;    // dir
	add_dentry->mode = S_IFDIR | 0755;
	add_dentry->ctime = time(NULL);
	add_dentry->mtime = time(NULL);
	add_dentry->atime = time(NULL);
	add_dentry->dentry_name = cur_name;
	add_dentry->nlink = 0;
	add_dentry->gid = getgid();
	add_dentry->uid = getuid();
	add_dentry->size = 0;
	char *value = (char *)calloc(1, sizeof(struct dentry));
	serialize_dentry(add_dentry, value);
	add_value = value;
	gramfs_super->get_edgekv().set(add_key, add_value);

	// add to node kv
	add_key = to_string(dentry->o_inode) + PATH_DELIMIT + dentry->dentry_name + PATH_DELIMIT + to_string(add_dentry->o_inode);
	add_value = cur_name;
	gramfs_super->get_nodekv().set(add_key, add_value);
	return 0;
}

int gramfs_rmdir(const char *path, bool rmall)
{
	string full_path = path;
	if (full_path == "/")
		return -1;    // cannot remove root
	string p_path;
	string cur_name;
	string p_name;
	int index = full_path.find_last_of(PATH_DELIMIT);
	if (index == 0)
	{
		p_path = PATH_DELIMIT;
		p_name = PATH_DELIMIT;
		cur_name = full_path.substr(index + 1, full_path.size());
	} else {
		p_path = full_path.substr(0, index);
		cur_name = full_path.substr(index + 1, full_path.size());
		index = p_path.find_last_of(PATH_DELIMIT);
		p_name = p_path.substr(index + 1, p_path.size());
	}
	struct dentry *dentry = NULL;
	lookup(full_path, dentry);
	if (dentry == NULL)
		return -1;    // path not exist
	string rm_key;
	rm_key = to_string(dentry->o_inode) + PATH_DELIMIT + dentry->dentry_name;
	vector<string> tmp_key;
	gramfs_super->get_nodekv().match_prefix(rm_key, &tmp_key, -1, NULL);
	if (tmp_key.empty())
	{
		rm_key = p_name + PATH_DELIMIT + dentry->dentry_name + PATH_DELIMIT + to_string(dentry->p_inode);
		gramfs_super->get_edgekv().remove(rm_key);
		return 0;
	} else {
		if (rmall)
		{
			rm_key = p_name + PATH_DELIMIT + dentry->dentry_name + PATH_DELIMIT + to_string(dentry->p_inode);
			gramfs_super->get_edgekv().remove(rm_key);    // no need to remove the child, because it can't be found
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
	string read_key = to_string(dentry->o_inode) + PATH_DELIMIT + dentry->dentry_name;
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
	string p_path;
	string cur_name;
	string p_name;
	int index = full_path.find_last_of(PATH_DELIMIT);
	if (index == 0)
	{
		p_path = PATH_DELIMIT;
		p_name = PATH_DELIMIT;
		cur_name = full_path.substr(index + 1, full_path.size());
	} else {
		p_path = full_path.substr(0, index);
		cur_name = full_path.substr(index + 1, full_path.size());
		index = p_path.find_last_of(PATH_DELIMIT);
		p_name = p_path.substr(index + 1, p_path.size());
	}
	struct dentry *dentry = NULL;
	lookup(p_path, dentry);
	if (dentry == NULL)
		return -1;
	string add_key;
	string add_value;
	add_key = p_name + PATH_DELIMIT + cur_name + PATH_DELIMIT + to_string(dentry->o_inode);
	if (gramfs_super->get_edgekv().get(add_key, &add_value))
	{
		return -1;    // this dir has existed
	}
	struct dentry *add_dentry = (struct dentry *)calloc(1, sizeof(struct dentry));
	add_dentry->o_inode = gramfs_super->generate_unique_id();
	add_dentry->p_inode = dentry->o_inode;
	cur_name.copy(add_dentry->dentry_name, cur_name.size(), 0);
	*(add_dentry->dentry_name + cur_name.size()) = '\0';
	add_dentry->flags = 1;    // file
	add_dentry->mode = S_IFDIR | 0755;
	add_dentry->ctime = time(NULL);
	add_dentry->mtime = time(NULL);
	add_dentry->atime = time(NULL);
	add_dentry->dentry_name = cur_name;
	add_dentry->nlink = 0;
	add_dentry->gid = getgid();
	add_dentry->uid = getuid();
	add_dentry->size = 0;
	char *value = (char *)calloc(1, sizeof(struct dentry));
	serialize_dentry(add_dentry, value);
	add_value = value;
	gramfs_super->get_edgekv().set(add_key, add_value);

	// add to node kv
	add_key = to_string(dentry->o_inode) + PATH_DELIMIT + dentry->dentry_name + PATH_DELIMIT + to_string(add_dentry->o_inode);
	add_value = cur_name;
	gramfs_super->get_nodekv().set(add_key, add_value);
	return 0;	
}

int gramfs_unlink(const char *path)
{
	string full_path = path;
	string p_path;
	string cur_name;
	string p_name;
	int index = full_path.find_last_of(PATH_DELIMIT);
	if (index == 0)
	{
		p_path = PATH_DELIMIT;
		p_name = PATH_DELIMIT;
		cur_name = full_path.substr(index + 1, full_path.size());
	} else {
		p_path = full_path.substr(0, index);
		cur_name = full_path.substr(index + 1, full_path.size());
		index = p_path.find_last_of(PATH_DELIMIT);
		p_name = p_path.substr(index + 1, p_path.size());
	}
	struct dentry *dentry = NULL;
	lookup(full_path, dentry);
	if (dentry == NULL)
		return -1;
	string rm_key;
	rm_key = p_name + PATH_DELIMIT + cur_name + PATH_DELIMIT + to_string(dentry->p_inode);
	gramfs_super->get_edgekv().remove(rm_key);
	rm_key = to_string(dentry->p_inode) + PATH_DELIMIT + p_name + PATH_DELIMIT + to_string(dentry->o_inode);
	gramfs_super->get_nodekv().remove(rm_key);
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
