#define FUSE_USE_VERSION 30
#include <stdio.h>
#include <fuse.h>
#include <stdbool.h>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include "tools/logging.h"
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
	int fd = 0;
	mode_t mode = fileInfo->flags;
	fd = gramfs_open(path, mode);
	if (fd < 0)
		return fd;
	fileInfo->fh = (uint64_t)fd;
	return 0;
}

int fuse_create(const char * path, mode_t mode, struct fuse_file_info * info)
{
	int ret = 0;
	ret = gramfs_create(path, mode);
	if (ret < 0)
		return ret;
	info->fh = 0;
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
	int ret = 0;
	struct dentry *dentry = NULL;
	dentry = (struct dentry*)calloc(1, sizeof(struct dentry));
	char *sub_name = NULL;
	ret = lookup(path, dentry);
	if (ret < 0)
		return -ENOENT;
	if (get_dentry_flag(dentry, D_type) == FILE_DENTRY)
		return -ENOTDIR;
	string read_key = to_string(dentry->o_inode) + PATH_DELIMIT + dentry->dentry_name;
	vector<string> tmp_key;
	string read_value;
	get_gramfs_super()->node_db.match_prefix(read_key, &tmp_key, -1, NULL);
#ifdef GRAMFS_DEBUG
	if (tmp_key.size() == 0)
	{
		get_gramfs_super()->GetLog()->LogMsg("readdir path = %s has no child\n", path);
	}
#endif
	
	for (uint32_t i = 0; i < tmp_key.size(); i++)
	{
		read_key = tmp_key[i];
		get_gramfs_super()->node_db.get(read_key, &read_value);
		sub_name = (char *)read_value.data();
	#ifdef GRAMFS_DEBUG
		get_gramfs_super()->GetLog()->LogMsg("readdir path = %s, have child key : %s\n", path, sub_name);
	#endif
		if (filler(buf, sub_name, NULL, 0) < 0)
		{
			printf("filler %s error in func = %s\n", sub_name, __FUNCTION__);
			return -1;
		}
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
	return gramfs_rmdir(path, false);
}

int fuse_rename(const char *path, const char *newpath)
{
	return 0;
}

int fuse_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo)
{
	int fd = (int) fileInfo->fh;
	int ret = 0;
	ret = gramfs_read(path, buf, size, offset, fd);
	//if (ret < 0)
	//	return ret;
	//fileInfo->fh = (uint64_t) ret;
	return ret;
}

int fuse_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fileInfo)
{
	int fd = (int) fileInfo->fh;
	int ret = 0;
	ret = gramfs_write(path, buf, size, offset, fd);
	return ret;
}

int fuse_release(const char *path, struct fuse_file_info *fileInfo)
{
	int ret = 0;
	int fd = (int) fileInfo->fh;
	if (fd < 0)
		return -EBADF;
	if (fileInfo->fh == 0)
		return 0;
	ret = close(fd);
	if (ret != 0)
		return -EIO;
	fileInfo->fh = -1;
	return 0;
}

int fuse_releasedir(const char *path, struct fuse_file_info *fileInfo)
{
	return 0;
}

int fuse_utimens(const char * path, const struct timespec tv[2])
{
	return gramfs_utimens(path, tv);
}

int fuse_truncate(const char * path, off_t offset)
{
	return 0;
}

int fuse_unlink(const char * path)
{
	return gramfs_unlink(path);
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
    //.mkdir = fuse_mkdir,
    //.opendir = fuse_opendir,
    //.readdir = fuse_readdir,
    //.getattr = fuse_getattr,
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

	// for test begin
	string edgepath = "/mnt/myfs/edge.kch";
	string nodepath = "/mnt/myfs/node.kch";
	string datapath = "/mnt/myfs/sf.kch";
	// for test end
	fuse_init(edgepath.data(), nodepath.data(), datapath.data());
	printf("starting fuse main...\n");

	// hook the file operations
	fuse_ops.mkdir = fuse_mkdir;
	fuse_ops.readdir = fuse_readdir;
	fuse_ops.getattr = fuse_getattr;
	fuse_ops.create = fuse_create;
	fuse_ops.open = fuse_open;
	fuse_ops.rmdir = fuse_rmdir;
	fuse_ops.unlink = fuse_unlink;
	fuse_ops.read = fuse_read;
	fuse_ops.write = fuse_write;
	fuse_ops.utimens = fuse_utimens;
	fuse_ops.release = fuse_release;
	
	ret = fuse_main(fuse_argc, fuse_argv, &fuse_ops, NULL);
	printf("fuse main finished, ret %d\n", ret);
	fuse_destroy();
	return ret;
}
