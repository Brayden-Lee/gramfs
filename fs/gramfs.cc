#include <iostream>
#include <stdlib.h>
#include <cstring>
#include <kcpolydb.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <sstream>
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

// for /a/b/c/d/e ==> ' ','a','b','c','d','e'
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

string serialize_dentry(struct dentry *dentry)
{
	string str = "";
	str += to_string(dentry->p_inode) + KC_VALUE_DELIMIT;
	str += to_string(dentry->o_inode) + KC_VALUE_DELIMIT;
	str += dentry->dentry_name + string(KC_VALUE_DELIMIT);
	str += to_string(dentry->flags) + KC_VALUE_DELIMIT;
	str += to_string(dentry->mode) + KC_VALUE_DELIMIT;
	str += to_string(dentry->ctime) + KC_VALUE_DELIMIT;
	str += to_string(dentry->mtime) + KC_VALUE_DELIMIT;
	str += to_string(dentry->atime) + KC_VALUE_DELIMIT;
	str += to_string(dentry->size) + KC_VALUE_DELIMIT;
	str += to_string(dentry->uid) + KC_VALUE_DELIMIT;
	str += to_string(dentry->gid) + KC_VALUE_DELIMIT;
	str += to_string(dentry->nlink);
	return str;
}

void deserialize_dentry(const string str, struct dentry *dentry)
{
	vector<string> str_vector;
	split_path(str, KC_VALUE_DELIMIT, str_vector);
	dentry->p_inode = stoull(str_vector[0].c_str());
	dentry->o_inode = stoull(str_vector[1].c_str());
	strcpy(dentry->dentry_name, str_vector[2].data());
	dentry->flags = stoul(str_vector[3].c_str());
	dentry->mode = stoul(str_vector[4].c_str());
	dentry->ctime = stoul(str_vector[5].c_str());
	dentry->mtime = stoul(str_vector[6].c_str());
	dentry->atime = stoul(str_vector[7].c_str());
	dentry->size = stoul(str_vector[8].c_str());
	dentry->uid = stoul(str_vector[9].c_str());
	dentry->gid = stoul(str_vector[10].c_str());
	dentry->nlink = stoul(str_vector[11].c_str());
}

int dentry_match(struct part_list *plist, struct dentry *dentry)
{
	struct part_list *p = NULL;
	string str_dentry;
	uint64_t curr_p_id = 0;    // root inode
	bool triggle = false;
	uint32_t vec_size = 0;
	if (plist == NULL)
	{
		printf("dentry_match, this is error case\n");
		return -1;
	}
	while (plist != NULL)
	{
	#ifdef GRAMFS_DEBUG
		printf("dentry_match, these plist = %s, and bucket total = %d\n", plist->list_name, (int)plist->part_bucket->size());
	#endif
		vec_size = (uint32_t) plist->part_bucket->size();
		for (uint32_t i = 0; i < vec_size; i++)
		{
			str_dentry = plist->part_bucket->back();
			plist->part_bucket->pop_back();
			deserialize_dentry(str_dentry, dentry);
			if (dentry->p_inode == curr_p_id)
			{
				plist->part_bucket->clear();
				curr_p_id = dentry->o_inode;
				triggle = true;    // if the path exist, must be triggle, or not eixst
				break;
			}
		}
		if (!triggle)
		{
			while (plist != NULL)
			{
				p = plist;
				plist = plist->next;
				delete p;
				p = NULL;
			}
		#ifdef GRAMFS_DEBUG
			printf("dentry_match fail\n");
		#endif
			return -1;
		}
		triggle = false;
		p = plist;
		plist = plist->next;
		delete p->part_bucket;
		free(p);
		p = NULL;
	}
#ifdef GRAMFS_DEBUG
	printf("dentry_match succ, found dentry name = %s\n", dentry->dentry_name);
#endif
	return 0;
}

// full path lookup (serial, need to change to parallel)
int lookup(const char *path, struct dentry *dentry)
{
	vector<string> str_vector;
	vector<string> tmp_key;
	string pre_str;
	string str_dentry;

	int ret = 0;
	if (strcmp(path, PATH_DELIMIT) == 0)
	{ // for lookup "/"
		string root_key;
		string root_value;
		root_key = "//0";
		ret = gramfs_super->edge_db.get(root_key, &root_value);
		printf("look up (root) ret = %d, get value = %s\n", ret, root_value.data());
		if (ret == 0)
		{
			ret = -ENOROOT;
			return ret;
		} else {
			ret = 0;
		}
		deserialize_dentry(root_value, dentry);
	#ifdef GRAMFS_DEBUG
		printf("this is root dentry, get ret = %d\n", ret);
	#endif
		return ret;
	}
	split_path(path, "/", str_vector);

	if (str_vector.size() == 2) // just the subdir of root
	{
		pre_str = str_vector[1];
		pre_str = string("/") + PATH_DELIMIT + pre_str + PATH_DELIMIT + to_string(0); // for second, p_inode is 0;
		ret = gramfs_super->edge_db.get(pre_str, &str_dentry); // for root dentry
		if (ret == 0) // get false
		{
			ret = -ENOENT;
			return ret;
		} else {
			ret = 0;
		}
	#ifdef GRAMFS_DEBUG
		gramfs_super->GetLog()->LogMsg("lookup : %s, dentry : %s, ret = %d\n", pre_str.data(), str_dentry.data(), ret);
	#endif
		deserialize_dentry(str_dentry, dentry);
		return ret;
	}

	// sub dentry more than 2 (" ", xx which means "/xx" path)
	struct part_list * plist = (struct part_list*)calloc(1, sizeof(struct part_list));
	struct part_list *p = NULL;
	struct part_list *q = NULL;
	pre_str = str_vector[1];
	pre_str = string("/") + PATH_DELIMIT + pre_str + PATH_DELIMIT + to_string(0); // for second, p_inode is 0;
	//plist->list_name = (char *)pre_str.data();
	strcpy(plist->list_name, pre_str.data());
#ifdef GRAMFS_DEBUG
	printf("lookup the first pre_str = %s, plist name = %s\n", pre_str.data(), plist->list_name);
#endif
	plist->part_bucket = new vector<string>();
	plist->next = NULL;
	ret = gramfs_super->edge_db.get(pre_str, &str_dentry); // for root dentry
	if (ret == 0)
	{
		ret = -ENOENT;
		return ret;
	} else {
		ret = 0;
	}
	
	plist->part_bucket->push_back(str_dentry);
	q = plist;

	int64_t search_count = 0;
	for (uint32_t i = 2; i < str_vector.size(); i++)
	{
		string new_pre_str;
		p = (struct part_list*)calloc(1, sizeof(struct part_list));
		new_pre_str = str_vector[i - 1] + PATH_DELIMIT + str_vector[i] + PATH_DELIMIT;    // the last "/" avoid the prefix
		strcpy(p->list_name, new_pre_str.data());
	#ifdef GRAMFS_DEBUG
		printf("lookup the %d th, pre_str = %s, plist name = %s\n", i, new_pre_str.data(), p->list_name);
	#endif
		p->part_bucket = new vector<string>();
		p->next = NULL;

		search_count = gramfs_super->edge_db.match_prefix(new_pre_str, &tmp_key, -1, NULL);  // get all prefix key without inode
		if (search_count <= 0)    // error case
		{
		#ifdef GRAMFS_DEBUG
			printf("lookup, pre_str = %s not in edge kv\n", new_pre_str.data());
		#endif
			ret = -ENOENT;
			return ret;
		}
	#ifdef GRAMFS_DEBUG
		gramfs_super->GetLog()->LogMsg("lookup, %d th sub-dentry, pre-key = %s, total  = %d\n", i, new_pre_str.data(), tmp_key.size());
	#endif
		for(uint32_t j = 0; j < tmp_key.size(); j++)
		{
			string tmp_value;
			ret = gramfs_super->edge_db.get(tmp_key[j], &tmp_value);
			if (ret == 0)    // error case
			{
			#ifdef GRAMFS_DEBUG
				printf("lookup, error case : find key = %s but not find the value\n", tmp_key[j].data());
			#endif
				ret = -EIO;
				return ret;
			}
			p->part_bucket->push_back(tmp_value);  // save all value for this key
		#ifdef GRAMFS_DEBUG
			gramfs_super->GetLog()->LogMsg("lookup : %s, dentry : %s\n", tmp_key[j].data(), tmp_value.data());
		#endif
		}
		tmp_key.clear();
		q->next = p;
		q = q->next;
		p = NULL;
	}
#ifdef GRAMFS_DEBUG
	gramfs_super->GetLog()->LogMsg("lookup, find all sub-dentry, begin matching...\n");
#endif
	ret = dentry_match(plist, dentry);    // allocate value and find value
	if (ret == -1)
		return -ENOENT;
	else
		return 0;
}


int lookup_multi_thread(const char *path, struct dentry *dentry)
{
	vector<string> str_vector;
	vector<string> tmp_key;
	string pre_str;
	string str_dentry;

	int ret = 0;
	if (strcmp(path, PATH_DELIMIT) == 0)
	{ // for lookup "/"
		string root_key;
		string root_value;
		root_key = "//0";
		ret = gramfs_super->edge_db.get(root_key, &root_value);
		printf("look up (root) ret = %d, get value = %s\n", ret, root_value.data());
		if (ret == 0)
		{
			ret = -ENOROOT;
			return ret;
		} else {
			ret = 0;
		}
		deserialize_dentry(root_value, dentry);
	#ifdef GRAMFS_DEBUG
		printf("this is root dentry, get ret = %d\n", ret);
	#endif
		return ret;
	}
	split_path(path, "/", str_vector);

	if (str_vector.size() == 2) // just the subdir of root
	{
		pre_str = str_vector[1];
		pre_str = string("/") + PATH_DELIMIT + pre_str + PATH_DELIMIT + to_string(0); // for second, p_inode is 0;
		ret = gramfs_super->edge_db.get(pre_str, &str_dentry); // for root dentry
		if (ret == 0) // get false
		{
			ret = -ENOENT;
			return ret;
		} else {
			ret = 0;
		}
	#ifdef GRAMFS_DEBUG
		gramfs_super->GetLog()->LogMsg("lookup : %s, dentry : %s, ret = %d\n", pre_str.data(), str_dentry.data(), ret);
	#endif
		deserialize_dentry(str_dentry, dentry);
		return ret;
	}

	// sub dentry more than 2 (" ", xx which means "/xx" path)
	struct part_list * plist = (struct part_list*)calloc(1, sizeof(struct part_list));
	struct part_list *p = NULL;
	struct part_list *q = NULL;
	pre_str = str_vector[1];
	pre_str = string("/") + PATH_DELIMIT + pre_str + PATH_DELIMIT + to_string(0); // for second, p_inode is 0;
	//plist->list_name = (char *)pre_str.data();
	strcpy(plist->list_name, pre_str.data());
#ifdef GRAMFS_DEBUG
	printf("lookup the first pre_str = %s, plist name = %s\n", pre_str.data(), plist->list_name);
#endif
	plist->part_bucket = new vector<string>();
	plist->next = NULL;
	ret = gramfs_super->edge_db.get(pre_str, &str_dentry); // for root dentry
	if (ret == 0)
	{
		ret = -ENOENT;
		return ret;
	} else {
		ret = 0;
	}
	
	plist->part_bucket->push_back(str_dentry);
	q = plist;

	int64_t search_count = 0;
	for (uint32_t i = 2; i < str_vector.size(); i++)
	{
		string new_pre_str;
		p = (struct part_list*)calloc(1, sizeof(struct part_list));
		new_pre_str = str_vector[i - 1] + PATH_DELIMIT + str_vector[i] + PATH_DELIMIT;
		//p->list_name = (char *)new_pre_str.data();
		strcpy(p->list_name, new_pre_str.data());
	#ifdef GRAMFS_DEBUG
		printf("lookup the %d th, pre_str = %s, plist name = %s\n", i, new_pre_str.data(), p->list_name);
	#endif
		p->part_bucket = new vector<string>();
		p->next = NULL;

		search_count = gramfs_super->edge_db.match_prefix(new_pre_str, &tmp_key, -1, NULL);  // get all prefix key without inode
		if (search_count <= 0)    // error case
		{
		#ifdef GRAMFS_DEBUG
			printf("lookup, pre_str = %s not in edge kv\n", new_pre_str.data());
		#endif
			ret = -ENOENT;
			return ret;
		}
	#ifdef GRAMFS_DEBUG
		gramfs_super->GetLog()->LogMsg("lookup, %d th sub-dentry, pre-key = %s, total  = %d\n", i, new_pre_str.data(), tmp_key.size());
	#endif
		for(uint32_t j = 0; j < tmp_key.size(); j++)
		{
			string tmp_value;
			ret = gramfs_super->edge_db.get(tmp_key[j], &tmp_value);
			if (ret == 0)    // error case
			{
			#ifdef GRAMFS_DEBUG
				printf("lookup, error case : find key = %s but not find the value\n", tmp_key[j].data());
			#endif
				ret = -EIO;
				return ret;
			}
			p->part_bucket->push_back(tmp_value);  // save all value for this key
		#ifdef GRAMFS_DEBUG
			gramfs_super->GetLog()->LogMsg("lookup : %s, dentry : %s\n", tmp_key[j].data(), tmp_value.data());
		#endif
		}
		tmp_key.clear();
		q->next = p;
		q = q->next;
		p = NULL;
	}
#ifdef GRAMFS_DEBUG
	gramfs_super->GetLog()->LogMsg("lookup, find all sub-dentry, begin matching...\n");
#endif
	ret = dentry_match(plist, dentry);    // allocate value and find value
	if (ret == -1)
		return -ENOENT;
	else
		return 0;
}

void update_kv(string key, string value, int type)
{
	if (type == 0)    // edge kv
	{
		gramfs_super->edge_db.set(key, value);
	} else {
		gramfs_super->node_db.set(key, value);
	}
}

// ============== user api ==================

// root should be created first!
int gramfs_init(const char *edgepath, const char *nodepath, const char *sfpath)
{
	int ret = 0;
	gramfs_super = new GramfsSuper(edgepath, nodepath, sfpath);
	ret = gramfs_super->Open();
	if (ret < 0)
		return ret;
	struct dentry *root_dentry = (struct dentry *)calloc(1, sizeof(struct dentry));
	string add_key;
	string add_value;
	root_dentry->o_inode = 0;
	root_dentry->p_inode = 0;
	//root_dentry->dentry_name = "/";
	strcpy(root_dentry->dentry_name, "/");
	set_dentry_flag(root_dentry, D_type, DIR_DENTRY);
	root_dentry->mode = S_IFDIR | 0755;
	root_dentry->ctime = time(NULL);
	root_dentry->mtime = time(NULL);
	root_dentry->atime = time(NULL);
	root_dentry->nlink = 0;
	root_dentry->gid = getgid();
	root_dentry->uid = getuid();
	root_dentry->size = 0;
	add_value = serialize_dentry(root_dentry);
	add_key = PATH_DELIMIT + string(PATH_DELIMIT) + to_string(root_dentry->o_inode);
	ret = gramfs_super->edge_db.set(add_key, add_value);
#ifdef GRAMFS_DEBUG
	gramfs_super->GetLog()->LogMsg("gramfs init root entry key = %s, value = %s, with ret = %d\n", add_key.data(), add_value.data(), ret);
#endif
	return ret;
}

void gramfs_destroy()
{
	gramfs_super->Close();
	delete gramfs_super;
}

GramfsSuper* get_gramfs_super()
{
	return gramfs_super;
}

int gramfs_mkdir(const char *path, mode_t mode)
{
	int ret = 0;
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
	#ifdef GRAMFS_DEBUG
		gramfs_super->GetLog()->LogMsg("mkdir path = %s is a child of root, his name = %s\n", path, cur_name.data());
	#endif
	} else {
		p_path = full_path.substr(0, index);
		cur_name = full_path.substr(index + 1, full_path.size());
		index = p_path.find_last_of(PATH_DELIMIT);
		p_name = p_path.substr(index + 1, p_path.size());
	#ifdef GRAMFS_DEBUG
		gramfs_super->GetLog()->LogMsg("mkdir path = %s is not a child of root, his name = %s, p_name = %s\n", path, cur_name.data(),p_name.data());
	#endif
	}
	struct dentry *dentry = NULL;
	dentry = (struct dentry*)calloc(1, sizeof(struct dentry));
	ret = lookup(p_path.data(), dentry);
	if (ret < 0)
		return -ENOTDIR;    // parent not exist
	if (get_dentry_flag(dentry, D_type) == FILE_DENTRY)
		return -ENOTDIR;
	string add_key;
	string add_value;
	add_key = p_name + PATH_DELIMIT + cur_name + PATH_DELIMIT + to_string(dentry->o_inode);
	if (gramfs_super->edge_db.get(add_key, &add_value))
	{
	#ifdef GRAMFS_DEBUG
		gramfs_super->GetLog()->LogMsg("mkdir, you created dir = %s exists in the namespace\n", cur_name.data());
	#endif
		return -EEXIST;    // this dir has existed
	}
	struct dentry *add_dentry = (struct dentry *)calloc(1, sizeof(struct dentry));
	add_dentry->o_inode = gramfs_super->generate_unique_id();
	add_dentry->p_inode = dentry->o_inode;
	cur_name.copy(add_dentry->dentry_name, cur_name.size(), 0);
	*(add_dentry->dentry_name + cur_name.size()) = '\0';
	set_dentry_flag(add_dentry, D_type, DIR_DENTRY);
	add_dentry->mode = S_IFDIR | 0755;
	add_dentry->ctime = time(NULL);
	add_dentry->mtime = time(NULL);
	add_dentry->atime = time(NULL);
	add_dentry->nlink = 0;
	add_dentry->gid = getgid();
	add_dentry->uid = getuid();
	add_dentry->size = 0;
	
	add_value = serialize_dentry(add_dentry);
	ret = gramfs_super->edge_db.set(add_key, add_value);

	if (ret == 0)
		return -EIO;
#ifdef GRAMFS_DEBUG
		gramfs_super->GetLog()->LogMsg("mkdir, create edge key = %s with ret = %d\n", add_key.data(), ret);
#endif
	// add to node kv
	add_key = to_string(dentry->o_inode) + PATH_DELIMIT + dentry->dentry_name + PATH_DELIMIT + to_string(add_dentry->o_inode);
	add_value = cur_name;
	ret = gramfs_super->node_db.set(add_key, add_value);
	if (ret == 0)
		return -EIO;
#ifdef GRAMFS_DEBUG
	gramfs_super->GetLog()->LogMsg("mkdir, create node key = %s with ret = %d\n", add_key.data(), ret);
#endif
	return 0;
}

int gramfs_rmdir(const char *path, bool rmall)
{
	int ret = 0;
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
	dentry = (struct dentry*)calloc(1, sizeof(struct dentry));
	ret = lookup(path, dentry);
	if (ret < 0)
		return -ENOENT;    // path not exist
	if (get_dentry_flag(dentry, D_type) == FILE_DENTRY)
		return -ENOTDIR;
	string rm_key;
	rm_key = to_string(dentry->o_inode) + PATH_DELIMIT + dentry->dentry_name + PATH_DELIMIT;
	vector<string> tmp_key;
	gramfs_super->node_db.match_prefix(rm_key, &tmp_key, -1, NULL);
	if (tmp_key.empty())    // this is an empty dir (remove directly)
	{
		rm_key = p_name + PATH_DELIMIT + dentry->dentry_name + PATH_DELIMIT + to_string(dentry->p_inode);
		ret = gramfs_super->edge_db.remove(rm_key);
		if (ret == 0)
		{
		#ifdef GRAMFS_DEBUG
			gramfs_super->GetLog()->LogMsg("remove, rm the empty dir = %s failed!\n", cur_name.data());
		#endif
			return -EIO;
		}
		return 0;
	} else {
		if (rmall)
		{
			rm_key = p_name + PATH_DELIMIT + dentry->dentry_name + PATH_DELIMIT + to_string(dentry->p_inode);
			ret = gramfs_super->edge_db.remove(rm_key);    // no need to remove the child, because it can't be found
			if (ret == 0)
			{
			#ifdef GRAMFS_DEBUG
				gramfs_super->GetLog()->LogMsg("remove, rm the empty dir = %s failed!\n", cur_name.data());
			#endif
				return -EIO;
			}
			return 0;		
		} else {
			return -ENOTEMPTY;    // not an empty dir without -r
		}
	}
}

int gramfs_readdir(const char *path)
{
	int ret = 0;
	string full_path = path;
	struct dentry *dentry = NULL;
	dentry = (struct dentry*)calloc(1, sizeof(struct dentry));
	ret = lookup(path, dentry);
	if (ret < 0)
		return -ENOENT;
	if (get_dentry_flag(dentry, D_type) == FILE_DENTRY)
		return -ENOTDIR;
	string read_key = to_string(dentry->o_inode) + PATH_DELIMIT + dentry->dentry_name + PATH_DELIMIT;
	vector<string> tmp_key;
	string read_value;
	gramfs_super->node_db.match_prefix(read_key, &tmp_key, -1, NULL);
	for (uint32_t i = 0; i < tmp_key.size(); i++)
	{
		read_key = tmp_key[i];
		get_gramfs_super()->node_db.get(read_key, &read_value);
		cout<< "child "<< i << " is :" <<read_value<<endl;
	}
	return 0;
}

int gramfs_opendir(const char *path)
{
	return 0;
}

int gramfs_releasedir(const char *path)
{
	return 0;
}

int gramfs_getattr(const char *path, struct stat *st)
{
	int ret = 0;
	string full_path = path;
	struct dentry *dentry  = NULL;
	dentry = (struct dentry *)calloc(1, sizeof(struct dentry));
#ifdef GRAMFS_DEBUG	
	gramfs_super->GetLog()->LogMsg("getattr: %s\n", path);
#endif
	ret = lookup(path, dentry);
	if (ret < 0)
		return -ENOENT;
	st->st_mtime = dentry->mtime;
	st->st_ctime = dentry->ctime;
	st->st_gid = dentry->gid;
	st->st_uid = dentry->uid;
	st->st_nlink = dentry->nlink;
	st->st_size = dentry->size;
	st->st_mode = dentry->mode;
	return ret;
}

int gramfs_rename(const char *path, const char *newpath)
{
	return 0;
}

int gramfs_create(const char *path, mode_t mode)
{
	int ret = 0;
	int fd = 0;
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
	#ifdef GRAMFS_DEBUG
		gramfs_super->GetLog()->LogMsg("create path = %s is a child of root, his name = %s\n", path, cur_name.data());
	#endif
	} else {
		p_path = full_path.substr(0, index);
		cur_name = full_path.substr(index + 1, full_path.size());
		index = p_path.find_last_of(PATH_DELIMIT);
		p_name = p_path.substr(index + 1, p_path.size());
	#ifdef GRAMFS_DEBUG
		gramfs_super->GetLog()->LogMsg("create path = %s is not a child of root, his name = %s, p_name = %s\n", path, cur_name.data(),p_name.data());
	#endif
	}
	struct dentry *dentry = NULL;
	dentry = (struct dentry*)calloc(1, sizeof(struct dentry));
	ret = lookup(p_path.data(), dentry);
	if (ret < 0)
		return -ENOTDIR;
	if (get_dentry_flag(dentry, D_type) == FILE_DENTRY)
		return -ENOTDIR;
	string add_key;
	string add_value;
	add_key = p_name + PATH_DELIMIT + cur_name + PATH_DELIMIT + to_string(dentry->o_inode);
	if (gramfs_super->edge_db.get(add_key, &add_value))
	{
	#ifdef GRAMFS_DEBUG
		gramfs_super->GetLog()->LogMsg("create, you created file = %s exists in the namespace\n", cur_name.data());
	#endif
		return -EEXIST;    // this dir has existed
	}
	struct dentry *add_dentry = (struct dentry *)calloc(1, sizeof(struct dentry));
	add_dentry->o_inode = gramfs_super->generate_unique_id();
	fd = add_dentry->o_inode;    // record the created inode as open description
	add_dentry->p_inode = dentry->o_inode;
	cur_name.copy(add_dentry->dentry_name, cur_name.size(), 0);
	*(add_dentry->dentry_name + cur_name.size()) = '\0';
	set_dentry_flag(add_dentry, D_type, FILE_DENTRY);
	set_dentry_flag(add_dentry, D_small_file, SMALL_FILE);
	add_dentry->mode = S_IFREG | 0755;
	add_dentry->ctime = time(NULL);
	add_dentry->mtime = time(NULL);
	add_dentry->atime = time(NULL);
	add_dentry->nlink = 0;
	add_dentry->gid = getgid();
	add_dentry->uid = getuid();
	add_dentry->size = 0;
	add_value = serialize_dentry(add_dentry);
	ret = gramfs_super->edge_db.set(add_key, add_value);
	if (ret == 0)
		return -EIO;
#ifdef GRAMFS_DEBUG
	gramfs_super->GetLog()->LogMsg("create, create edge key = %s with ret = %d\n", add_key.data(), ret);
#endif
	// add to node kv
	add_key = to_string(dentry->o_inode) + PATH_DELIMIT + dentry->dentry_name + PATH_DELIMIT + to_string(add_dentry->o_inode);
	add_value = cur_name;
	ret = gramfs_super->node_db.set(add_key, add_value);
	if (ret == 0)
		return -EIO;
#ifdef GRAMFS_DEBUG
	gramfs_super->GetLog()->LogMsg("create, create node key = %s with ret = %d\n", add_key.data(), ret);
#endif
	//return fd;	
	return 0;
}

int gramfs_unlink(const char *path)
{
	int ret = 0;
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
	dentry = (struct dentry*)calloc(1, sizeof(struct dentry));
	ret = lookup(path, dentry);
	if (ret < 0)
		return -ENOENT;
	if (get_dentry_flag(dentry, D_type) == DIR_DENTRY)
		return -EISDIR;
	string rm_key;
	rm_key = p_name + PATH_DELIMIT + cur_name + PATH_DELIMIT + to_string(dentry->p_inode);
	ret = gramfs_super->edge_db.remove(rm_key);
	if (ret == 0)
	{
	#ifdef GRAMFS_DEBUG
		gramfs_super->GetLog()->LogMsg("unlink, path = %s not in edge kv (error)\n", path);
	#endif
		return -EIO;
	}
	rm_key = to_string(dentry->p_inode) + PATH_DELIMIT + p_name + PATH_DELIMIT + to_string(dentry->o_inode);
	ret = gramfs_super->node_db.remove(rm_key);	
	if (ret == 0)
	{
	#ifdef GRAMFS_DEBUG
		gramfs_super->GetLog()->LogMsg("unlink, path = %s not in edge kv (error)\n", path);
	#endif
		return -EIO;
	}

	// following seems redundant
	if (get_dentry_flag(dentry, D_small_file) == SMALL_FILE)    // for small file
	{
		rm_key = to_string(dentry->o_inode) + cur_name;
		gramfs_super->sf_db.remove(rm_key);    // remove the data
	} else {
		rm_key = BIG_FILE_PATH + to_string(dentry->o_inode / 1000) + PATH_DELIMIT + to_string(dentry->o_inode);
		if (access(rm_key.data(), F_OK) != 0)
		{
		#ifdef GRAMFS_DEBUG
			gramfs_super->GetLog()->LogMsg("unlink, path = %s not exist this big file (error)\n", path);
		#endif
			return -EIO;
		}
		remove(rm_key.data());
	}
	return 0;
}

int gramfs_open(const char *path, mode_t mode)
{
	int ret = 0;
	int fd = 0;
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
	dentry = (struct dentry *)calloc(1, sizeof(struct dentry));
	ret = lookup(p_path.data(), dentry);
	if (ret < 0)
		return -ENOENT;
	if (get_dentry_flag(dentry, D_type) == FILE_DENTRY)
		return -ENOTDIR;
	string key;
	string value;
	key = p_name + PATH_DELIMIT + cur_name + PATH_DELIMIT + to_string(dentry->o_inode);
	if (gramfs_super->edge_db.get(key, &value) == 0)
	{
		if ((mode & O_CREAT) == 0)
			return -ENOENT;
		return gramfs_create(path, mode);
	}
	deserialize_dentry(value, dentry);
	if (get_dentry_flag(dentry, D_type) == DIR_DENTRY)
		return -EISDIR;
	if (get_dentry_flag(dentry, D_small_file) == SMALL_FILE)
	{
	#ifdef GRAMFS_DEBUG
		gramfs_super->GetLog()->LogMsg("open, is a small file just reture the inode = %d, as file description\n", (int)dentry->o_inode);
	#endif
		//return dentry->o_inode;
		return 0;
	} else {
		string read_path;
		read_path = BIG_FILE_PATH + to_string(dentry->o_inode / 1000) + PATH_DELIMIT + to_string(dentry->o_inode);
		fd = open(read_path.data(), mode | O_CREAT);
	#ifdef GRAMFS_DEBUG
		gramfs_super->GetLog()->LogMsg("open, not a small file, big file path = %s, fd = %d\n", read_path.data(), fd);
	#endif
		return fd;
	}
}

int gramfs_release(const char *path)
{
	return 0;
}

int gramfs_read(const char *path, char *buf, size_t size, off_t offset, int fd = 0)
{
	int ret = 0;
	int new_fd = fd;
	string full_path = path;
	struct dentry *dentry = NULL;
	dentry = (struct dentry*)calloc(1, sizeof(struct dentry));
	ret = lookup(path, dentry);
	if (ret < 0)
		return -ENOENT;
	if (get_dentry_flag(dentry, D_type) == DIR_DENTRY)
		return -EISDIR;
	if (get_dentry_flag(dentry, D_small_file) == SMALL_FILE)    // small file
	{
	#ifdef GRAMFS_DEBUG
		printf("read, dentry is small file\n");
	#endif
		string read_key;
		string read_value;
		read_key = to_string(dentry->o_inode) + PATH_DELIMIT + dentry->dentry_name;
		gramfs_super->sf_db.get(read_key, &read_value);
		#ifdef GRAMFS_DEBUG
			gramfs_super->GetLog()->LogMsg("read, from data kv get cont = %s\n", read_value.data());
		#endif
		if (size > read_value.size())
		{
			size = read_value.size();
			read_value.copy(buf, read_value.size(), offset);
			*(buf + read_value.size()) = '\0';
			offset += read_value.size();
			return size;
		} else {
			read_value.copy(buf, size, 0);
			offset += size;
			return size;
		}
	} else {
		string read_path;
		read_path = BIG_FILE_PATH + to_string(dentry->o_inode / 1000) + PATH_DELIMIT + to_string(dentry->o_inode);
	#ifdef GRAMFS_DEBUG
		gramfs_super->GetLog()->LogMsg("read, from big file path = %s, fd = %d\n", read_path.data(), new_fd);
	#endif
		if (new_fd <= 0)    // not reopen the path
			new_fd = open(read_path.data(), O_RDONLY);
		if (new_fd < 0)
			return -EBADF;
		int ret = 0;
		ret = pread(new_fd, buf, size, offset);
		if (ret < 0)
			return -EIO;
		offset += ret;
		if (fd <= 0)
			close(new_fd);
		return ret;
	}
}

int gramfs_write(const char *path, const char *buf, size_t size, off_t offset, int fd = 0)
{
	int ret = 0;
	int new_fd = fd;
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
	dentry = (struct dentry*)calloc(1, sizeof(struct dentry));
	ret = lookup(path, dentry);
	if (ret < 0)
		return -ENOENT;
	if (get_dentry_flag(dentry, D_type) == DIR_DENTRY)
		return -EISDIR;
	if (get_dentry_flag(dentry, D_small_file) == SMALL_FILE)
	{
	#ifdef GRAMFS_DEBUG
		printf("write, dentry is small file. offset = %d, dentry size = %d\n", (int)offset, (int)dentry->size);
	#endif
	
		if (offset > dentry->size)
		{
		#ifdef GRAMFS_DEBUG
			printf("write, offset bigger than the whole file\n");
		#endif
			return -EIO;
		}
		
		string read_key;
		string value;
		string write_buf;
		read_key = to_string(dentry->o_inode) + PATH_DELIMIT + dentry->dentry_name;
		gramfs_super->sf_db.get(read_key, &value);
		if (offset + dentry->size > SMALL_FILE_MAX_SIZE)
		{
		#ifdef GRAMFS_DEBUG
			gramfs_super->GetLog()->LogMsg("write, change from small file to big file\n");
		#endif
			gramfs_super->sf_db.remove(read_key);
			dentry->size += size;
			offset += size;
			set_dentry_flag(dentry, D_small_file, NORMAL_FILE);    // set flag
			string write_path;
			write_path = BIG_FILE_PATH + to_string(dentry->o_inode / 1000) + PATH_DELIMIT + to_string(dentry->o_inode);
			new_fd = open(write_path.data(), O_WRONLY | O_CREAT);
			write_buf = value.substr(0, offset) + buf;
			int ret = pwrite(new_fd, write_buf.data(), dentry->size, 0);
			if (ret < 0)
				return -1;
			close(new_fd);

			// dentry have changed
			string update_key;
			string update_value;
			update_key = p_name + PATH_DELIMIT + cur_name + PATH_DELIMIT + to_string(dentry->p_inode);
			update_value = serialize_dentry(dentry);
			update_kv(update_key, update_value, 0);
			return size;
		} else {
			dentry->size += size;
			offset += size;
			write_buf = value.substr(0, offset) + buf;
			gramfs_super->sf_db.set(read_key, write_buf);
			
			// dentry have changed
			string update_key;
			string update_value;
			update_key = p_name + PATH_DELIMIT + cur_name + PATH_DELIMIT + to_string(dentry->p_inode);
			update_value = serialize_dentry(dentry);
			update_kv(update_key, update_value, 0);
			return size;
		}
	} else {
		int ret = size;
		string write_path;
		write_path = BIG_FILE_PATH + to_string(dentry->o_inode / 1000) + PATH_DELIMIT + to_string(dentry->o_inode);
	#ifdef GRAMFS_DEBUG
		gramfs_super->GetLog()->LogMsg("read, from big file path = %s, fd = %d\n", write_path.data(), new_fd);
	#endif
		if (new_fd <= 0)
			new_fd = open(write_path.data(), O_WRONLY | O_CREAT);
		if (new_fd < 0)
			return -EBADF;
		ret = pwrite(new_fd, buf, size, offset);
		dentry->size += size;
		offset += size;
		if (fd <= 0)
			close(new_fd);

		// dentry have changed
		string update_key;
		string update_value;
		update_key = p_name + PATH_DELIMIT + cur_name + PATH_DELIMIT + to_string(dentry->p_inode);
		update_value = serialize_dentry(dentry);
		update_kv(update_key, update_value, 0);
		return ret;
	}
}

int gramfs_utimens(const char *path, const struct timespec tv[2])
{
	int ret = 0;
	struct dentry *dentry = (struct dentry *)calloc(1, sizeof(struct dentry));
	ret = lookup(path, dentry);
	if (ret < 0)
		return -ENOENT;
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
	string update_key;
	string update_value;
	update_key = p_name + PATH_DELIMIT + cur_name + PATH_DELIMIT + to_string(dentry->p_inode);
	dentry->atime = tv[0].tv_sec;
	dentry->mtime = tv[1].tv_sec;
	update_value = serialize_dentry(dentry);
	ret = gramfs_super->edge_db.set(update_key, update_value);
#ifdef GRAMFS_DEBUG
	gramfs_super->GetLog()->LogMsg("utimens, dentry name  = %s, ret = %d\n", dentry->dentry_name, ret);
#endif
	if (ret == 0)
		return -EIO;
	ret = 0;
	return ret;
}

