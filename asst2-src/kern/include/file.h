/*
 * Declarations for file handle and file table management.
 */

#ifndef _FILE_H_
#define _FILE_H_

/*
 * Contains some file-related maximum length constants
 */
#include <limits.h>


/*
 * Put your function declarations and data types here ...
 */

struct of_entry{
    int s_flags;             //status flags
    off_t offset;
    int ref_cnt;
    struct vnode *vn;
};

struct of_table{
    struct of_entry *fp[OPEN_MAX];
    struct lock *oftlock;     
    // set up a lock for this system-wide table, in case multiple processes modify same entry
};

struct of_table *oft;

struct fd_table{
    struct of_entry *fd[OPEN_MAX];
};

int of_table_init(void);
int fd_table_init(void);
int sys_open(userptr_t filename, int flags, mode_t mode, int *retv);
int sys_read(int fd, void *buf, size_t buflen, int *retv);
int sys_write(int fd, void *buf, size_t nbytes, int *retv);
off_t sys_lseek(int fd, off_t pos, int whence, int *retv);
int sys_close(int fd);
int sys_dup2(int oldfd, int newfd, int *retv);

#endif /* _FILE_H_ */
