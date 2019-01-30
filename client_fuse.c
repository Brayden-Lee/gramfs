#define FUSE_USE_VERSION 30
#include <stdio.h>
#include <fuse.h>
#include <stdbool.h>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include "fs/gramfs.h"

using namespace std;



int fuse_init(const char *edgepath, const char *nodepath, const char *sfpath)
{
	return gramfs_init(edgepath, nodepath, sfpath);
}

void fuse_destroy()
{
	gramfs_destroy();
}

int fuse_open(const char *path, struct fuse_file_info *fileInfo)
{
	return 0;
}

int fuse_create(const char * path, mode_t mode, struct fuse_file_info * info)
{
	return 0;
}

int fuse_mkdir(const char *path, mode_t mode)
{
	return gramfs_mkdir(path, mode);
}

int fuse_opendir(const char *path, struct fuse_file_info *fileInfo)
{
	return 0;
}

int fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fileInfo)
{
	struct dentry *dentry = NULL;
	char *sub_name = NULL;
	lookup(path, dentry);
	if (dentry == NULL)
		return -ENOENT;
	string read_key = to_string(dentry->o_inode) + PATH_DELIMIT + dentry->dentry_name;
	vector<string> tmp_key;
	get_gramfs_super()->node_db.match_prefix(read_key, &tmp_key, -1, NULL);
	for (uint32_t i = 0; i < tmp_key.size(); i++)
	{
		sub_name = (char *)tmp_key[i].data();
		if (filler(buf, sub_name, NULL, 0) < 0)
		{
			printf("filler %s error in func = %s\n", sub_name, __FUNCTION__);
			return -1;
		}
		//cout<< "child "<< i << " is :" <<tmp_key[i]<<endl;
	}
	if (filler(buf, ".", NULL, 0) < 0) {
		printf("filler . error in func = %s\n", __FUNCTION__);
		return -1;
	}
	if (filler(buf, "..", NULL, 0) < 0) {
		printf("filler .. error in func = %s\n", __FUNCTION__);
		return -1;
	}
	return 0;
}

int fuse_getattr(const char* path, struct stat* st)
{
	return gramfs_getattr(path, st);
}

int fuse_rmdir(const char *path)
{
	return 0;
}

int fuse_rename(const char *path, const char *newpath)
{
	return 0;
}

int fuse_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo)
{
	return 0;
}

int fuse_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo)
{
	return 0;
}

int fuse_release(const char *path, struct fuse_file_info *fileInfo)
{
	return 0;
}

int fuse_releasedir(const char *path, struct fuse_file_info *fileInfo)
{
	return 0;
}

int fuse_utimens(const char * path, const struct timespec tv[2])
{
	return 0;
}

int fuse_truncate(const char * path, off_t offset)
{
	return 0;
}

int fuse_unlink(const char * path)
{
	return 0;
}

int fuse_chmod(const char * path, mode_t mode)
{
	return 0;
}

int fuse_chown(const char * path, uid_t owner, gid_t group)
{
	return 0;
}

int fuse_access(const char * path, int amode)
{
	return 0;
}

int fuse_symlink(const char * oldpath, const char * newpath)
{
	return 0;
}

int fuse_readlink(const char * path, char * buf, size_t size)
{
	return 0;
}

/*
// this is in C
static struct fuse_operations fuse_ops =
{
    //.open = fuse_open,
    .mkdir = fuse_mkdir,
    //.opendir = fuse_opendir,
    .readdir = fuse_readdir,
    .getattr = fuse_getattr,
    //.rmdir = fuse_rmdir,
    //.rename = fuse_rename,
    //.create = fuse_create,
    //.read = fuse_read,
    //.write = fuse_write,
    //.release = fuse_release,
    //.releasedir = fuse_releasedir,
    //.utimens = fuse_utimens,
    //.truncate = fuse_truncate,
    //.unlink = fuse_unlink,
    //.chmod = fuse_chmod,
    //.chown = fuse_chown,
    //.access = fuse_access,
    //.symlink = fuse_symlink,
    //.readlink = fuse_readlink,
};

*/
static struct fuse_operations fuse_ops;

static void usage(void)
{
    printf(
    "usage: ./client_fuse edgepath nodepath smallfilepath\n"
    );
}

int main(int argc, char * argv[])
{
	int ret;
	int i;

	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "--help") == 0) {
			usage();
			return 0;
		}
	}

	int fuse_argc = 0;
	char * fuse_argv[argc];
	for (i = 0; i < argc; i++) {
		if (argv[i] != NULL) {
			fuse_argv[fuse_argc++] = argv[i];
		}
	}

	fuse_init(fuse_argv[1], fuse_argv[2], fuse_argv[3]);
	printf("starting fuse main...\n");

	fuse_ops.mkdir = fuse_mkdir;
	fuse_ops.readdir = fuse_readdir;
	fuse_ops.getattr = fuse_getattr;
	
	ret = fuse_main(fuse_argc, fuse_argv, &fuse_ops, NULL);
	printf("fuse main finished, ret %d\n", ret);
	fuse_destroy();
	return ret;
}
