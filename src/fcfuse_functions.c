/*
  NPHeap File System
  Copyright (C) 2016 Hung-Wei Tseng, Ph.D. <hungwei_tseng@ncsu.edu>

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.

  This code is derived from function prototypes found /usr/include/fuse/fuse.h
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  His code is licensed under the LGPLv2.
  A copy of that code is included in the file fuse.h
  
  The point of this FUSE filesystem is to provide an introduction to
  FUSE.  It was my first FUSE filesystem as I got to know the
  software; hopefully, the comments in this code will help people who
  follow later to get a gentler introduction.

*/

#include "fcfuse.h"
#include <sys/types.h>
#include <sys/unistd.h>
#include <fcontainer.h>

extern struct fcfuse_state *fcfuse_data;

static void fcfuse_fullpath(char fpath[PATH_MAX], const char *path)
{
    char container_num[11];

    strcpy(fpath, FCFS_DATA->rootdir);

    strncat(fpath, "/.");
    itoa(fcontainer_getcid(FCFS_DATA->devfd, fuse_get_context()->pid), container_num, 10);
    strcat(real_path, container_num);
    
//    if(strcmp("/", path)!=0)
    {
//        strncat(fpath, "/", 2); // ridiculously long paths will
        strncat(fpath, path, PATH_MAX); // ridiculously long paths will
	    			        // break here
    }
    log_msg("    fcfuse_fullpath:  rootdir = \"%s\", path = \"%s\", fpath = \"%s\"\n",
	    FCFS_DATA->rootdir, path, fpath);
}

///////////////////////////////////////////////////////////
//
// Prototypes for all these functions, and the C-style comments,
// come from /usr/include/fuse.h
//

char* get_real_path(const char* path) {
    char real_path[PATH_MAX];
    strcpy(real_path, path);
    
    
    strcat(real_path, "/.");

}
/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
int fcfuse_getattr(const char *path, struct stat *stbuf)
{
    int retstat = -ENOENT;;
    char fpath[PATH_MAX];
    
    log_msg("\nfcfuse_getattr(path=\"%s\", statbuf=0x%08x), pid: %d cid: %d\n",
	  path, stbuf, fuse_get_context()->pid, fcontainer_getcid(FCFS_DATA->devfd, fuse_get_context()->pid));
    fcfuse_fullpath(fpath, path);

    retstat = log_syscall("lstat", lstat(fpath, stbuf), 0);
    
    log_stat(stbuf);
    return retstat;
    
}

/** Read the target of a symbolic link
 *
 * The buffer should be filled with a null terminated string.  The
 * buffer size argument includes the space for the terminating
 * null character.  If the linkname is too long to fit in the
 * buffer, it should be truncated.  The return value should be 0
 * for success.
 */
// Note the system readlink() will truncate and lose the terminating
// null.  So, the size passed to to the system readlink() must be one
// less than the size passed to fcfuse_readlink()
// fcfuse_readlink() code by Bernardo F Costa (thanks!)
int fcfuse_readlink(const char *path, char *link, size_t size)
{
    int retstat;
    char fpath[PATH_MAX];
    
    log_msg("fcfuse_readlink(path=\"%s\", link=\"%s\", size=%d)\n",
	  path, link, size);
    fcfuse_fullpath(fpath, path);

    retstat = log_syscall("fpath", readlink(fpath, link, size - 1), 0);
    if (retstat >= 0) {
	link[retstat] = '\0';
	retstat = 0;
    }
    
    return retstat;
}

/** Create a file node
 *
 * There is no create() operation, mknod() will be called for
 * creation of all non-directory, non-symlink nodes.
 */
int fcfuse_mknod(const char *path, mode_t mode, dev_t dev)
{
    int retstat;
    char fpath[PATH_MAX];
    
    log_msg("\nfcfuse_mknod(path=\"%s\", mode=0%3o, dev=%lld)\n",
	  path, mode, dev);
    fcfuse_fullpath(fpath, path);
    
    // On Linux this could just be 'mknod(path, mode, dev)' but this
    // tries to be be more portable by honoring the quote in the Linux
    // mknod man page stating the only portable use of mknod() is to
    // make a fifo, but saying it should never actually be used for
    // that.
    if (S_ISREG(mode)) {
	retstat = log_syscall("open", open(fpath, O_CREAT | O_EXCL | O_WRONLY, mode), 0);
	if (retstat >= 0)
	    retstat = log_syscall("close", close(retstat), 0);
    } else
	if (S_ISFIFO(mode))
	    retstat = log_syscall("mkfifo", mkfifo(fpath, mode), 0);
	else
	    retstat = log_syscall("mknod", mknod(fpath, mode, dev), 0);
    
    return retstat;
}

/** Create a directory */
int fcfuse_mkdir(const char *path, mode_t mode)
{
    char fpath[PATH_MAX];
    
    log_msg("\nfcfuse_mkdir(path=\"%s\", mode=0%3o)\n",
	    path, mode);
    fcfuse_fullpath(fpath, path);

    return log_syscall("mkdir", mkdir(fpath, mode), 0);
}

/** Remove a file */
int fcfuse_unlink(const char *path)
{
    char fpath[PATH_MAX];
    
    log_msg("fcfuse_unlink(path=\"%s\")\n",
	    path);
    fcfuse_fullpath(fpath, path);

    return log_syscall("unlink", unlink(fpath), 0);
}

/** Remove a directory */
int fcfuse_rmdir(const char *path)
{
    char fpath[PATH_MAX];
    
    log_msg("fcfuse_rmdir(path=\"%s\")\n",
	    path);
    fcfuse_fullpath(fpath, path);

    return log_syscall("rmdir", rmdir(fpath), 0);
}

/** Create a symbolic link */
// The parameters here are a little bit confusing, but do correspond
// to the symlink() system call.  The 'path' is where the link points,
// while the 'link' is the link itself.  So we need to leave the path
// unaltered, but insert the link into the mounted directory.
int fcfuse_symlink(const char *path, const char *link)
{
    char flink[PATH_MAX];
    
    log_msg("\nfcfuse_symlink(path=\"%s\", link=\"%s\")\n",
	    path, link);
    fcfuse_fullpath(flink, link);

    return log_syscall("symlink", symlink(path, flink), 0);
}

/** Rename a file */
// both path and newpath are fs-relative
int fcfuse_rename(const char *path, const char *newpath)
{
    char fpath[PATH_MAX];
    char fnewpath[PATH_MAX];
    
    log_msg("\nfcfuse_rename(fpath=\"%s\", newpath=\"%s\")\n",
	    path, newpath);
    fcfuse_fullpath(fpath, path);
    fcfuse_fullpath(fnewpath, newpath);

    return log_syscall("rename", rename(fpath, fnewpath), 0);
}

/** Create a hard link to a file */
int fcfuse_link(const char *path, const char *newpath)
{
    char fpath[PATH_MAX], fnewpath[PATH_MAX];
    
    log_msg("\nfcfuse_link(path=\"%s\", newpath=\"%s\")\n",
	    path, newpath);
    fcfuse_fullpath(fpath, path);
    fcfuse_fullpath(fnewpath, newpath);

    return log_syscall("link", link(fpath, fnewpath), 0);
}

/** Change the permission bits of a file */
int fcfuse_chmod(const char *path, mode_t mode)
{
    char fpath[PATH_MAX];
    
    log_msg("\nfcfuse_chmod(fpath=\"%s\", mode=0%03o)\n",
	    path, mode);
    fcfuse_fullpath(fpath, path);

    return log_syscall("chmod", chmod(fpath, mode), 0);
}

/** Change the owner and group of a file */
int fcfuse_chown(const char *path, uid_t uid, gid_t gid)
{
    char fpath[PATH_MAX];
    
    log_msg("\nfcfuse_chown(path=\"%s\", uid=%d, gid=%d)\n",
	    path, uid, gid);
    fcfuse_fullpath(fpath, path);

    return log_syscall("chown", chown(fpath, uid, gid), 0);
}

/** Change the size of a file */
int fcfuse_truncate(const char *path, off_t newsize)
{
    char fpath[PATH_MAX];
    
    log_msg("\nfcfuse_truncate(path=\"%s\", newsize=%lld)\n",
	    path, newsize);
    fcfuse_fullpath(fpath, path);

    return log_syscall("truncate", truncate(fpath, newsize), 0);
}

/** Change the access and/or modification times of a file */
int fcfuse_utime(const char *path, struct utimbuf *ubuf)
{
    char fpath[PATH_MAX];
    
    log_msg("\nfcfuse_utime(path=\"%s\", ubuf=0x%08x)\n",
	    path, ubuf);
    fcfuse_fullpath(fpath, path);

    return log_syscall("utime", utime(fpath, ubuf), 0);
}

/** File open operation
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * return an arbitrary filehandle in the fuse_file_info structure,
 * which will be passed to all file operations.
 *
 * Changed in version 2.2
 */
int fcfuse_open(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    int fd;
    char fpath[PATH_MAX];
    
    log_msg("\nfcfuse_open(path\"%s\", fi=0x%08x)\n",
	    path, fi);
    fcfuse_fullpath(fpath, path);

    // if the open call succeeds, my retstat is the file descriptor,
    // else it's -errno.  I'm making sure that in that case the saved
    // file descriptor is exactly -1.
    fd = log_syscall("open", open(fpath, fi->flags), 0);
    if (fd < 0)
	retstat = log_error("open");
	
    fi->fh = fd;

    log_fi(fi);
    
    return retstat;

}

/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.  An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * Changed in version 2.2
 */
// I don't fully understand the documentation above -- it doesn't
// match the documentation for the read() system call which says it
// can return with anything up to the amount of data requested. nor
// with the fusexmp code which returns the amount of data also
// returned by read.
int fcfuse_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    log_msg("\nfcfuse_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
	    path, buf, size, offset, fi);
    // no need to get fpath on this one, since I work from fi->fh not the path
    log_fi(fi);

    return log_syscall("pread", pread(fi->fh, buf, size, offset), 0);
}

/** Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.  An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 */
int fcfuse_write(const char *path, const char *buf, size_t size, off_t offset,
	     struct fuse_file_info *fi)
{
    int retstat = 0;
    
    log_msg("\nfcfuse_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x), pid: %d\n",
	    path, buf, size, offset, fi, fuse_get_context()->pid
	    );
    // no need to get fpath on this one, since I work from fi->fh not the path
    log_fi(fi);

    return log_syscall("pwrite", pwrite(fi->fh, buf, size, offset), 0);
}

/** Get file system statistics
 *
 * The 'f_frsize', 'f_favail', 'f_fsid' and 'f_flag' fields are ignored
 *
 * Replaced 'struct statfs' parameter with 'struct statvfs' in
 * version 2.5
 */
int fcfuse_statfs(const char *path, struct statvfs *statv)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nfcfuse_statfs(path=\"%s\", statv=0x%08x)\n",
	    path, statv);
    fcfuse_fullpath(fpath, path);
    
    // get stats for underlying filesystem
    retstat = log_syscall("statvfs", statvfs(fpath, statv), 0);
    
    log_statvfs(statv);
    
    return retstat;
}

/** Possibly flush cached data
 *
 * BIG NOTE: This is not equivalent to fsync().  It's not a
 * request to sync dirty data.
 *
 * Flush is called on each close() of a file descriptor.  So if a
 * filesystem wants to return write errors in close() and the file
 * has cached dirty data, this is a good place to write back data
 * and return any errors.  Since many applications ignore close()
 * errors this is not always useful.
 *
 * NOTE: The flush() method may be called more than once for each
 * open().  This happens if more than one file descriptor refers
 * to an opened file due to dup(), dup2() or fork() calls.  It is
 * not possible to determine if a flush is final, so each flush
 * should be treated equally.  Multiple write-flush sequences are
 * relatively rare, so this shouldn't be a problem.
 *
 * Filesystems shouldn't assume that flush will always be called
 * after some writes, or that if will be called at all.
 *
 * Changed in version 2.2
 */

// this is a no-op in NPHFS.  It just logs the call and returns success
int fcfuse_flush(const char *path, struct fuse_file_info *fi)
{
    log_msg("\nfcfuse_flush(path=\"%s\", fi=0x%08x)\n", path, fi);
    // no need to get fpath on this one, since I work from fi->fh not the path
    log_fi(fi);
	
    return 0;
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int fcfuse_release(const char *path, struct fuse_file_info *fi)
{
    log_msg("\nfcfuse_release(path=\"%s\", fi=0x%08x)\n",
	  path, fi);
    log_fi(fi);

    // We need to close the file.  Had we allocated any resources
    // (buffers etc) we'd need to free them here as well.
    return log_syscall("close", close(fi->fh), 0);
}

/** Synchronize file contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data.
 *
 * Changed in version 2.2
 */
int fcfuse_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
    log_msg("\nfcfuse_fsync(path=\"%s\", datasync=%d, fi=0x%08x)\n",
	    path, datasync, fi);
    log_fi(fi);
    
    // some unix-like systems (notably freebsd) don't have a datasync call
#ifdef HAVE_FDATASYNC
    if (datasync)
	return log_syscall("fdatasync", fdatasync(fi->fh), 0);
    else
#endif	
	return log_syscall("fsync", fsync(fi->fh), 0);
}

#ifdef HAVE_SYS_XATTR_H
/** Set extended attributes */
int fcfuse_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
    return -61;
}

/** Get extended attributes */
int fcfuse_getxattr(const char *path, const char *name, char *value, size_t size)
{
    return -61;
}

/** List extended attributes */
int fcfuse_listxattr(const char *path, char *list, size_t size)
{
    return -61;
}

/** Remove extended attributes */
int fcfuse_removexattr(const char *path, const char *name)
{
    return -61;
}
#endif

/** Open directory
 *
 * This method should check if the open operation is permitted for
 * this directory
 *
 * Introduced in version 2.3
 */
int fcfuse_opendir(const char *path, struct fuse_file_info *fi)
{
    DIR *dp;
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nfcfuse_opendir(path=\"%s\", fi=0x%08x)\n",
	  path, fi);
    fcfuse_fullpath(fpath, path);
    fprintf(stderr,"PID: %d\n", getpid());
    // since opendir returns a pointer, takes some custom handling of
    // return status.
    dp = opendir(fpath);
    log_msg("    opendir returned 0x%p\n", dp);
    if (dp == NULL)
	retstat = log_error("fcfuse_opendir opendir");
    
    fi->fh = (intptr_t) dp;
    
    log_fi(fi);
    
    return retstat;
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */

int fcfuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	       struct fuse_file_info *fi)
{
    int retstat = 0;
    DIR *dp;
    struct dirent *de;
    
    log_msg("\nfcfuse_readdir(path=\"%s\", buf=0x%08x, filler=0x%08x, offset=%lld, fi=0x%08x)\n",
	    path, buf, filler, offset, fi);
    // once again, no need for fullpath -- but note that I need to cast fi->fh
    dp = (DIR *) (uintptr_t) fi->fh;

    // Every directory contains at least two entries: . and ..  If my
    // first call to the system readdir() returns NULL I've got an
    // error; near as I can tell, that's the only condition under
    // which I can get an error from readdir()
    de = readdir(dp);
    log_msg("    readdir returned 0x%p\n", de);
    if (de == 0) {
	retstat = log_error("fcfuse_readdir readdir");
	return retstat;
    }

    // This will copy the entire directory into the buffer.  The loop exits
    // when either the system readdir() returns NULL, or filler()
    // returns something non-zero.  The first case just means I've
    // read the whole directory; the second means the buffer is full.
    do {
	log_msg("calling filler with name %s\n", de->d_name);
	if (filler(buf, de->d_name, NULL, 0) != 0) {
	    log_msg("    ERROR fcfuse_readdir filler:  buffer full");
	    return -ENOMEM;
	}
    } while ((de = readdir(dp)) != NULL);
    
    log_fi(fi);
    
    return retstat;
}

/** Release directory
 */
int fcfuse_releasedir(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    log_msg("\nfcfuse_releasedir(path=\"%s\", fi=0x%08x)\n",
	    path, fi);
    log_fi(fi);
    
    closedir((DIR *) (uintptr_t) fi->fh);
    
    return retstat;
}

/** Synchronize directory contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data
 *
 * Introduced in version 2.3
 */
// when exactly is this called?  when a user calls fsync and it
// happens to be a directory? ??? 
int fcfuse_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    log_msg("\nfcfuse_fsyncdir(path=\"%s\", datasync=%d, fi=0x%08x)\n",
	    path, datasync, fi);
    log_fi(fi);
    
    return retstat;
}

int fcfuse_access(const char *path, int mask)
{
    int retstat = 0;
    char fpath[PATH_MAX];
   
    log_msg("\nfcfuse_access(path=\"%s\", mask=0%o)\n",
	    path, mask);
    fcfuse_fullpath(fpath, path);
    
    retstat = access(fpath, mask);
    
    if (retstat < 0)
	retstat = log_error("fcfuse_access access");
    
    return retstat;
}

/**
 * Change the size of an open file
 *
 * This method is called instead of the truncate() method if the
 * truncation was invoked from an ftruncate() system call.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the truncate() method will be
 * called instead.
 *
 * Introduced in version 2.5
 */
int fcfuse_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    log_msg("\nfcfuse_ftruncate(path=\"%s\", offset=%lld, fi=0x%08x)\n",
	    path, offset, fi);
    log_fi(fi);
    
    retstat = ftruncate(fi->fh, offset);
    if (retstat < 0)
	retstat = log_error("fcfuse_ftruncate ftruncate");
    
    return retstat;
}

/**
 * Get attributes from an open file
 *
 * This method is called instead of the getattr() method if the
 * file information is available.
 *
 * Currently this is only called after the create() method if that
 * is implemented (see above).  Later it may be called for
 * invocations of fstat() too.
 *
 */
int fcfuse_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi)
{
    int retstat = 0;
    
    log_msg("\nfcfuse_fgetattr(path=\"%s\", statbuf=0x%08x, fi=0x%08x)\n",
	    path, statbuf, fi);
    log_fi(fi);

    // On FreeBSD, trying to do anything with the mountpoint ends up
    // opening it, and then using the FD for an fgetattr.  So in the
    // special case of a path of "/", I need to do a getattr on the
    // underlying root directory instead of doing the fgetattr().
    if (!strcmp(path, "/"))
	return fcfuse_getattr(path, statbuf);
    
    retstat = fstat(fi->fh, statbuf);
    if (retstat < 0)
	retstat = log_error("fcfuse_fgetattr fstat");
    
    log_stat(statbuf);
    
    return retstat;
}

void *fcfuse_init(struct fuse_conn_info *conn)
{
    log_msg("\nfcfuse_init()\n");
    log_conn(conn);
    log_fuse_context(fuse_get_context());
        
    return FCFS_DATA;
}

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void fcfuse_destroy(void *userdata)
{
    log_msg("\nfcfuse_destroy(userdata=0x%08x)\n", userdata);
}
