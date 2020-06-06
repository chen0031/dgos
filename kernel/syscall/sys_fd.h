#pragma once

#include "dirent.h"
#include "dev_storage.h"

extern "C" {

typedef uint32_t socklen_t;

ssize_t sys_read(int fd, void *bufaddr, size_t count);
ssize_t sys_write(int fd, void const *bufaddr, size_t count);
int sys_close(int fd);
off_t sys_lseek(int fd, off_t ofs, int whence);
ssize_t sys_pread64(int fd, void *bufaddr,
                    size_t count, off_t ofs);
ssize_t sys_pwrite64(int fd, void const *bufaddr,
                     size_t count, off_t ofs);
int sys_readdir_r(int fd, dirent_t *buf);
int sys_opendirat(int dirfd, char const *pathname);
int sys_closedir(int fd);
int sys_fsync(int fd);
int sys_fdatasync(int fd);
int sys_dup(int oldfd);
int sys_dup2(int oldfd, int newfd);
int sys_dup3(int oldfd, int newfd, int flags);
int sys_ftruncate(int fd, off_t size);
int sys_ioctl(int fd, int cmd, void* arg);
int sys_faccess(int fd, int mask);
int sys_openat(int dirfd, char const *pathname, int flags, mode_t mode);
int sys_creatat(int dirfd, char const *pathname, mode_t mode);
int sys_renameat(int olddirfd, char const *old_pathname,
                 int newdirfd, char const *new_pathname);
int sys_mkdirat(int dirfd, char const *path, mode_t mode);
int sys_rmdirat(int dirfd, char const *path);
int sys_unlinkat(int dirfd, char const *path);
int sys_truncateat(int dirfd, char const *path, off_t size);
int sys_accessat(int dirfd, char const *path, int mask);

int sys_mknodat(int dirfd, char const *path, mode_t mode, int rdev);

int sys_linkat(int fromdirfd, char const *from,
               int todirfd, char const *to);
int sys_chmodat(int dirfd, char const *path, mode_t mode);
int sys_fchmod(int fd, mode_t mode);
int sys_chownat(int dirfd, char const *path, int uid, int gid);
int sys_fchown(int fd, int uid, int gid);

int sys_setxattrat(int dirfd, char const *path,
                   char const* name, char const* value,
                   size_t size, int flags);
int sys_getxattrat(int dirfd, char const *path,
                   char const* name, char* value, size_t size);
int sys_listxattrat(int dirfd, char const *path,
                    char const* list, size_t size);
int sys_readlinkat(int dirfd, char const *path,
                   int mask);

//int opendir(fs_file_info_t **fi, fs_cpath_t path) final;
//ssize_t readdir(fs_file_info_t *fi, dirent_t* buf,
//                off_t offset) final;
//int releasedir(fs_file_info_t *fi) final;
//int getattr(fs_cpath_t path, fs_stat_t* stbuf) final;
//int readlink(fs_cpath_t path, char* buf, size_t size) final;
//int symlink(fs_cpath_t to, fs_cpath_t from) final;
//int utimens(fs_cpath_t path,
//       fs_timespec_t const *ts) final;
//int release(fs_file_info_t *fi) final;
//int fstat(fs_file_info_t *fi,
//     fs_stat_t *st) final;
//int fsyncdir(fs_file_info_t *fi,
//        int isdatasync) final;
//int flush(fs_file_info_t *fi) final;
//int lock(fs_file_info_t *fi,
//    int cmd, fs_flock_t* locks) final;
//int bmap(fs_cpath_t path, size_t blocksize,
//    uint64_t* blockno) final;
//int statfs(fs_statvfs_t* stbuf) final;
//int poll(fs_file_info_t *fi,
//    fs_pollhandle_t* ph, unsigned* reventsp) final;

}
int sys_socket(int domain, int type, int protocol);
int sys_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int sys_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
ssize_t sys_send(int sockfd, void const *buf, size_t len, int flags);
ssize_t sys_sendto(int sockfd, void const *buf, size_t len, int flags,
                   struct sockaddr const *dest_addr, socklen_t addrlen);
ssize_t sys_recv(int sockfd, void *buf, size_t len, int flags);
ssize_t sys_recvfrom(int sockfd, void *buf, size_t len, int flags,
                     struct sockaddr *src_addr, socklen_t *addrlen);
int sys_shutdown(int sockfd, int how);
int sys_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int sys_listen(int sockfd, int backlog);
char *sys_getcwd(char *buf, size_t size);

int sys_fcntl(int fd, int cmd, void *arg);

int sys_statfs(char const *path, fs_statvfs_t *buf);
int sys_fstatfs(int fd, fs_statvfs_t *buf);
