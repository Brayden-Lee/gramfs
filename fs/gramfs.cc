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

struct dentry* dentry_match(struct part_list *plist)
{
	struct dentry* dentry = (struct dentry*)calloc(1, sizeof(struct dentry));
	struct part_list *p;
	string str_dentry = NULL;
	uint32_t curr_id = 0;    // root inode
	bool triggle = false;
	if (plist == NULL)
	{
		delete dentry;
		dentry = NULL;
		return dentry;
	}
	while (plist != NULL)
	{
		for (uint32_t i = 0; i < plist->part_bucket->size(); i++)
		{
			str_dentry = plist->part_bucket->front();
			plist->part_bucket->pop_back();
			deserialize_dentry(str_dentry, dentry);
			if (dentry->p_inode == curr_id)
			{
				plist->part_bucket->clear();
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

	int ret = 0;
	if (strcmp(path, PATH_DELIMIT) == 0)
	{ // for lookup "/"
		string root_key;
		string root_value;
		root_key = "//0";
		ret = gramfs_super->edge_db.get(root_key, &root_value);
		printf("look up (root) ret = %d, get value = %s\n", ret, root_value.data());
		if (ret < 0)
			return ret;
		else
			ret = 0;
		deserialize_dentry(root_value, dentry);
	#ifdef GRAMFS_DEBUG
		printf("this is root dentry, get ret = %d\n", ret);
	#endif
		return ret;
	}
	split_path(path, "/", str_vector);

	struct part_list * plist = (struct part_list*)malloc(sizeof(struct part_list));
	struct part_list *p = NULL;
	struct part_list *q = NULL;
	pre_str = str_vector[1];
	pre_str = string("/") + PATH_DELIMIT + pre_str;
	plist->list_name = (char *)pre_str.data();
	plist->part_bucket = new vector<string>();
	plist->next = NULL;
	gramfs_super->edge_db.get(pre_str, &str_dentry); // for root dentry
	plist->part_bucket->push_back(str_dentry);
	q = plist;

#ifdef GRAMFS_DEBUG
	gramfs_super->GetLog()->LogMsg("lookup : %s, dentry : %s\n", pre_str.data(), str_dentry.data());
#endif
	for (uint32_t i = 2; i < str_vector.size(); i++)
	{
		p = (struct part_list*)malloc(sizeof(struct part_list));
		pre_str = str_vector[i - 1] + PATH_DELIMIT + str_vector[i];
		p->list_name = (char *)pre_str.data();
		p->part_bucket = new vector<string>();
		p->next = NULL;

		gramfs_super->edge_db.match_prefix(pre_str, &tmp_key, -1, NULL);  // get all prefix key without inode
		for(uint32_t j = 0; j < tmp_key.size(); j++)
		{
			gramfs_super->edge_db.get(tmp_key[j], &pre_str);
			p->part_bucket->push_back(pre_str);  // save all value for this key
		#ifdef GRAMFS_DEBUG
			gramfs_super->GetLog()->LogMsg("lookup : %s, dentry : %s\n", tmp_key[i].data(), pre_str.data());
		#endif
		}
		tmp_key.clear();
		q->next = p;
		q = q->next;
		p = NULL;
	}
	dentry = dentry_match(plist);    // allocate value and find value
	if (dentry == NULL)
		return -ENOENT;
	else
		return 0;
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
	lookup(p_path.data(), dentry);
	if (dentry == NULL)
		return -ENOTDIR;    // parent not exist
	string add_key;
	string add_value;
	add_key = p_name + PATH_DELIMIT + cur_name + PATH_DELIMIT + to_string(dentry->o_inode);
	if (gramfs_super->edge_db.get(add_key, &add_value))
	{
		return -EEXIST;    // this dir has existed
	}
	struct dentry *add_dentry = (struct dentry *)calloc(1, sizeof(struct dentry));
	add_dentry->o_inode = gramfs_super->generate_unique_id();
	add_dentry->p_inode = dentry->o_inode;
	cur_name.copy(add_dentry->dentry_name, cur_name.size(), 0);
	*(add_dentry->dentry_name + cur_name.size()) = '\0';
	//add_dentry->flags = 0;    // dir
	set_dentry_flag(add_dentry, D_type, DIR_DENTRY);
	add_dentry->mode = S_IFDIR | 0755;
	add_dentry->ctime = time(NULL);
	add_dentry->mtime = time(NULL);
	add_dentry->atime = time(NULL);
	cur_name.copy(add_dentry->dentry_name,cur_name.size(),0);
	*(add_dentry->dentry_name + cur_name.size()) = '\0';
	//add_dentry->dentry_name = cur_name;
	add_dentry->nlink = 0;
	add_dentry->gid = getgid();
	add_dentry->uid = getuid();
	add_dentry->size = 0;
	add_value = serialize_dentry(add_dentry);
	gramfs_super->edge_db.set(add_key, add_value);

	// add to node kv
	add_key = to_string(dentry->o_inode) + PATH_DELIMIT + dentry->dentry_name + PATH_DELIMIT + to_string(add_dentry->o_inode);
	add_value = cur_name;
	gramfs_super->node_db.set(add_key, add_value);
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
	dentry = (struct dentry*)calloc(1, sizeof(struct dentry));
	lookup(path, dentry);
	if (dentry == NULL)
		return -1;    // path not exist
	string rm_key;
	rm_key = to_string(dentry->o_inode) + PATH_DELIMIT + dentry->dentry_name;
	vector<string> tmp_key;
	gramfs_super->node_db.match_prefix(rm_key, &tmp_key, -1, NULL);
	if (tmp_key.empty())
	{
		rm_key = p_name + PATH_DELIMIT + dentry->dentry_name + PATH_DELIMIT + to_string(dentry->p_inode);
		gramfs_super->edge_db.remove(rm_key);
		return 0;
	} else {
		if (rmall)
		{
			rm_key = p_name + PATH_DELIMIT + dentry->dentry_name + PATH_DELIMIT + to_string(dentry->p_inode);
			gramfs_super->edge_db.remove(rm_key);    // no need to remove the child, because it can't be found
		} else {
			return -1;    // not an empty dir without -r
		}
	}
	return 0;
}

int gramfs_readdir(const char *path)
{
	string full_path = path;
	struct dentry *dentry = NULL;
	dentry = (struct dentry*)calloc(1, sizeof(struct dentry));
	lookup(path, dentry);
	if (dentry == NULL)
		return -ENOENT;
	string read_key = to_string(dentry->o_inode) + PATH_DELIMIT + dentry->dentry_name;
	vector<string> tmp_key;
	gramfs_super->node_db.match_prefix(read_key, &tmp_key, -1, NULL);
	for (uint32_t i = 0; i < tmp_key.size(); i++)
	{
		cout<< "child "<< i << " is :" <<tmp_key[i]<<endl;
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
	if (dentry == NULL)
		return -ENOENT;
	st->st_mtime = dentry->ctime;
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
	lookup(p_path.data(), dentry);
	if (dentry == NULL)
		return -1;
	string add_key;
	string add_value;
	add_key = p_name + PATH_DELIMIT + cur_name + PATH_DELIMIT + to_string(dentry->o_inode);
	if (gramfs_super->edge_db.get(add_key, &add_value))
	{
		return -1;    // this dir has existed
	}
	struct dentry *add_dentry = (struct dentry *)calloc(1, sizeof(struct dentry));
	add_dentry->o_inode = gramfs_super->generate_unique_id();
	add_dentry->p_inode = dentry->o_inode;
	cur_name.copy(add_dentry->dentry_name, cur_name.size(), 0);
	*(add_dentry->dentry_name + cur_name.size()) = '\0';
	//add_dentry->flags = 1;    // file
	set_dentry_flag(add_dentry, D_type, FILE_DENTRY);
	set_dentry_flag(add_dentry, D_small_file, SMALL_FILE);
	add_dentry->mode = S_IFDIR | 0755;
	add_dentry->ctime = time(NULL);
	add_dentry->mtime = time(NULL);
	add_dentry->atime = time(NULL);
	cur_name.copy(add_dentry->dentry_name,cur_name.size(),0);
	*(add_dentry->dentry_name + cur_name.size()) = '\0';
	//add_dentry->dentry_name = cur_name;
	add_dentry->nlink = 0;
	add_dentry->gid = getgid();
	add_dentry->uid = getuid();
	add_dentry->size = 0;
	add_value = serialize_dentry(add_dentry);
	gramfs_super->edge_db.set(add_key, add_value);

	// add to node kv
	add_key = to_string(dentry->o_inode) + PATH_DELIMIT + dentry->dentry_name + PATH_DELIMIT + to_string(add_dentry->o_inode);
	add_value = cur_name;
	gramfs_super->node_db.set(add_key, add_value);
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
	dentry = (struct dentry*)calloc(1, sizeof(struct dentry));
	lookup(path, dentry);
	if (dentry == NULL)
		return -1;
	string rm_key;
	rm_key = p_name + PATH_DELIMIT + cur_name + PATH_DELIMIT + to_string(dentry->p_inode);
	gramfs_super->edge_db.remove(rm_key);
	rm_key = to_string(dentry->p_inode) + PATH_DELIMIT + p_name + PATH_DELIMIT + to_string(dentry->o_inode);
	gramfs_super->node_db.remove(rm_key);
	return 0;
}

int gramfs_open(const char *path, mode_t mode)
{
	int ret = 0;
	if ((mode & O_CREAT) == 0)
		return gramfs_create(path, mode);
	return ret;
}

int gramfs_release(const char *path)
{
	return 0;
}

int gramfs_read(const char *path, char *buf, size_t size, off_t offset)
{
	string full_path = path;
	struct dentry *dentry = NULL;
	dentry = (struct dentry*)calloc(1, sizeof(struct dentry));
	lookup(path, dentry);
	if (dentry == NULL)
		return -1;
	if (get_dentry_flag(dentry, D_small_file) == 0)    // small file
	{
		string read_key;
		string read_value;
		read_key = to_string(dentry->o_inode) + PATH_DELIMIT + dentry->dentry_name;
		gramfs_super->sf_db.get(read_key, &read_value);
		if (size > read_value.size())
		{
			read_value.copy(buf, read_value.size(), 0);
			*(buf + read_value.size()) = '\0';
			offset += read_value.size();
			return read_value.size();
		} else {
			read_value.copy(buf, size, 0);
			offset += size;
			return size;
		}
	} else {
		string read_path;
		read_path = BIG_FILE_PATH + to_string(dentry->o_inode / 1000) + PATH_DELIMIT + to_string(dentry->o_inode);
		int fd = open(read_path.data(), O_RDONLY);
		if (fd < 0)
			return -1;
		int ret = 0;
		ret = pread(fd, buf, size, offset);
		if (ret < 0)
			return -1;
		offset += ret;
		return ret;
	}
}

int gramfs_write(const char *path, const char *buf, size_t size, off_t offset)
{
	string full_path = path;
	struct dentry *dentry = NULL;
	dentry = (struct dentry*)calloc(1, sizeof(struct dentry));
	lookup(path, dentry);
	if (dentry == NULL)
		return -1;
	if (get_dentry_flag(dentry, D_small_file) == 0)
	{
		if (offset > dentry->size)
			return 0;    // offset bigger than the whole file
		string read_key;
		string value;
		string write_buf;
		read_key = to_string(dentry->o_inode) + PATH_DELIMIT + dentry->dentry_name;
		gramfs_super->sf_db.get(read_key, &value);
		if (offset + size > SMALL_FILE_MAX_SIZE)
		{
			gramfs_super->sf_db.remove(read_key);
			set_dentry_flag(dentry, D_small_file, NORMAL_FILE);    // set flag
			dentry->size += size;
			offset += size;
			string write_path;
			write_path = BIG_FILE_PATH + to_string(dentry->o_inode / 1000) + PATH_DELIMIT + to_string(dentry->o_inode);
			int fd = open(write_path.data(), O_WRONLY | O_CREAT);
			write_buf = value.substr(0, offset) + buf;
			int ret = pwrite(fd, write_buf.data(), dentry->size, 0);
			if (ret < 0)
				return -1;
			return size;
		} else {
			dentry->size += size;
			offset += size;
			write_buf = value.substr(0, offset) + buf;
			gramfs_super->sf_db.set(read_key, write_buf);
			return size;
		}
	} else {
		int ret = size;
		//need write
		return ret;
	}
}
