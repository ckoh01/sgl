/* source/include/sgl_fs.h
 *
 * MIT License
 *
 * Copyright(c) 2023-present All contributors of SGL  
 * Document reference link: https://sgl-docs.readthedocs.io
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __SGL_FS_H__
#define __SGL_FS_H__

#include "sgl_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SGL_S_IFMT   0170000  /* mask of file type */
#define SGL_S_IFREG  0100000  /* regular file */
#define SGL_S_IFDIR  0040000  /* directory file */

#define SGL_S_IRWXU  00700    /* owner permission */
#define SGL_S_IRWXG  00070    /* group permission */
#define SGL_S_IRWXO  00007    /* other permission */

#define SGL_MAX_FD   (8)      /* max file descriptor */
#define SGL_MAX_DD   (4)      /* max directory descriptor */

/**
 * @brief File status
 * @st_mode: file type and permission
 * @st_size: file size, in bytes
 * @st_mtime: last modify time
 */
typedef struct {
    uint32_t st_mode;
    uint32_t st_size;
    uint32_t st_mtime;
} sgl_stat_t;

/**
 * @brief File system device
 * @init: init disk device
 * @read_sectors: read sectors from disk devices
 * @write_sector: write sector to disk devices
 * @ioctl: ioctl command, it is optional
 * @status: status command, it is optional
 * @priv: private data
 */
typedef struct sgl_block_dev {
    int (*init)(struct sgl_block_dev *dev);
    int (*read_sectors)(struct sgl_block_dev *dev, uint32_t sector, uint8_t *buf, uint32_t count);
    int (*write_sectors)(struct sgl_block_dev *dev, uint32_t sector, const uint8_t *buf, uint32_t count);
    int (*ioctl)(struct sgl_block_dev *dev, uint8_t cmd, void *param);
    int (*status)(struct sgl_block_dev *dev);
    void *priv;
} sgl_block_dev_t;

/**
 * @brief File system operations
 * @mount: mount file system
 * @unmount: unmount file system
 * @open: open file
 * @close: close file
 * @read: read file
 * @write: write file
 * @opendir: open directory
 * @readdir: read directory
 * @closedir: close directory
 * @sync: sync file system
 * @format: format file system
 * @remove: remove file
 * @mkdir: make directory
 * @stat: get file status
 * @rename: rename file
 */
typedef struct sgl_vfs_ops {
    int (*mount)(struct sgl_vfs_ops *ops, struct sgl_block_dev *dev, const char *mount_point, void *fs_config);
    int (*unmount)(struct sgl_vfs_ops *ops, const char *mount_point);

    int (*open)(struct sgl_vfs_ops *ops, const char *path, uint32_t flags);
    int (*close)(struct sgl_vfs_ops *ops, int fd);
    int (*read)(struct sgl_vfs_ops *ops, int fd, void *buffer, uint32_t count);
    int (*write)(struct sgl_vfs_ops *ops, int fd, const void *buffer, uint32_t count);

    int (*opendir)(struct sgl_vfs_ops *ops, const char *path, int *dd);
    int (*readdir)(struct sgl_vfs_ops *ops, int dd, char *name, uint32_t name_size, uint32_t *type);
    int (*closedir)(struct sgl_vfs_ops *ops, int dd);

    int (*sync)(struct sgl_vfs_ops *ops);
    int (*format)(struct sgl_vfs_ops *ops);
    int (*remove)(struct sgl_vfs_ops *ops, const char *path);
    int (*mkdir)(struct sgl_vfs_ops *ops, const char *path);

    int (*stat)(struct sgl_vfs_ops *ops, const char *path, sgl_stat_t *st);
    int (*rename)(struct sgl_vfs_ops *ops, const char *old_path, const char *new_path);
} sgl_vfs_ops_t;

/**
 * @brief File system type
 * @name: file system name
 * @ops: file system operations
 */
typedef struct sgl_fs_type {
    sgl_list_node_t node;
    const char *name;
    sgl_vfs_ops_t *ops;
} sgl_fs_type_t;

/**
 * @brief Mount point
 * @path: mount point path
 * @dev: block device
 * @fs_type: file system type
 * @fs_data: file system data
 */
typedef struct sgl_mount_point {
    sgl_list_node_t node;
    const char *path;
    sgl_block_dev_t *dev;
    sgl_fs_type_t *fs_type;
    void *fs_data;
} sgl_mount_point_t;

/**
 * @brief File descriptor control block
 * @used: used flag
 * @local_fd: local file descriptor
 * @mp: mount point
 */
typedef struct {
    uint16_t used;
    int16_t  local_fd;
    sgl_mount_point_t *mp;
} sgl_fd_ctrl_t;

/**
 * @brief Directory descriptor control block
 * @used: used flag
 * @local_dd: local directory descriptor
 * @mp: mount point
 */
typedef struct {
    uint16_t used;
    int16_t  local_dd;
    sgl_mount_point_t *mp;
} sgl_dd_ctrl_t;

#ifdef __cplusplus
}
#endif

#endif /* __SGL_FS_H__ */
