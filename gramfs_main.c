#define FUSE_USE_VERSION 30

#include <stdio.h>
#include <fuse_lowlevel.h>

#include "fs/gramfs.h"

static void gramfs_ll_init(void *userdata, struct fuse_conn_info *conn)
{
	fs_init();
}

static void gramfs_ll_destroy(void *userdata)
{
	fs_destroy();
}

static void gramfs_ll_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	fs_lookup();
}

static void gramfs_ll_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	fs_getattr();
}

static void gramfs_ll_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr, int to_set, struct fuse_file_info *fi)
{
	fs_setattr();
}

static void gramfs_ll_mkdir(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode)
{
	fs_mkdir();
}

static void gramfs_ll_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	fs_open();
}

static  void gramfs_ll_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset, struct fuse_file_info *fi)
{
	fs_read();
}

static void gramfs_ll_write(fuse_req_t req, fuse_ino_t ino, const char *buf, size_t size, off_t off, struct fuse_file_info *fi)
{
	fs_write();
}

static void gramfs_ll_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi)
{
	fs_readdir();
}

static void gramfs_ll_release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	fs_release();
}

static void gramfs_ll_releasedir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
	fs_releasedir();
}

static void gramfs_ll_create(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi)
{
	fs_create();
}

static void gramfs_ll_rmdir(fuse_req_t req, fuse_ino_t parent, const char *name)
{
	fs_rmdir();
}

static void gramfs_ll_rename(fuse_req_t req, fuse_ino_t parent, const char *name, fuse_ino_t newparent, const char *newname)
{
	fs_rename();
}

static struct fuse_lowlevel_ops gramfs_ll_operation = {
	.init		= gramfs_ll_init,
	.destroy	= gramfs_ll_destroy,
	.lookup 	= gramfs_ll_lookup,
	.getattr 	= gramfs_ll_getattr,
	.setattr	= gramfs_ll_setattr,
	.mkdir		= gramfs_ll_mkdir,
	.rmdir 		= gramfs_ll_rmdir,
	.rename 	= gramfs_ll_rename,
	.readdir 	= gramfs_ll_readdir,
	.releasedir = gramfs_ll_releasedir,
	.open 		= gramfs_ll_open,
	.read 		= gramfs_ll_read,
	.write 		= gramfs_ll_write,
	.release 	= gramfs_ll_release,
	.create		= gramfs_ll_create
};

int main(int argc, char * argv[])
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	struct fuse_session *se;
	struct fuse_cmdline_opts opts;
	if (fuse_parse_cmdline(&args, &opts) != 0)
		return 1;
	if (opts.show_help) {
		printf("usage: %s [options] <mountpoint>\n\n", argv[0]);
		fuse_cmdline_help();
		fuse_lowlevel_help();
		ret = 0;
		goto err_out1;
	} else if (opts.show_version) {
		printf("FUSE library version %s\n", fuse_pkgversion());
		fuse_lowlevel_version();
		ret = 0;
		goto err_out1;
	}
	se = fuse_session_new(&args, &gramfs_ll_operation, sizeof(gramfs_ll_operation), NULL);
	if (se == NULL)
		goto err_out1;
	if (fuse_set_signal_handlers(se) != 0)
		goto err_out2;
	if (fuse_session_mount(se, opts.mountpoint) != 0)
	    goto err_out3;

	fuse_daemonize(opts.foreground);
	/* Block until ctrl+c or fusermount -u */
	if (opts.singlethread)
		ret = fuse_session_loop(se);
	else
		ret = fuse_session_loop_mt(se, opts.clone_fd);

	fuse_session_unmount(se);
err_out3:
	fuse_remove_signal_handlers(se);
err_out2:
	fuse_session_destroy(se);
err_out1:
	free(opts.mountpoint);
	fuse_opt_free_args(&args);

	return ret ? 1 : 0;
}