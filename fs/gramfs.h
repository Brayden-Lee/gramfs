#ifndef GRAMFS_H
#define GRAMFS_H

#include <sys/time.h>
#include <stdbool.h>
#include <string>
#include <vector>
#include "gramfs_super.h"

using namespace std;

// DEBUG
#define GRAMFS_DEBUG

#define DENTRY_NAME_LEN 32
#define PATH_DELIMIT "/"
#define KC_VALUE_DELIMIT "&"
#define BIG_FILE_PATH "/mnt/bigfile/"
#define SMALL_FILE_MAX_SIZE 4 * 1024


// error code
#define ENOENT 2
#define EIO 5
#define EEXIST 17
#define ENOTDIR 20
#define EISDIR 21
#define ENOTEMPTY 39


#define DIR_DENTRY 0
#define FILE_DENTRY 1
#define SMALL_FILE 1
#define NORMAL_FILE 0

struct dentry {
	uint64_t p_inode;
	uint64_t o_inode;
	char dentry_name[DENTRY_NAME_LEN];
	uint32_t flags;
	uint32_t mode;
	uint32_t ctime;
	uint32_t mtime;
	uint32_t atime;
	uint32_t size;
	uint32_t uid;
	uint32_t gid;
	uint32_t nlink;
};

enum dentryflags
{
	D_type,    // file/dir
	D_small_file,    // 1 is small file, 0 is normal file
};
	
struct part_list {
	char *list_name;
	vector<string> *part_bucket;    // store the value (kv) of this node
	struct part_list *next;
};

// api for user
int gramfs_init(const char *edgepath, const char *nodepath, const char *sfpath);
void gramfs_destroy();
GramfsSuper* get_gramfs_super();
int lookup(const char *path, struct dentry *dentry);
int gramfs_mkdir(const char *path, mode_t mode);
int gramfs_rmdir(const char *path, bool rmall);
int gramfs_readdir(const char *path);
int gramfs_releasedir(const char *path);
int gramfs_getattr(const char *path, struct stat *st);
int gramfs_rename(const char *path, const char *newpath);
int gramfs_open(const char *path, mode_t mode);
int gramfs_release(const char *path);
int gramfs_read(const char *path, char *buf, size_t size, off_t offset);
int gramfs_write(const char *path, const char *buf, size_t size, off_t offset);

#endif