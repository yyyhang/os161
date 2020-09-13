#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/seek.h>
#include <lib.h>
#include <uio.h>
#include <thread.h>
#include <current.h>
#include <synch.h>
#include <vfs.h>
#include <vnode.h>
#include <file.h>
#include <syscall.h>
#include <copyinout.h>
#include <proc.h>


/*
 * Add your file-related functions here ...
 */

// it will be called at runprogram.c to initialize
int of_table_init(void){
    int result;
    struct vnode *vn = NULL;

    if (oft == NULL){
        oft = kmalloc(sizeof(struct of_table));
        if (oft == NULL) return ENOMEM;
    } 

    oft-> oftlock = lock_create("oftlock");
    if (oft->oftlock == NULL) {
        kfree(oft);
        return ENOMEM;
    }

    // three standard files
    char c0[] = "con:";
    struct of_entry *ent_rec = kmalloc(sizeof(struct of_entry));
    if (ent_rec == NULL) return ENOMEM;

    ent_rec->s_flags = O_RDONLY;
    ent_rec->ref_cnt = 1;
    ent_rec->offset = 0;
    ent_rec->vn = vn;

    result = vfs_open (c0, ent_rec->s_flags, 0, &ent_rec->vn);
    if(result) return result;

    oft->fp[0] = ent_rec;

    char c1[] = "con:";
    struct of_entry *ent_rec1 = kmalloc(sizeof(struct of_entry));
    if (ent_rec1 == NULL) return ENOMEM;

    ent_rec1->s_flags = O_WRONLY;
    ent_rec1->ref_cnt = 1;
    ent_rec1->offset = 0;
    ent_rec1->vn = vn;

    result = vfs_open (c1, ent_rec1->s_flags, 0, &ent_rec1->vn);
    if(result) return result;

    oft->fp[1] = ent_rec1;

    char c2[] = "con:";
    struct of_entry *ent_rec2 = kmalloc(sizeof(struct of_entry));
    if (ent_rec2 == NULL) return ENOMEM;

    ent_rec2->s_flags = O_WRONLY;
    ent_rec2->ref_cnt = 1;
    ent_rec2->offset = 0;
    ent_rec2->vn = vn;

    result = vfs_open (c2, ent_rec2->s_flags, 0, &ent_rec2->vn);
    if(result) return result;

    oft->fp[2] = ent_rec2;

    return 0;
}

int fd_table_init(void){
    curproc->fdt = kmalloc(sizeof(struct fd_table));
    if (curproc->fdt == NULL) return ENOMEM;
    int i;
    //initialize this fd table
    for (i=3; i<OPEN_MAX; i++) {
        curproc->fdt->fd[i] = NULL;
    }
    ///point to files stdin, stdout, stderr
    curproc->fdt->fd[0] = oft->fp[0];
    curproc->fdt->fd[1] = oft->fp[1];
    curproc->fdt->fd[2] = oft->fp[2];
    return 0;
}

int sys_open(userptr_t filename, int flags, mode_t mode, int *retv){
    
    int result;
    char fname[NAME_MAX];

    //we are not sure if the string from the user mode will exceed the buffer or not, that's why we need copyinstr()
    result = copyinstr(filename, fname, NAME_MAX, NULL);  
    if (result) return result;

    struct vnode *vn;
    result = vfs_open(fname, flags, mode, &vn);
    if (result) return result;   
    
    lock_acquire(oft->oftlock);

    int fd;
    for (fd=3; fd<OPEN_MAX; fd++){
        if (curproc->fdt->fd[fd] == NULL) break;
    }
    // find vaild place to create fd

    int fpn;
    for (fpn = 3; fpn<OPEN_MAX; fpn++){
        if (oft->fp[fpn] == NULL) break;
    }
    // find valid place to create open file table entry

    if (fd >= OPEN_MAX || fpn >= OPEN_MAX) {
        lock_release(oft->oftlock);
        return EMFILE;}
    //full

    //create file handle
    struct of_entry *ent_rec = kmalloc(sizeof(struct of_entry));
    if (ent_rec == NULL) {
        lock_release(oft->oftlock);
        return ENOMEM;
    }
    ent_rec->s_flags = flags;
    ent_rec->offset = 0;
    ent_rec->ref_cnt = 1;
    ent_rec->vn = vn;

    oft -> fp[fpn] = ent_rec;  
    curproc->fdt->fd[fd] = oft -> fp[fpn];

    lock_release(oft->oftlock);     
    *retv = fd;
    return 0;
}


int sys_read(int fd, void *buf, size_t buflen, int *retv){

    if (fd < 0 || fd >= OPEN_MAX ) return EBADF;
    struct of_entry *entry = curproc->fdt->fd[fd];
    if (entry == NULL) return EBADF;
    if (buf == NULL) return EFAULT; 
    if (entry->s_flags == O_WRONLY) return EACCES;

    int result;
    struct uio ui;
    struct iovec io;

    lock_acquire(oft->oftlock);

    uio_kinit(&io, &ui, buf, buflen,entry->offset, UIO_READ);
    result = VOP_READ(entry -> vn, &ui);
    if (result) {
        lock_release(oft->oftlock);
        return result;
    }
    int readcnt = buflen - ui.uio_resid;
    *retv = readcnt;
    entry->offset = ui.uio_offset;
    lock_release(oft->oftlock);
    return 0;
}

int sys_write(int fd, void *buf, size_t nbytes, int *retv){

    if (fd < 0 || fd >= OPEN_MAX ) return EBADF;
    struct of_entry *entry = curproc->fdt->fd[fd];
    if (entry == NULL) return EBADF;
    if (buf == NULL) return EFAULT; 
    if (entry->s_flags == O_RDONLY) return EACCES;

    int result;
    struct uio ui;
    struct iovec io;


    lock_acquire(oft -> oftlock);
    uio_kinit(&io, &ui, buf, nbytes, entry->offset, UIO_WRITE);
    result = VOP_WRITE(entry-> vn, &ui);
    if (result) {
        lock_release(oft->oftlock);
        return result;
    }
    int writtencnt = nbytes - ui.uio_resid;
    *retv = writtencnt;
    entry->offset = ui.uio_offset;
    lock_release(oft->oftlock);
    return 0;
}

off_t sys_lseek(int fd, off_t pos, int whence, int *retv){

    if (fd < 0 || fd >= OPEN_MAX ) return EBADF;
    struct of_entry *entry = curproc->fdt->fd[fd];
    if (entry == NULL) return EBADF;
    if(!VOP_ISSEEKABLE(entry->vn)) return ESPIPE;

    int result;
    struct stat st;
    
    lock_acquire(oft -> oftlock);
    switch(whence) {
        case SEEK_SET:
            result= pos;
            break;
        case SEEK_CUR:
            result = entry->offset + pos;
            break;
        case SEEK_END:
            result = VOP_STAT(entry->vn,&st);
            if (result){
                lock_release(oft->oftlock);
                return result;
            }
            result = st.st_size + pos;
            break;
        //whence is invalid or the resulting seek position would be negative
        default:
            lock_release(oft->oftlock);
            return EINVAL;
    }
    entry->offset = result;
    lock_release(oft->oftlock);
    *retv = result;
    return 0;
}

int sys_close(int fd) {

    if (fd < 0 || fd >= OPEN_MAX ) return EBADF;
    struct of_entry *entry = curproc->fdt->fd[fd];
    if (entry == NULL) return EBADF;

	lock_acquire(oft -> oftlock);

	if (entry-> ref_cnt == 0) {
        vfs_close(entry->vn);
        entry->ref_cnt--;

        lock_release(oft->oftlock);

        lock_destroy(oft -> oftlock);

        kfree(entry);

        entry = NULL;
        return 0;
    }
    entry->ref_cnt--;
	lock_release(oft->oftlock);
	return 0;
}

int sys_dup2(int oldfd, int newfd, int *retv){
    //int fpn = curproc->fdt->fd[oldfd];
    if (oldfd < 0 || oldfd >= OPEN_MAX ) return EBADF;
    struct of_entry *entry = curproc->fdt->fd[oldfd];
    if (entry == NULL) return EBADF;
    if (newfd < 0 || newfd >= OPEN_MAX ) return EBADF;
    if (newfd == oldfd) return newfd;  

    lock_acquire(oft -> oftlock);
    curproc->fdt->fd[newfd] = entry;
    entry->ref_cnt++;
    lock_release(oft -> oftlock);
    *retv = newfd;
    return 0;
}
