/* 
 * rfsdelta.c
 * kernel module for 2.6.x kernels (tested with 2.6.18.1)
 * modified by pts@fazekas.hu at Thu Jan 11 15:11:40 CET 2007
 *
 * based on rlocate.c (in rlocate-0.5.5)
 * Copyright (C) 2004 Rasto Levrinc.
 *
 * Parts of the LSM implementation were taken from
 * (http://www.logic.at/staff/robinson/)     
 * Copyright (C) 2004 by Peter Robinson
 *   
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/version.h>
#define __NO_VERSION__
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/poll.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/moduleparam.h>

#include <linux/security.h>
#include <linux/init.h>
#include <linux/rwsem.h>
#include <linux/spinlock.h>
#include <linux/device.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
#include <linux/devfs_fs_kernel.h>
#endif

#include <linux/namei.h>
#include <linux/jiffies.h>

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,12)
#define class_create class_simple_create
#define class_destroy class_simple_destroy
#define class_device_create(a, b, c, d, e) class_simple_device_add(a, c, d, e);
#define class_device_destroy(a, b) class_simple_device_remove(b)
static struct class_simple *rfsdelta_class;
#else

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,15)
#define class_device_create(a, b, c, d, e) class_device_create(a, c, d, e); 
#endif

static struct class *rfsdelta_class;
#endif

MODULE_AUTHOR("Peter Szabo");
MODULE_DESCRIPTION("rfsdelta "RFSDELTA_VERSION" recursive filesystem change notify");
MODULE_LICENSE("GPL");

#ifdef CONFIG_SECURITY
#else
#  error please enable CONFIG_SECURITY
#endif
#ifdef CONFIG_SECURITY_CAPABILITIES
#  error please disable (or modularize) CONFIG_SECURITY_CAPABILITIES
#endif

/**** pts ****/
#undef  SUPPORT_UPDATEDB_ARG
#define SUPPORT_UPDATEDB_ARG 0
#undef  SUPPORT_OUTPUT_ARG
#define SUPPORT_OUTPUT_ARG 0
#undef  SUPPORT_UPDATES
#define SUPPORT_UPDATES 0
#undef  SUPPORT_EXCLUDEDIR
/** Just a setting, not used by the kernel module. */
#define SUPPORT_EXCLUDEDIR 0
#undef  SUPPORT_BROKEN_STACKING
/** Doesn't work yet, `return 0' is not enough. */
#define SUPPORT_BROKEN_STACKING 0

#define DEVICE_NAME     "rfsdelta-event"
MODULE_SUPPORTED_DEVICE(DEVICE_NAME)

/* 30 minutes timeout for daemon */
#define D_TIMEOUT       (60*30*HZ) 

#ifndef SETPROC_OPS
#define SETPROC_OPS(entry, ops) (entry)->proc_fops = &(ops)
#endif

/* string list */
struct string_list {
        struct list_head list;
        char *string;
};

inline static void add_string_entry(struct list_head *string_list_head, 
                                    const char *string, const char *prefix);
inline static void remove_string_entry(struct string_list *string_entry);
inline static void remove_string_list(const struct list_head *string_list_head);
inline static void add_filenames_entry(char const *path, const char *prefix);

static rwlock_t proc_lock = RW_LOCK_UNLOCKED;

LIST_HEAD(filenames_list); 	      /* list of filenames with leading 
				 	 'm' or 'a' */
#if SUPPORT_EXCLUDEDIR
static char *EXCLUDE_DIR_ARG;         /* excluded directories */
#endif
static char *STARTING_PATH_ARG;       /* starting at path */
#if SUPPORT_OUTPUT_ARG
static char *OUTPUT_ARG;              /* default database */
#endif
/**** pts ****/ /* default set to ACTIVATED_ARG = 'r' */
static char ACTIVATED_ARG = 'r';      /* activated 0 - not running, 
				                   1 - running 
				                   r - running if have reader
                                                   d - disabled */
#if SUPPORT_UPDATEDB_ARG
static unsigned char UPDATEDB_ARG = 0;/* updatedb counter. If it reaches 0, 
					 full update will be performed */
#endif
static unsigned long NEXT_D_TIMEOUT;  /* next daemon timeout */

static unsigned char RELOAD = '0';    /* Will be written as first line in the
					 dev file. It will be set to '1' if
					 mount or umount was called. */


/* device file */
static DECLARE_MUTEX(dev_mutex);


static int rfsdelta_dev_register( void );
static void rfsdelta_dev_unregister( void );
static int rfsdelta_dev_open( struct inode *, struct file * );
static int rfsdelta_dev_release( struct inode *, struct file * );
static loff_t rfsdelta_dev_llseek( struct file *filp, loff_t offset, int whence);
static int rfsdelta_dev_ioctl( struct inode *inode, struct file *filp, 
                              unsigned int cmd, unsigned long arg );
ssize_t rfsdelta_dev_write( struct file *filp, const char *buff, size_t count, 
                           loff_t *offp );
static ssize_t rfsdelta_dev_read( struct file *, char *, size_t, loff_t * );
static unsigned int rfsdelta_dev_poll( struct file *filp, poll_table *wait);

static int major=0;
module_param(major, int, 0400);
MODULE_PARM_DESC(major, "major char device to use"); /**** pts ****/
static int using_major=0;           /* holds major number from device */
static char *path_buffer;   /* path_buffer for d_path call */

static struct file_operations device_fops = {
        .owner = THIS_MODULE,
        .open           = rfsdelta_dev_open,
        .llseek         = rfsdelta_dev_llseek,
        .ioctl          = rfsdelta_dev_ioctl,
        .write          = rfsdelta_dev_write,
        .read           = rfsdelta_dev_read,
        .release        = rfsdelta_dev_release,
        .poll           = rfsdelta_dev_poll,
};

/* lsm hooks */
static int rfsdelta_inode_create( struct inode *dir, struct dentry *dentry, 
                                 int mode);
static int rfsdelta_inode_mkdir( struct inode * dir, struct dentry * dentry, 
                                int mode );
static int rfsdelta_inode_rmdir( struct inode * dir, struct dentry * dentry);
static int rfsdelta_inode_unlink( struct inode * dir, struct dentry * dentry);
static int rfsdelta_inode_link ( struct dentry * old_dentry, struct inode * dir, 
                                struct dentry * dentry);
static int rfsdelta_inode_symlink ( struct inode * dir, struct dentry * dentry, 
                                   const char *old_name);
static int rfsdelta_inode_mknod ( struct inode * dir, struct dentry * dentry,
                                 int mode, dev_t dev);
static int rfsdelta_inode_rename( struct inode * old_dir,
                                 struct dentry * old_dentry,
                                 struct inode * new_dir,
                                 struct dentry * new_dentry);

static int rfsdelta_sb_mount( char *dev_name, 
			     struct nameidata *nd, 
			     char *type, 
			     unsigned long flags, 
			     void *data);

static int rfsdelta_sb_umount( struct vfsmount *mnt, int flags );

#if SUPPORT_UPDATES
static int rfsdelta_inode_permission (struct inode *inode, int mask, 
                                     struct nameidata *nd);
#endif

#if SUPPORT_BROKEN_STACKING
static int rfsdelta_register_security(const char *name,
                                      struct security_operations *ops);
static int rfsdelta_unregister_security(const char *name,
                                      struct security_operations *ops);
#endif

//static wait_queue_head_t filenames_wq;
DECLARE_WAIT_QUEUE_HEAD(filenames_wq);
static DEFINE_SPINLOCK(dev_lock);
static int dev_is_open = 0;

static struct security_operations rfsdelta_security_ops = {
        .inode_create =                 rfsdelta_inode_create,
        .inode_mkdir  =                 rfsdelta_inode_mkdir,
        .inode_rmdir  =                 rfsdelta_inode_rmdir,
        .inode_unlink =                 rfsdelta_inode_unlink,
        .inode_link   =                 rfsdelta_inode_link,
        .inode_symlink=                 rfsdelta_inode_symlink,
        .inode_mknod  =                 rfsdelta_inode_mknod,
        .inode_rename =                 rfsdelta_inode_rename,
	.sb_mount     =			rfsdelta_sb_mount,
	.sb_umount    =			rfsdelta_sb_umount,
#if SUPPORT_UPDATES
        .inode_permission =             rfsdelta_inode_permission,
#endif
#if SUPPORT_BROKEN_STACKING
        .register_security =            rfsdelta_register_security,
        .unregister_security =          rfsdelta_unregister_security,
#endif
};

/* proc fs */
enum Option { NOTSET, UNKNOWN, VERSION, EXCLUDE_DIR, ACTIVATED, STARTING_PATH, 
              OUTPUT, UPDATEDB, DEVNUMBERS };
#define LINES_IN_PROC 7

/* proc_data is used for reading and writing to the proc fs */
struct proc_data {
        char        *pbuffer;
        int         pbuffer_pos;
        int         read_line;
        enum Option option;

};

static int proc_rfsdelta_open( struct inode *inode, struct file *file);
static ssize_t proc_rfsdelta_read( struct file *file,
                                  char *buffer,
                                  size_t len,
                                  loff_t *offset);
static void parse_proc(struct proc_data *pdata);
static ssize_t proc_rfsdelta_write( struct file *file,
                                   const char *buffer,
                                   size_t len,
                                   loff_t *offset );
static int proc_rfsdelta_close( struct inode *inode, struct file *file);

static struct file_operations proc_rfsdelta_ops = {
        .open    = proc_rfsdelta_open,
        .read    = proc_rfsdelta_read,
        .write   = proc_rfsdelta_write,
        .release = proc_rfsdelta_close,
};



/* --------------------------- Functions ----------------------------------- */

/* string_list functions *****************************************************/

/*
 * add_string_entry() adds 'string' to the string_list.
 */
inline void add_string_entry(struct list_head *string_list_head, 
                             const char *string, const char *prefix)
{
        struct string_list *s;
        size_t prefix_len=strlen(prefix);
#if 0
        printk(KERN_ERR "rfsdelta: add_string_entry string=(%s)\n", string);
#endif
        if ( (s = kmalloc(sizeof(struct string_list), GFP_KERNEL)) == NULL) {
                printk(KERN_ERR "rfsdelta: add_string_entry:"
                                " memory allocation error.\n");
        } else {
                if ((s->string = 
                     kmalloc(strlen(string)+prefix_len+1, GFP_KERNEL)) == NULL) {
			printk(KERN_ERR "rfsdelta: add_string_entry: "
                                        "memory allocation error.\n");
                        kfree(s);
		} else {
		        strcpy(s->string, prefix); /**** pts ****/
			strcpy(s->string+prefix_len, string);
                        list_add_tail(&s->list, string_list_head);
		}
                
        }
}

/*
 * remove_string_entry() removes the STRING_ENTRY from string list and 
 * frees the memory.
 */
inline static void remove_string_entry(struct string_list *string_entry)
{
        list_del(&string_entry->list);
        kfree(string_entry->string);
        kfree(string_entry);
}

/*
 * remove_string_list() loops over the string list with specified list_head 
 * and removes all entries in the list.
 */
inline static void remove_string_list(const struct list_head *string_list_head)
{
        while (!list_empty(string_list_head)) {
                struct string_list *entry;
                entry = list_entry(string_list_head->next,
                                   struct string_list,
                                   list);
                remove_string_entry(entry);
        }
}

/*
 * add_filenames_entry() inserts PATH, that was created in the filesystem, to 
 * the filenames list. First character of path is 'a' or 'm', depending on if 
 * the filename was added or moved.
 */
inline static void add_filenames_entry(char const *path, char const *prefix) 
{

        // check path against STARTING_PATH_ARG
	// not doing in deamon for now

        if (*STARTING_PATH_ARG != '\0') {
		int starting_path_len = strlen(STARTING_PATH_ARG);
		int path_len = strlen(path); // -1 without first character
		if (starting_path_len>path_len+1 ||
		    strncmp(STARTING_PATH_ARG, path,
		            (starting_path_len==path_len+1 ? /* Dat: ignore trailing `/' if long enough */
			     			path_len:starting_path_len)))
                        return; // this path is not in starting path
        }
        add_string_entry(&filenames_list, path, prefix);
        wake_up_interruptible( &filenames_wq );
        /* ^^^ SUXX: having two processes reading /dev/rfsdelta, Ctrl-<C> is
         *     ignored in the 2nd one -- anyway we have only 1 queue, so
         *     we'll return EBUSY for the 2nd open().
         */
        return;
}

/* proc fs functions *********************************************************/

/* 
 * rfsdelta_init_fs() creates /proc/rfsdelta entry.
 */
static int rfsdelta_init_procfs(void)
{
        struct proc_dir_entry *p;

        p = create_proc_entry("rfsdelta", 00600, NULL);
        if (!p)
                return -ENOMEM;
        p->owner = THIS_MODULE;
        SETPROC_OPS(p, proc_rfsdelta_ops);
        
        /**** pts ****/
       	p = create_proc_entry("rfsdelta-event", 00600, NULL); /* Dat: root only */
        if (!p)
                return -ENOMEM;
        p->owner = THIS_MODULE;
        SETPROC_OPS(p, device_fops);
        return 0;
}

/*
 * proc_rfsdelta_open() initializes pdata structure.
 */
static int proc_rfsdelta_open( struct inode *inode, struct file *file) {
        struct proc_data *pdata;
        int ret;
        if ((file->private_data = kmalloc(sizeof(struct proc_data), 
                                          GFP_KERNEL)) == NULL) {
                ret = -ENOMEM;
                goto out;
        }
        memset(file->private_data, 0, sizeof(struct proc_data)); 
        pdata = (struct proc_data *)file->private_data;
        pdata->pbuffer = (char*)__get_free_page( GFP_KERNEL );
        if (!pdata->pbuffer) {
                ret = -ENOMEM;
                goto no_pbuffer;
        }
        pdata->option      = NOTSET;
        pdata->read_line   = 0;
        pdata->pbuffer_pos = 0;
        ret = 0;
        goto out;

no_pbuffer:
        kfree(file->private_data);
out:
        return ret;
}

/* 
 * proc_rfsdelta_close()  
 */
static int proc_rfsdelta_close( struct inode *inode, struct file *file)
{
        struct proc_data *pdata = (struct proc_data*)file->private_data;
        if (pdata->option!=NOTSET) {
                // in case there was no new line by writing
                pdata->pbuffer[pdata->pbuffer_pos]='\n';
                parse_proc(pdata);
        }
        free_page( (unsigned long)pdata->pbuffer);
        kfree(pdata);
        return 0;
}

/* 
 * proc_rfsdelta_read() prints out all the options with their arguments (one 
 * argument at a time) to the read buffer.
 */
static ssize_t proc_rfsdelta_read( struct file *file,
                                  char *buffer,
                                  size_t len,
                                  loff_t *offset)
{
        int i;
        char p;
        struct proc_data *pdata = (struct proc_data*)file->private_data;
        if( !pdata->pbuffer ) return -EINVAL;
        for (i = 0; i<len; i++) {
                if (pdata->pbuffer_pos == 0) { // beginning of the line
                        if (pdata->option == NOTSET) {
                                switch(pdata->read_line)
                                {
                                case 0:
                                        sprintf(pdata->pbuffer, "version: ");
                                        pdata->option = VERSION;
                                        break;
#if SUPPORT_EXCLUDEDIR
                                case 1:
                                        sprintf(pdata->pbuffer, "excludedir: ");
                                        pdata->option = EXCLUDE_DIR;
                                        break;
#endif
                                case 2:
                                        sprintf(pdata->pbuffer, "activated: ");
                                        pdata->option = ACTIVATED;
                                        break;
                                case 3:
                                        sprintf(pdata->pbuffer, "startingpath: ");
                                        pdata->option = STARTING_PATH;
                                        break;
#if SUPPORT_OUTPUT_ARG
                                case 4:
                                        sprintf(pdata->pbuffer, "output: ");
                                        pdata->option = OUTPUT;
                                        break;
#endif
#if SUPPORT_UPDATEDB_ARG
                                case 5:
                                        sprintf(pdata->pbuffer, "updatedb: ");
                                        pdata->option = UPDATEDB;
                                        break;
#endif
                                case 6:
                                        sprintf(pdata->pbuffer, "devnumbers: ");
                                        pdata->option = DEVNUMBERS;
                                        break;
                                default: /**** pts ****/
                                        pdata->read_line++;
                                        pdata->option=NOTSET;
                                        goto next_line;
                                }
                        }
                }
                p = pdata->pbuffer[pdata->pbuffer_pos];
                if (p == '\0') { 
                        if (pdata->pbuffer[pdata->pbuffer_pos-1] == '\n') {
                                pdata->option = NOTSET;
                        
                                pdata->read_line++;
                                if (pdata->read_line>=LINES_IN_PROC) {
                                        if (put_user( '\0', buffer+i ))
                                                return -EFAULT;
                                        break;
                                }
                        }
                        pdata->pbuffer_pos = 0;
                        switch(pdata->option) 
                        {
                        case VERSION:
                                sprintf(pdata->pbuffer, "%s\n", RFSDELTA_VERSION);
                                break;
                        case EXCLUDE_DIR:
#if SUPPORT_EXCLUDEDIR
                                read_lock(&proc_lock);
                                sprintf(pdata->pbuffer, "%s\n", 
                                                EXCLUDE_DIR_ARG);
                                read_unlock(&proc_lock);
#endif
                                break;
                        case ACTIVATED:
                                read_lock(&proc_lock);
                                sprintf(pdata->pbuffer, "%c\n", 
                                                ACTIVATED_ARG);
                                read_unlock(&proc_lock);
                                break;
                        case STARTING_PATH:
                                read_lock(&proc_lock);
                                sprintf(pdata->pbuffer, "%s\n", 
						STARTING_PATH_ARG);
                                read_unlock(&proc_lock);
                                break;
                        case OUTPUT:
#if SUPPORT_OUTPUT_ARG /* Dat: pacify gcc `case not handled' warning */
                                read_lock(&proc_lock);
                                sprintf(pdata->pbuffer, "%s\n", 
                                                OUTPUT_ARG);
                                read_unlock(&proc_lock);
#endif
                                break;
                        case UPDATEDB:
#if SUPPORT_UPDATEDB_ARG /* Dat: pacify gcc `case not handled' warning */
                                read_lock(&proc_lock);
                                sprintf(pdata->pbuffer, "%i\n", 
                                                UPDATEDB_ARG);
                                read_unlock(&proc_lock);
#endif
                                break;
                        case DEVNUMBERS: /**** pts ****/
                                read_lock(&proc_lock);
                                sprintf(pdata->pbuffer, "c %u %u\n", using_major, 0);
                                read_unlock(&proc_lock);
                                break;
                        case NOTSET:
                        case UNKNOWN:
                                break;
                        }
                        i--; // remove '\0'
                } else {
                        if (put_user( p, buffer+i ))
                                return -EFAULT;
                        pdata->pbuffer_pos++;
                }
              next_line: ;
        }
        *offset += i;
        return i;
}

/*
 * proc_set_option()
 */
inline void proc_set_option(const char *option_string, 
                            struct proc_data *pdata) 
{
	if (0) {
#if SUPPORT_EXCLUDEDIR
        } else if (!strcmp(option_string, "excludedir")) {
                pdata->option   = EXCLUDE_DIR;
#endif
        } else if (!strcmp(option_string, "activated")) {
                pdata->option   = ACTIVATED;
        } else if (!strcmp(option_string, "startingpath")) {
                pdata->option   = STARTING_PATH;
#if SUPPORT_OUTPUT_ARG
        } else if (!strcmp(option_string, "output")) {
                pdata->option   = OUTPUT;
#endif
#if SUPPORT_UPDATEDB_ARG
        } else if (!strcmp(option_string, "updatedb")) {
                pdata->option   = UPDATEDB;
#endif
        } else {
                printk(KERN_WARNING "unknown option: %s\n", option_string);
                pdata->option   = UNKNOWN;
        }
}

/*
 * parse_proc() parses a line like activated:0 written to /proc/rfsdelta
 */
static void parse_proc(struct proc_data *pdata)
{
        char *p;
        p = pdata->pbuffer + pdata->pbuffer_pos;
        // remove leading spaces or reduce more spaces to one.
        if ( *p == ' ' && (pdata->pbuffer_pos == 0 || *(p-1) == ' ')) {
                pdata->pbuffer_pos--;
                return;
        }
        switch (pdata->option) 
        {
        case NOTSET:
                if (*p == ':') {
                        // option string is in pbuffer

                        // remove space before ':'
                        if (pdata->pbuffer_pos>0 && *(p-1) == ' ')
                                p--;
                        *p = '\0';
                        proc_set_option(pdata->pbuffer, pdata);
                        pdata->pbuffer_pos = -1;
                } else if (*p == '\n') {
                        pdata->pbuffer_pos = -1;
                }
                break;
#if SUPPORT_EXCLUDEDIR
        case EXCLUDE_DIR:
                if ( *p == '\n' ) {
                        // the argument is in pbuffer
                        pdata->option = NOTSET;
                        *p = '\0';
                        write_lock(&proc_lock);
#if SUPPORT_UPDATEDB_ARG
                        if (ACTIVATED_ARG == '1' 
                            && strcmp(EXCLUDE_DIR_ARG, pdata->pbuffer)) 
                                UPDATEDB_ARG = 0; /* full updatedb if exclude dir
                                                     has changed */
#endif
                        strcpy(EXCLUDE_DIR_ARG, pdata->pbuffer);
                        write_unlock(&proc_lock);
                        pdata->pbuffer_pos = -1;
                }
#endif
                break;
        case ACTIVATED:
                if ( *p == '\n' ) {
                        pdata->option = NOTSET;
                        *p = '\0';
                        if ( pdata->pbuffer[0] >= '0' && 
                             pdata->pbuffer[0] <= '1') {
                                write_lock(&proc_lock);
#if SUPPORT_UPDATEDB_ARG
                                if(ACTIVATED_ARG == '1' && pdata->pbuffer[0]=='0')
                                        UPDATEDB_ARG = 0; /* full updatedb if
                                                             activated was 1 and 
                                                             changed to 0  */
#endif
                                ACTIVATED_ARG = pdata->pbuffer[0];
                                write_unlock(&proc_lock);
                                if (ACTIVATED_ARG == '0') { /**** pts ****/
                                	/* Imp: lock */
                                	/* Imp: remove f_entry */
					/* rfsdelta_dev_pending_read_size=0; */
                                        wake_up_interruptible( &filenames_wq );
				}
                        }
                        pdata->pbuffer_pos = -1;
                }
                break;
        case STARTING_PATH:
                if ( *p == '\n' ) {
                        // the argument is in pbuffer
                        pdata->option = NOTSET;
                        // add trailing slash, if it's not there
                        if (p>pdata->pbuffer) {
                                if (*(p-1) == '/' || 
				    p - pdata->pbuffer >= PATH_MAX + 16 -2) { 
                                        *p = '\0';
                                } else { 
                                        *p = '/';
                                        *(p+1) = '\0';
                                }
                        } else {
                                *p = '\0';
                        }
                        write_lock(&proc_lock);
#if SUPPORT_UPDATEDB_ARG
                        if (ACTIVATED_ARG == '1' 
                            && strcmp(STARTING_PATH_ARG, pdata->pbuffer)) 
                                UPDATEDB_ARG = 0; /* full updatedb if starting 
                                                     path has changed */
#endif
                        strcpy(STARTING_PATH_ARG, pdata->pbuffer);

                        write_unlock(&proc_lock);
                        pdata->pbuffer_pos = -1;
                }
                break;
#if SUPPORT_OUTPUT_ARG
        case OUTPUT:
                if ( *p == '\n' ) {
                        // the argument is in pbuffer
                        pdata->option = NOTSET;
                        *p = '\0';
                        write_lock(&proc_lock);
#if SUPPORT_UPDATEDB_ARG
                        if (ACTIVATED_ARG == '1' 
                            && strcmp(OUTPUT_ARG, pdata->pbuffer)) 
                                UPDATEDB_ARG = 0; /* full updatedb if output has
                                                     changed */
#endif
                        strcpy(OUTPUT_ARG, pdata->pbuffer);
                        write_unlock(&proc_lock);
                        pdata->pbuffer_pos = -1;
                }
                break;
#endif
#if SUPPORT_UPDATEDB_ARG
        case UPDATEDB:
                if ( *p == '\n' ) {
                        pdata->option = NOTSET;
                        *p = '\0';
                        write_lock(&proc_lock);
                        UPDATEDB_ARG = simple_strtoul(pdata->pbuffer , NULL, 10);
                        write_unlock(&proc_lock);
                        pdata->pbuffer_pos = -1;
                }
                break;
#endif
        default:
                if ( *p=='\n' ) {
                        pdata->option = NOTSET;
                        pdata->pbuffer_pos = -1;
                }
                break;
        }
}

/*
 * proc_rfsdelta_write() fills lists with arguments from buffer
 */
static ssize_t proc_rfsdelta_write( struct file *file,
                                   const char *buffer,
                                   size_t len,
                                   loff_t *offset )
{
        int i;
        char p;
        struct proc_data *pdata = (struct proc_data*)file->private_data;
        for (i = 0; i<len; i++) {
                if (get_user( p, buffer + i))
                        return -EFAULT;
                if ( pdata->pbuffer_pos < (PAGE_SIZE-2)) { // buffer overflow
                        pdata->pbuffer[pdata->pbuffer_pos] = p;
                        parse_proc(pdata);
                        pdata->pbuffer_pos++;
                } else {
                        pdata->pbuffer[pdata->pbuffer_pos] = '\n';
                        pdata->pbuffer[pdata->pbuffer_pos+1] = '\0';
                }
        }
        *offset += i;
        return i;
}

/*
 * rfsdelta_exit_procfs()
 */
int rfsdelta_exit_procfs(void)
{
        remove_proc_entry("rfsdelta", 0);
        remove_proc_entry("rfsdelta-event", 0);
        return 0;
}

/* device functions *********************************************************/


/* 
 * rfsdelta_dev_register()
 */
static int rfsdelta_dev_register(void)
{
        int ret = 0;
	/* Dat: see also linux/drivers/char/mem.c */
	/* Dat: register_chrdev accepts 0 as 1st arg to generate major */
	if (0>(using_major = register_chrdev((major<=0 ? 0 : major), DEVICE_NAME, &device_fops))) {
                printk (KERN_ERR "rfsdelta: register_chrdev failed with %d\n", 
                                 using_major);
                ret = using_major;
                goto out; /* ^^^ Dat: might fail with EBUSY */
	}
	if (major>0) using_major=major;
       	printk (KERN_INFO "rfsdelta: %s device " 
	                DEVICE_NAME " %u:%u\n",
			(major<=0 ? "registered" : "using"), using_major, 0);

        // sysfs 
        rfsdelta_class = class_create(THIS_MODULE, "rfsdelta");
        if (IS_ERR(rfsdelta_class)) {
                printk(KERN_ERR "Error creating rfsdelta class.\n");
                ret = PTR_ERR(rfsdelta_class);
                goto no_simple_class; 
        }
        class_device_create(rfsdelta_class, NULL, MKDEV(using_major, 0), NULL, 
                                DEVICE_NAME);

        // devfs
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
        devfs_mk_cdev(MKDEV(using_major, 0), S_IFCHR|S_IRUSR|S_IWUSR, DEVICE_NAME);
#endif
       	goto out;
no_simple_class:
        unregister_chrdev(using_major, DEVICE_NAME);
out:
        return ret;
}

/*
 * rfsdelta_dev_unregister()
 */
void rfsdelta_dev_unregister(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
        devfs_remove(DEVICE_NAME);
#endif
        class_device_destroy(rfsdelta_class, MKDEV(using_major, 0));
        class_destroy(rfsdelta_class);
        unregister_chrdev(using_major, DEVICE_NAME);
}

/*
 * rfsdelta_dev_open()
 */
static int rfsdelta_dev_open(struct inode *inode, struct file *file)
{
        int old_dev_is_open;

        /**** pts ****/
        spin_lock_irq(&dev_lock);
        old_dev_is_open=dev_is_open;
        dev_is_open = 1;
        spin_unlock_irq(&dev_lock);
        if (old_dev_is_open) return -EBUSY;

        /* Dat: this would block the process (uninterruptible with Ctrl-<C>!) */
        down(&dev_mutex);
        return 0;
}

/*
 * rfsdelta_dev_release()
 */
static int rfsdelta_dev_release(struct inode *inode, struct file *file)
{
        up(&dev_mutex);

        /**** pts ****/
        spin_lock_irq(&dev_lock);
        dev_is_open = 0;
        spin_unlock_irq(&dev_lock);

        return 0;
}

/*
 * rfsdelta_dev_llseek()
 */
static loff_t rfsdelta_dev_llseek( struct file *filp, loff_t offset, int whence)
{
        return -ESPIPE; /* unseekable */
}

/*
 * rfsdelta_dev_ioctl()
 */
static int rfsdelta_dev_ioctl( struct inode *inode, struct file *filp, 
                              unsigned int cmd, unsigned long arg )
{
        return 0; /* success */    
}

/*
 * rfsdelta_dev_write
 */
ssize_t rfsdelta_dev_write( struct file *filp, const char *buff, size_t count, 
                           loff_t *offp )
{
        return (ssize_t)0;
}

/**** pts ****/
size_t rfsdelta_dev_pending_read_size=0;
/** Irrelevant if rfsdelta_dev_pending_read_size==0 */
char *rfsdelta_dev_pending_read_buf=NULL;
struct string_list *rfsdelta_dev_pending_read_entry;

/**** pts ****/
/** @param src_buf should remain readable after return.
 */
ssize_t rfsdelta_dev_read_to_user(char *user_buf, char *src_buf, ssize_t src_size, ssize_t user_buf_left) {
        ssize_t dummy;
        (void)dummy;
	/* Imp: assert(rfsdelta_dev_pending_read_size==0) */
	if (user_buf_left<src_size) {
		/* Imp: return if copy_to_user failed */
		dummy=copy_to_user(user_buf, src_buf, user_buf_left);
		rfsdelta_dev_pending_read_size=src_size-user_buf_left;
		rfsdelta_dev_pending_read_buf=src_buf+user_buf_left;
		rfsdelta_dev_pending_read_entry=NULL;
		return user_buf_left;
	} else {
		/* Imp: return if copy_to_user failed */
		dummy=copy_to_user(user_buf, src_buf, src_size);
		return src_size;
	}
}

/* 
 * rfsdelta_dev_read() 
 * traverse the list of filenames and write them to the user buffer.
 * Remove every entry from the filenames list that has been written. 
 */
static ssize_t rfsdelta_dev_read(struct file *file, 
                           char *buffer, 
                           size_t length, 
                           loff_t *offset)
        {
	ssize_t copyc, length0=length, dummy;
        char *string_ptr;
        struct string_list *f_entry;
        
	/**** pts ****/
        if (length<=0) return 0;
	if (rfsdelta_dev_pending_read_size>length) {
		/* Imp: return if copy_to_user failed */
		dummy=copy_to_user(buffer, rfsdelta_dev_pending_read_buf, length);
		rfsdelta_dev_pending_read_buf+=length;
		rfsdelta_dev_pending_read_size-=length;
		return length;
	}
	if (rfsdelta_dev_pending_read_size>0) {
		/* Imp: return if copy_to_user failed */
		dummy=copy_to_user(buffer, rfsdelta_dev_pending_read_buf, rfsdelta_dev_pending_read_size);
		buffer+=rfsdelta_dev_pending_read_size;
		length-=rfsdelta_dev_pending_read_size;
		rfsdelta_dev_pending_read_buf=NULL;
		rfsdelta_dev_pending_read_size=0;
		if (rfsdelta_dev_pending_read_entry!=NULL) {
			remove_string_entry(rfsdelta_dev_pending_read_entry);
			rfsdelta_dev_pending_read_entry=NULL;
		}
			
	}
	if (length<=0) return length0-length;

        /* daemon activity, set next timeout */
        NEXT_D_TIMEOUT = jiffies + D_TIMEOUT;
        if (ACTIVATED_ARG == 'd') {
                /* inserting of paths was disabled, reenable it */
                printk(KERN_INFO "rfsdelta: inserting of paths reenabled.\n");
                ACTIVATED_ARG = '1';
        }
        if ( list_empty(&filenames_list) && length==length0 ) { // put to sleep
                if( file->f_flags & O_NONBLOCK ) 
                        return -EWOULDBLOCK;
                if (ACTIVATED_ARG != '0') /**** pts ****/
                        interruptible_sleep_on( &filenames_wq );
        }
        while ( (!list_empty(&filenames_list)) ) {
		if (RELOAD != '0') { /* '1', '2':umount or '3' */
		        char s[2];
		        s[0]=RELOAD; s[1]='\0';
			copyc=rfsdelta_dev_read_to_user(buffer, s, 2, length);
			length-=copyc; buffer+=copyc;
			RELOAD = '0';
			if (copyc<2) break; /* if (rfsdelta_dev_pending_read_size>0) */
		}
                f_entry = list_entry(filenames_list.next,
                                     struct string_list,
                                     list);
                string_ptr = f_entry->string;
		/**** pts ****/
		/* vvv Dat: put '\0' at end */
		copyc=rfsdelta_dev_read_to_user(buffer, string_ptr, strlen(string_ptr)+1, length);
		length-=copyc; buffer+=copyc;
		if (rfsdelta_dev_pending_read_size>0) {
			rfsdelta_dev_pending_read_entry=f_entry;
			break;
		}
                remove_string_entry(f_entry);
        }
        return length0-length;
}

/*
 * rfsdelta_dev_poll
 */
static unsigned int rfsdelta_dev_poll( struct file *filp, poll_table *wait)
{
        unsigned int mask = 0;

        poll_wait( filp, &filenames_wq, wait );
        if( !list_empty(&filenames_list) )
                mask |= POLLIN | POLLRDNORM;
        return mask;
}

/*
 * get_path() return the path of a dentry with major and minor number instead
 * of mount point.
 */
inline static char * get_path(struct dentry *dentry, char *buffer, int buflen) {
	char * end = buffer + buflen;
	int namelen;

	*--end = '\0';
	buflen--;

	end[-1]='/'; /**** pts ****/
	for (;;) {
		struct dentry *parent;
		if (IS_ROOT(dentry)) {
			goto mountroot;
		}	
		parent = dentry->d_parent;
		prefetch(parent);
		namelen = dentry->d_name.len;
		buflen -= namelen + 1;
		if (buflen < 0)
			goto Elong;
		end -= namelen;
		memcpy(end, dentry->d_name.name, namelen);
		*--end = '/';
		dentry = parent;
	}
mountroot:
	namelen = strlen(dentry->d_sb->s_id);
	buflen -=namelen + 2;
	if (buflen < 0)
		goto Elong;
	*--end = ':';
	end -= namelen;
	memcpy(end, dentry->d_sb->s_id, namelen);
	return end;
Elong:
	return ERR_PTR(-ENAMETOOLONG);
}
/********************* SECURITY MODULE HOOKS *************************/

#define insert_path(dentry,mode) insert_path_low(dentry,dentry,mode)

/* 
 * insert_path() finds the corresponding full path for a dentry and inserts 
 * it to the filenames_list. MODE which is 'a' or 'm' will be added to the 
 * beginning of the path (or `d' if removed/unlinked, or `1' elsewhere to
 * signify mount point changes). 
 */
inline static void insert_path_low(struct dentry *dentry_for_name,
  struct dentry *dentry_for_stat, const char mode) 
{
        char *path;

        if (ACTIVATED_ARG != '1' &&
            (ACTIVATED_ARG != 'r' || !dev_is_open)) /**** pts ****/
           {
        
#if SUPPORT_UPDATEDB_ARG
                UPDATEDB_ARG = 0; // for full updatedb
#endif
                return;  // not activated or disabled
        }
        if (time_after(jiffies, NEXT_D_TIMEOUT)) {
                /* timeout, disable inserting of paths */
                if (ACTIVATED_ARG != 'd') {
                        ACTIVATED_ARG = 'd'; // disabled
                        printk(KERN_INFO "rfsdelta: daemon timeout: "
                                "inserting of paths disabled.\n");
                }
                return;
        }
	path = get_path(dentry_for_name, path_buffer, PATH_MAX + 16);
        if (!IS_ERR(path)) {
 		char prefix[5*(sizeof(long)*2+1)+3];
	        if (dentry_for_stat->d_inode) { /**** pts ****/
	        	sprintf(prefix, "%c%lX,%lX,%lX,%lX,%lX:", mode,
	        	  (unsigned long)dentry_for_stat->d_inode->i_sb->s_dev, /* st_dev */
	        	  (unsigned long)dentry_for_stat->d_inode->i_ino, /* st_ino */
	        	  (unsigned long)dentry_for_stat->d_inode->i_mode, /* st_mode */
	        	  (unsigned long)dentry_for_stat->d_inode->i_nlink, /* st_nlink */
	        	  (unsigned long)dentry_for_stat->d_inode->i_rdev); /* st_rdev */
	        } else {
	        	/* Dat: no d_inode yet for mkdir() */
	        	sprintf(prefix, "%c%lX,?:", mode,
	        	  (unsigned long)dentry_for_stat->d_sb->s_dev);
	        }
                add_filenames_entry(path, prefix);
	} else 
        	printk(KERN_WARNING "rfsdelta: path too long\n");

}

static int rfsdelta_inode_create( struct inode *dir, struct dentry *dentry, 
                                 int mode )
{
        insert_path( dentry, 'c' );
        return 0;
}

static int rfsdelta_inode_mkdir (struct inode *dir, struct dentry * dentry, 
                                int mode)
{
  	insert_path( dentry, 'M' );
  	return 0;
}

/**** pts ****/
static int rfsdelta_inode_rmdir (struct inode *dir, struct dentry * dentry)
{
  	insert_path( dentry, 'R' );
  	return 0;
}

/**** pts ****/
static int rfsdelta_inode_unlink (struct inode *dir, struct dentry * dentry)
{
  	insert_path( dentry, 'u' );
  	return 0;
}

static int rfsdelta_inode_link (struct dentry * old_dentry,
                               struct inode *dir, 
                               struct dentry * dentry)
{
  	insert_path( old_dentry, 'k' );
  	insert_path_low( dentry, old_dentry, 'l' );
  	return 0;
}

static int rfsdelta_inode_symlink (struct inode *dir, struct dentry * dentry, 
                                  const char *old_name)
{
  	insert_path( dentry, 's' );
  	return 0;
}

static int rfsdelta_inode_mknod (struct inode *dir, struct dentry * dentry,
                                int mode, dev_t dev)
{
        insert_path( dentry, 'm' );
  	return 0;
}

static int rfsdelta_inode_rename( struct inode * old_dir,
                                 struct dentry * old_dentry,
                                 struct inode * new_dir,
                                 struct dentry * new_dentry )
{
        if (new_dentry->d_inode)
                return 0;
        /* ^^^ Dat: usually we get an `aX' for new_dentry, inode is not
         *     available yet (for CONFIG_SECURITY handlers)
         */
        insert_path(old_dentry, 'o');
        insert_path_low( new_dentry, old_dentry, 'a' );
        return 0;
}

static int rfsdelta_sb_mount( char *dev_name, 
			     struct nameidata *nd, 
			     char *type, 
			     unsigned long flags, 
			     void *data)
{
	RELOAD |= 1;
	/* Dat: don't notify immediately, but upon next write */
        return 0;
}

static int rfsdelta_sb_umount( struct vfsmount *mnt, int flags ) 
{
	RELOAD |= 2;
	/* Dat: don't notify immediately, but upon next write */
	return 0;
}


#if SUPPORT_UPDATES
static int rfsdelta_inode_permission (struct inode *inode, int mask, 
                                     struct nameidata *nd)
{
        struct dentry *dentry;
        struct list_head d_list = inode->i_dentry;
        if( ! ( ( mask & MAY_APPEND ) || (mask & MAY_WRITE) ) ) {
                return 0;
        }
        dentry = list_entry(d_list.next, struct dentry, d_alias );
  	insert_path( dentry, 'u' );
  	return 0;
}
#endif

#if SUPPORT_BROKEN_STACKING
static int rfsdelta_register_security(const char *name,
                                      struct security_operations *ops) {
	return 0; /* Dat: this is needed so it works with `rlocate' etc. */
}

static int rfsdelta_unregister_security(const char *name,
                                      struct security_operations *ops) {
	return 0; /* Dat: this is needed so it works with `rlocate' etc. */
}
#endif

/* module functions **********************************************************/

static int mod_register = 0;

/*
 * init_rfsdelta()
 */
static int __init init_rfsdelta(void)
{
        int ret;
	printk(KERN_INFO "rfsdelta version "RFSDELTA_VERSION" loaded\n");
        //init_waitqueue_head (&filenames_wq);
        // register dev 
        ret = rfsdelta_dev_register();
        if (ret)
                goto out;
                                
        // register as security module
        ret = register_security( &rfsdelta_security_ops );
        if ( ret ) {
                ret = mod_reg_security(DEVICE_NAME, &rfsdelta_security_ops);
                if ( ret != 0 ) {
                        printk(KERN_ERR"Failed to register rfsdelta module with"
                                        " the kernel\n");
                        goto no_lsm;
                }
                mod_register = 1;
        }
        if ((path_buffer = kmalloc( PATH_MAX + 16, GFP_KERNEL )) == NULL) {
                printk (KERN_ERR "rfsdelta: __get_free_page failed\n");
                ret = -ENOMEM;
                goto no_path_buffer;
        }
#if SUPPORT_EXCLUDEDIR
        EXCLUDE_DIR_ARG = (char*)__get_free_page( GFP_KERNEL );
        if (!EXCLUDE_DIR_ARG) {
                ret = -ENOMEM;
                goto no_exclude_dir_arg;
        }
#endif
        STARTING_PATH_ARG = (char*)__get_free_page( GFP_KERNEL );
        if (!STARTING_PATH_ARG) {
                ret = -ENOMEM;
                goto no_starting_path_arg;
        }
#if SUPPORT_OUTPUT_ARG
        OUTPUT_ARG = (char*)__get_free_page( GFP_KERNEL );
        if (!OUTPUT_ARG) {
                ret = -ENOMEM;
                goto no_output_arg;
        }
#endif
#if SUPPORT_EXCLUDEDIR
        *EXCLUDE_DIR_ARG = '\0';
#endif
        *STARTING_PATH_ARG = '\0';
#if SUPPORT_OUTPUT_ARG
        *OUTPUT_ARG = '\0';
#endif
        // create proc entry
        ret = rfsdelta_init_procfs();
	if (ret) {
		printk (KERN_ERR "rfsdelta: rfsdelta_init_procfs() failed.\n");
                goto no_proc;
	}
        // set timeout for daemon inactivity
        NEXT_D_TIMEOUT = jiffies + D_TIMEOUT;
  	goto out;

no_proc:
#if SUPPORT_OUTPUT_ARG
        free_page( (unsigned long)OUTPUT_ARG);
no_output_arg:
#endif
        free_page( (unsigned long)STARTING_PATH_ARG);
no_starting_path_arg:
#if SUPPORT_EXCLUDEDIR
        free_page( (unsigned long)EXCLUDE_DIR_ARG);
no_exclude_dir_arg:
#endif
        kfree(path_buffer); 
no_path_buffer:
        if ( mod_register ) 
                mod_unreg_security(DEVICE_NAME, &rfsdelta_security_ops);
        else
                unregister_security(&rfsdelta_security_ops);
no_lsm:
        rfsdelta_dev_unregister();
out:
        return ret;
}

/*
 * exit_rfsdelta()
 */
static void __exit exit_rfsdelta(void)
{
        rfsdelta_exit_procfs();
	remove_string_list(&filenames_list);
#if SUPPORT_OUTPUT_ARG
        free_page( (unsigned long)OUTPUT_ARG);
#endif
        free_page( (unsigned long)STARTING_PATH_ARG);
#if SUPPORT_EXCLUDEDIR
        free_page( (unsigned long)EXCLUDE_DIR_ARG);
#endif
        kfree(path_buffer); 
        if ( mod_register ) {
		if (mod_unreg_security(DEVICE_NAME, &rfsdelta_security_ops)) 
                        printk(KERN_INFO "rfsdelta: failed to unregister "
					 "rfsdelta security module with primary "
					 "module.\n");
	} else if (unregister_security(&rfsdelta_security_ops)) 
                        printk(KERN_INFO "rfsdelta: failed to unregister "
					 "rfsdelta security module.\n");
        
        rfsdelta_dev_unregister();
  	printk(KERN_INFO "rfsdelta: unloaded\n");
}


security_initcall( init_rfsdelta );
module_exit( exit_rfsdelta );
