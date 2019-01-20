#ifndef GRAMFS_H
#define GRAMFS_H

#include <stdio.h>
#include <sys/time.h>
#include <stdbool.h>
#include <string>

#define DENTRY_NAME_LEN 32
#define PATH_DELIMIT "/"

#define DIR_DENTRY 0
#define FILE_DENTRY 1

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

struct part_list {
	char *list_name;
	std::vector<string> *part_bucket;
	struct part_list *next;
}

// api for user
int fs_init(const char *edgepath, const char *nodepath);
void gramfs_destroy(const char *edgepath, const char *nodepath);
int lookup(const char *path);
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