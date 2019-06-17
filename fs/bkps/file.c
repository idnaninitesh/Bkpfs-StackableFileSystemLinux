/*
 * Copyright (c) 1998-2017 Erez Zadok
 * Copyright (c) 2009	   Shrikar Archak
 * Copyright (c) 2003-2017 Stony Brook University
 * Copyright (c) 2003-2017 The Research Foundation of SUNY
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "bkpfs.h"

/* set extended attribute related to version management */
static int
bkpfs_setxattr_version(struct dentry *dentry, struct inode *inode,
		const char *name, const void *value, size_t size, int flags)
{
	int err;
	struct dentry *lower_dentry;
	struct path lower_path;

	bkpfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	if (!(d_inode(lower_dentry)->i_opflags & IOP_XATTR)) {
		err = -EOPNOTSUPP;
		goto out;
	}
	err = vfs_setxattr(lower_dentry, name, value, size, flags);
	if (err)
		goto out;
	fsstack_copy_attr_all(d_inode(dentry),
			      d_inode(lower_path.dentry));
out:
	bkpfs_put_lower_path(dentry, &lower_path);
	return err;
}


/* get extended attribute related to version management */
static ssize_t
bkpfs_getxattr_version(struct dentry *dentry, struct inode *inode,
		const char *name, void *buffer, size_t size)
{
	int err;
	struct dentry *lower_dentry;
	struct inode *lower_inode;
	struct path lower_path;

	bkpfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	lower_inode = bkpfs_lower_inode(inode);
	if (!(d_inode(lower_dentry)->i_opflags & IOP_XATTR)) {
		err = -EOPNOTSUPP;
		goto out;
	}
	err = vfs_getxattr(lower_dentry, name, buffer, size);
	if (err)
		goto out;
	fsstack_copy_attr_atime(d_inode(dentry),
				d_inode(lower_path.dentry));
out:
	bkpfs_put_lower_path(dentry, &lower_path);
	return err;
}

/*
 * handle the max version xattr
 * return the updated version on success
 * else error code (negative)
 */
static int bkpfs_update_max_version(struct file *file, int update)
{
	int initial_ver = 1;
	void *buffer = NULL;
	void *value = NULL;
	int err = 0;

	value = kmalloc(MAX_BKP_NUM_LEN, GFP_KERNEL);
	memcpy(value, &initial_ver, MAX_BKP_NUM_LEN);
	buffer = kmalloc(MAX_BKP_NUM_LEN, GFP_KERNEL);

	initial_ver = -1;
	err = bkpfs_getxattr_version(file->f_path.dentry, file->f_inode,
			BKPFS_XATTR_MAX_VERSION, buffer, MAX_BKP_NUM_LEN);
	if (err > 0) {
		initial_ver = *(int *)buffer;
		initial_ver = initial_ver + update;
		memcpy(value, &initial_ver, MAX_BKP_NUM_LEN);
	} else {
		if (err == -ENODATA)
			err = -EINVAL;
		initial_ver = err;
		goto out;
	}

	err = bkpfs_setxattr_version(file->f_path.dentry, file->f_inode,
			BKPFS_XATTR_MAX_VERSION, value, MAX_BKP_NUM_LEN, 0);
	if (err)
		initial_ver = err;

out:
	kfree(buffer);
	kfree(value);

	return initial_ver;
}

/*
 * handle the min version xattr
 * return the updated version on success
 * else error code (negative)
 */
static int bkpfs_update_min_version(struct file *file, int update)
{
	int initial_ver = 1;
	void *buffer = NULL;
	void *value = NULL;
	int err = 0;

	value = kmalloc(MAX_BKP_NUM_LEN, GFP_KERNEL);
	memcpy(value, &initial_ver, MAX_BKP_NUM_LEN);
	buffer = kmalloc(MAX_BKP_NUM_LEN, GFP_KERNEL);

	initial_ver = -1;
	err = bkpfs_getxattr_version(file->f_path.dentry, file->f_inode,
			BKPFS_XATTR_MIN_VERSION, buffer, MAX_BKP_NUM_LEN);
	if (err > 0) {
		initial_ver = *(int *)buffer;
		initial_ver = initial_ver + update;
		memcpy(value, &initial_ver, MAX_BKP_NUM_LEN);
	} else {
		if (err == -ENODATA)
			err = -EINVAL;
		initial_ver = err;
		goto out;
	}

	err = bkpfs_setxattr_version(file->f_path.dentry, file->f_inode,
			BKPFS_XATTR_MIN_VERSION, value, MAX_BKP_NUM_LEN, 0);
	if (err)
		initial_ver = err;

out:
	kfree(buffer);
	kfree(value);

	if (initial_ver > 0)
		initial_ver--;

	return initial_ver;

}

/* return number of versions if success else error code (negative) */
static int bkpfs_get_version_count(struct file *file)
{

	int min_ver = 1;
	int max_ver = 0;
	void *value = NULL;
	void *buffer = NULL;
	int count = -1;
	int err = 0;

	value = kmalloc(MAX_BKP_NUM_LEN, GFP_KERNEL);
	memcpy(value, &min_ver, MAX_BKP_NUM_LEN);
	buffer = kmalloc(MAX_BKP_NUM_LEN, GFP_KERNEL);

	/* get min ver xattr */
	err = bkpfs_getxattr_version(file->f_path.dentry, file->f_inode,
			BKPFS_XATTR_MIN_VERSION, buffer, MAX_BKP_NUM_LEN);
	if (err > 0) {
		min_ver = *(int *)buffer;
	} else if (err == -ENODATA) {
		err = bkpfs_setxattr_version(file->f_path.dentry, file->f_inode,
			BKPFS_XATTR_MIN_VERSION, value, MAX_BKP_NUM_LEN, 0);
		if (err) {
			count = err;
			goto out;
		}
	} else {
		count = err;
		goto out;
	}

	memcpy(value, &max_ver, MAX_BKP_NUM_LEN);

	/* get max ver xattr */
	err = bkpfs_getxattr_version(file->f_path.dentry, file->f_inode,
			BKPFS_XATTR_MAX_VERSION, buffer, MAX_BKP_NUM_LEN);
	if (err > 0) {
		max_ver = *(int *)buffer;
	} else if (err == -ENODATA) {
		err = bkpfs_setxattr_version(file->f_path.dentry, file->f_inode,
			BKPFS_XATTR_MAX_VERSION, value, MAX_BKP_NUM_LEN, 0);
		if (err) {
			count = err;
			goto out;
		}
	} else {
		count = err;
		goto out;
	}

	count = max_ver-min_ver+1;

out:
	kfree(buffer);
	kfree(value);

	return count;

}

/* generates file name from original file name and stores it in newfile name
 * i.e. file.txt -> .file.txt_v(1..N) for version files
 *	.file.txt_v(1..N) -> file.txt_v(1..N) for restore files
 */
static void bkpfs_create_version_file_name(const char *origfile_name,
			int version_num, int file_type, char *newfile_name)
{

	char *ver_str = NULL;

	/* add prefix for version files */
	if (file_type == VERSION)
		newfile_name[0] = '.';
	newfile_name[file_type] = '\0';

	/* append file name and suffix */
	strcat(newfile_name, origfile_name);
	strcat(newfile_name, BKPFS_FILE_SUFFIX);

	/* append version number */
	ver_str = kmalloc(MAX_BKP_NUM_LEN, GFP_KERNEL);
	snprintf(ver_str, MAX_BKP_NUM_LEN, "%d", version_num);
	strcat(newfile_name, ver_str);
	kfree(ver_str);

}

/* 0 on success with path stored in lower_path, else errno on failure
 * LOOKUP_CREATE - creates negative dentry using the this struct
 * LOOKUP_OPEN - vfs_path_lookup to find existing file name
 */
static int bkpfs_lookup_version_file(struct file *file, char *newfile_name,
					int flags, struct path *lower_path)
{

	struct dentry *dentry = file->f_path.dentry;
	struct dentry *lower_dir_dentry = NULL;
	struct path lower_parent_path;
	struct dentry *lower_parent_dentry = NULL;
	struct vfsmount *lower_dir_mnt = NULL;
	struct qstr this;
	struct dentry *lower_dentry = NULL;
	int err = 0;


	lower_dir_dentry = dget_parent(dentry);
	bkpfs_get_lower_path(lower_dir_dentry, &lower_parent_path);

	lower_parent_dentry = lower_parent_path.dentry;
	lower_dir_mnt = lower_parent_path.mnt;

	/* Use vfs_path_lookup to check if the dentry exists or not */
	err = vfs_path_lookup(lower_parent_dentry, lower_dir_mnt,
				newfile_name, 0, lower_path);

	/* create a dentry if not found using lookup */
	if (err && err != -ENOENT)
		goto out;

	/* ENOENT and we wish to find dentry so return error */
	if (err && (flags & LOOKUP_OPEN))
		goto out;

	if (err) {
		err = 0;

		/* initialize and add a negative dentry */
		this.name = newfile_name;
		this.len = strlen(this.name);
		this.hash = full_name_hash(lower_parent_dentry, this.name,
								this.len);

		lower_dentry = d_alloc(lower_parent_dentry, &this);

		if (!lower_dentry) {
			err = -ENOMEM;
			goto out;
		}
		d_add(lower_dentry, NULL);

	} else {
		lower_dentry = lower_path->dentry;
	}

	lower_path->dentry = lower_dentry;
	lower_path->mnt = mntget(lower_dir_mnt);

	/* update parent directory's atime */
	fsstack_copy_attr_atime(d_inode(lower_dir_dentry),
				bkpfs_lower_inode(d_inode(lower_dir_dentry)));


out:
	bkpfs_put_lower_path(lower_dir_dentry, &lower_parent_path);
	dput(lower_dir_dentry);

	return err;

}

/*
 * creates version file name
 * does lookup to create/fetch the path struct
 * vfs_create to create inode for the version file
 * dentry open to finally open the version file
 * valid path struct for the new version file on success
 * else ERR_PTR on failure
 */
static struct file *bkpfs_create_version_file(struct file *file, int ver)
{
	const char *origfile_name = file->f_path.dentry->d_name.name;
	char *newfile_name;
	struct path lower_path;
	struct dentry *lower_dentry;
	struct dentry *lower_parent_dentry = NULL;
	struct inode *dir = d_inode(file->f_path.dentry->d_parent);
	struct file *lower_file = NULL;
	int err = 0;

	/* getting the new file name to be created */
	newfile_name = kmalloc(MAX_FILE_NAME_LEN, GFP_KERNEL);
	bkpfs_create_version_file_name(origfile_name, ver, VERSION,
							newfile_name);

	/* getting dentry for the new file */
	err = bkpfs_lookup_version_file(file, newfile_name, LOOKUP_CREATE,
							&lower_path);
	if (err) {
		pr_info("Failed to create dentry for the new version file\n");
		lower_file = ERR_PTR(err);
		bkpfs_update_max_version(file, -1);
		goto out;
	}

	/* create inode for the dentry obtained */
	lower_dentry = lower_path.dentry;
	lower_parent_dentry = lock_parent(lower_dentry);

	err = vfs_create(d_inode(lower_parent_dentry), lower_dentry,
					file->f_inode->i_mode, 0);
	if (err && err != -EEXIST) {
		lower_file = ERR_PTR(err);
		pr_info(
		"Failed to create inode for the new version file : %d\n", err);
		bkpfs_update_max_version(file, -1);
		goto out_create;
	}

	fsstack_copy_attr_times(dir, bkpfs_lower_inode(dir));
	fsstack_copy_inode_size(dir, d_inode(lower_parent_dentry));

	/* open the new file created */
	lower_file = dentry_open(&lower_path, O_WRONLY|O_CREAT, current_cred());
	if (IS_ERR_OR_NULL(lower_file)) {
		pr_info("Failed to open the new file\n");
		if (filp_close(lower_file, NULL))
			pr_info("Failed to close the new file\n");
	} else {
		fsstack_copy_attr_all(dir, bkpfs_lower_inode(dir));
	}

out_create:
	unlock_dir(lower_parent_dentry);
out:
	kfree(newfile_name);

	return lower_file;
}

/*
 * valid file struct for the required version file on success
 * else ERR_PTR on failure
 */
static struct file *bkpfs_get_version_file(struct file *file, int initial_ver)
{


	const char *origfile_name = file->f_path.dentry->d_name.name;
	char *newfile_name;
	struct path lower_path;
	struct file *lower_file = NULL;
	struct inode *inode = d_inode(file->f_path.dentry->d_parent);
	int err = 0;

	/* getting the file name for the version file */
	newfile_name = kmalloc(MAX_FILE_NAME_LEN, GFP_KERNEL);
	bkpfs_create_version_file_name(origfile_name, initial_ver, VERSION,
								newfile_name);


	/* getting dentry for the existing version file */
	err = bkpfs_lookup_version_file(file, newfile_name, LOOKUP_OPEN,
							&lower_path);
	if (err) {
		pr_info("Failed to fetch dentry for the version file\n");
		lower_file = ERR_PTR(err);
		goto out;
	}

	/* open the new file created */
	lower_file = dentry_open(&lower_path, O_RDONLY, current_cred());
	if (IS_ERR_OR_NULL(lower_file)) {
		pr_info("Failed to open the new file\n");
		if (filp_close(lower_file, NULL))
			pr_info("Failed to close the new file\n");
	} else {
		fsstack_copy_attr_all(inode, bkpfs_lower_inode(inode));
	}

out:
	kfree(newfile_name);


	return lower_file;
}

/*
 * valid file struct for the new restore(original) file on success
 * else ERR_PTR on failure
 */
static struct file *bkpfs_create_restore_file(struct file *file,
							int initial_ver)
{

	const char *origfile_name = file->f_path.dentry->d_name.name;
	char *newfile_name;
	struct path lower_path;
	struct dentry *lower_dentry;
	struct dentry *lower_parent_dentry = NULL;
	struct inode *dir = d_inode(file->f_path.dentry->d_parent);
	struct file *lower_file = NULL;
	int err = 0;

	/* getting the new file name to restore version file in */
	newfile_name = kmalloc(MAX_FILE_NAME_LEN, GFP_KERNEL);
	bkpfs_create_version_file_name(origfile_name, initial_ver, ORIGINAL,
							newfile_name);


	/* getting dentry for the new file */
	err = bkpfs_lookup_version_file(file, newfile_name, LOOKUP_CREATE,
							&lower_path);
	if (err) {
		pr_info(
		"Failed to create dentry for the restored version file\n");
		goto out;
	}

	/* create inode for the dentry obtained */
	lower_dentry = lower_path.dentry;
	lower_parent_dentry = lock_parent(lower_dentry);

	err = vfs_create(d_inode(lower_parent_dentry), lower_dentry,
						file->f_inode->i_mode, 0);
	if (err && err != -EEXIST) {
		pr_info(
		"Failed to create inode for the new restore file : %d\n", err);
		lower_file = ERR_PTR(err);
		goto out_create;
	}

	fsstack_copy_attr_times(dir, bkpfs_lower_inode(dir));
	fsstack_copy_inode_size(dir, d_inode(lower_parent_dentry));

	/* open the new file created */
	lower_file = dentry_open(&lower_path, O_WRONLY|O_CREAT,
						current_cred());
	if (IS_ERR_OR_NULL(lower_file)) {
		pr_info("Failed to open the new file\n");
		if (filp_close(lower_file, NULL))
			pr_info("Failed to close the new file\n");
	} else {
		fsstack_copy_attr_all(dir, bkpfs_lower_inode(dir));
	}

out_create:
	unlock_dir(lower_parent_dentry);
out:
	kfree(newfile_name);

	return lower_file;
}

/*
 * copy file data from origfile to verfile
 * num bytes copied on success
 * else 0 or negative errno on failure
 */
static int bkpfs_copy_file_data(struct file *origfile, struct file *verfile,
						size_t len, unsigned int flags)
{

	int error;

	if (unlikely(!len))
		return 0;

	error = -EBADF;
	if (origfile) {
		if (verfile) {
			error = vfs_copy_file_range(origfile, 0,
						verfile, 0,
						len, flags);
		}
	}

	return error;
}

static ssize_t bkpfs_read(struct file *file, char __user *buf,
			   size_t count, loff_t *ppos)
{
	int err;
	struct file *lower_file;
	struct dentry *dentry = file->f_path.dentry;

	lower_file = bkpfs_lower_file(file);
	err = vfs_read(lower_file, buf, count, ppos);
	/* update our inode atime upon a successful lower read */
	if (err >= 0)
		fsstack_copy_attr_atime(d_inode(dentry),
					file_inode(lower_file));


	return err;
}

/* num bytes copied on success, 0 or negative errno on failure */
static int bkpfs_copy_to_version_file(struct file *file)
{
	struct file *lower_orig_file = NULL;
	struct file *lower_ver_file = NULL;
	int ver = -1;
	fmode_t prev_mode;
	int err = 0;

	lower_orig_file = bkpfs_lower_file(file);

	/* update max ver xattr to add new file */
	ver = bkpfs_update_max_version(file, 1);
	if (ver < 0) {
		err = ver;
		goto out;
	}

	/* create new version file */
	lower_ver_file = bkpfs_create_version_file(file, ver);
	if (IS_ERR_OR_NULL(lower_ver_file)) {
		pr_info("Failed to create new version file for backup\n");
		err = PTR_ERR(lower_ver_file);
		goto out;
	}

	prev_mode = lower_orig_file->f_mode;
	lower_orig_file->f_mode |= FMODE_READ;

	/* copy data from original file to version file */
	err = bkpfs_copy_file_data(lower_orig_file, lower_ver_file,
					lower_orig_file->f_inode->i_size, 0);
	lower_orig_file->f_mode = prev_mode;

	/* dentry open is done for the file in create version file */
	if (filp_close(lower_ver_file, NULL))
		pr_info("Failed to close the versioon file\n");

	if (err < 0)
		pr_info("Failed to backup the original file\n");
out:
	return err;

}

/*
 * update the bytes xattr using the number of bytes returned from vfs_write
 * return 1 if backup condition are met, 0 or errno otherwise
 */
static int bkpfs_may_create_version(struct file *file, int count)
{

	int initial = count;
	void *buffer = NULL;
	void *value = NULL;
	int max_bytes = BKPFS_NUM_BYTES_THRESHOLD;
	int flag = 0;
	int err = 0;

	value = kmalloc(MAX_BKP_NUM_LEN, GFP_KERNEL);
	buffer = kmalloc(MAX_BKP_NUM_LEN, GFP_KERNEL);

	/* get num bytes xattr and check if bkp conditions are met */
	err = bkpfs_getxattr_version(file->f_path.dentry, file->f_inode,
				BKPFS_XATTR_NUM_BYTES, buffer, MAX_BKP_NUM_LEN);
	if (err > 0) {
		initial = *(int *)buffer;
		if (count == -1) {
			count = 0;
			max_bytes = 1;
		}
		initial += count;
		if (initial >= max_bytes) {
			flag = 1;
			initial = 0;
		}
	} else if (err != -ENODATA) {
		flag = err;
		goto out;
	}

	/* set the update value in num bytes xattr */
	memcpy(value, &initial, MAX_BKP_NUM_LEN);
	err = bkpfs_setxattr_version(file->f_path.dentry, file->f_inode,
			BKPFS_XATTR_NUM_BYTES, value, MAX_BKP_NUM_LEN, 0);
	if (err)
		flag = err;

out:
	kfree(buffer);
	kfree(value);

	return flag;

}

/* positive len of version list on success, 0 or errno otherwise */
static int bkpfs_list_file_versions(struct file *file, char *res_buffer)
{
	int min_ver = 1;
	int max_ver = 0;
	int ret_len = 0;
	void *buffer = NULL;
	int i;
	char *prefix;
	const char *file_name = file->f_path.dentry->d_name.name;
	char *ver_str;
	int err = 0;

	buffer = kmalloc(MAX_BKP_NUM_LEN, GFP_KERNEL);
	prefix = kmalloc(MAX_FILE_NAME_LEN, GFP_KERNEL);
	ver_str = kmalloc(MAX_BKP_NUM_LEN, GFP_KERNEL);

	/* get min version xattr */
	err = bkpfs_getxattr_version(file->f_path.dentry, file->f_inode,
			BKPFS_XATTR_MIN_VERSION, buffer, MAX_BKP_NUM_LEN);
	if (err > 0) {
		min_ver = *(int *)buffer;
	} else {
		if (err == -ENODATA)
			err = -EINVAL;
		ret_len = err;
		goto out;
	}

	/* get max version xattr */
	err = bkpfs_getxattr_version(file->f_path.dentry, file->f_inode,
			BKPFS_XATTR_MAX_VERSION, buffer, MAX_BKP_NUM_LEN);
	if (err > 0)
		max_ver = *(int *)buffer;
	else {
		if (err == -ENODATA)
			err = -EINVAL;
		ret_len = err;
		goto out;
	}

	/* generate names for all versions from minver to maxver */
	prefix[0] = '.';
	prefix[1] = '\0';
	strcat(prefix, file_name);
	strcat(prefix, BKPFS_FILE_SUFFIX);

	for (i = min_ver; i <= max_ver; i++) {
		strcat(res_buffer, prefix);
		snprintf(ver_str, sizeof(i), "%d", i);
		strcat(res_buffer, ver_str);

		if (i == min_ver && min_ver != max_ver)
			strcat(res_buffer, " (oldest)");

		if (i == max_ver && min_ver != max_ver)
			strcat(res_buffer, " (newest)");
		else if (min_ver != max_ver)
			strcat(res_buffer, "\n");

	}

	ret_len = strlen(res_buffer);

out:
	kfree(buffer);
	kfree(prefix);
	kfree(ver_str);

	return ret_len;
}

/*
 * unlink the required version file
 * returns 0 on success, else errno on failure
 */
static int bkpfs_unlink_version_file(struct file *file, int ver)
{
	const char *origfile_name = file->f_path.dentry->d_name.name;
	char *newfile_name;
	struct path lower_path;
	struct dentry *lower_dentry;
	struct inode *dir = d_inode(file->f_path.dentry->d_parent);
	struct dentry *lower_dir_dentry;
	struct inode *lower_dir_inode = bkpfs_lower_inode(dir);
	int err = 0;

	/* getting the file name for the version file */
	newfile_name = kmalloc(MAX_FILE_NAME_LEN, GFP_KERNEL);
	bkpfs_create_version_file_name(origfile_name, ver, VERSION,
							newfile_name);

	/* getting dentry for the existing version file */
	err = bkpfs_lookup_version_file(file, newfile_name, LOOKUP_OPEN,
							&lower_path);
	if (err) {
		pr_info("Failed to fetch dentry for the version file\n");
		goto out;
	}

	lower_dentry = lower_path.dentry;

	/* unlink the dentry */
	dget(lower_dentry);
	lower_dir_dentry = lock_parent(lower_dentry);

	err = vfs_unlink(lower_dir_inode, lower_dentry, NULL);

	if (err == -EBUSY && lower_dentry->d_flags & DCACHE_NFSFS_RENAMED)
		err = 0;
	if (err)
		goto out_unlink;

	fsstack_copy_attr_times(dir, lower_dir_inode);
	fsstack_copy_inode_size(dir, lower_dir_inode);

out_unlink:
	unlock_dir(lower_dir_dentry);
	dput(lower_dentry);

out:
	kfree(newfile_name);

	return err;
}

/*
 * handler for unlink version to delete all version files
 * returns 0 on success, else errno on failure
 */
static int bkpfs_unlink_all_versions(struct file *file)
{

	int min_ver = 1;
	int max_ver = 0;
	void *buffer = NULL;
	int i = 0;
	int err = 0;
	int ver = 0;

	buffer = kmalloc(MAX_BKP_NUM_LEN, GFP_KERNEL);

	/* get min ver xattr */
	err = bkpfs_getxattr_version(file->f_path.dentry, file->f_inode,
			BKPFS_XATTR_MIN_VERSION, buffer, MAX_BKP_NUM_LEN);
	if (err > 0)
		min_ver = *(int *)buffer;
	else {
		if (err == -ENODATA)
			err = -EINVAL;
		goto out;
	}

	/* get max ver xattr */
	err = bkpfs_getxattr_version(file->f_path.dentry, file->f_inode,
			BKPFS_XATTR_MAX_VERSION, buffer, MAX_BKP_NUM_LEN);
	if (err > 0)
		max_ver = *(int *)buffer;
	else {
		if (err == -ENODATA)
			err = -EINVAL;
		goto out;
	}

	/* call unlink for version from minver to maxver and update maxver */
	for (i = min_ver; i <= max_ver; i++) {
		ver = bkpfs_update_max_version(file, -1) + 1;
		if (ver < 0) {
			err = ver;
			goto out;
		}

		err = bkpfs_unlink_version_file(file, ver);
		if (err) {
			bkpfs_update_max_version(file, 1);
			goto out;
		}
	}

out:
	kfree(buffer);

	return err;
}

/*
 * num bytes read on success and data stores in res_buffer
 * 0 if EOF, errno otherwise
 */
static int bkpfs_get_version_file_data(struct file *file, char *version,
				char __user *res_buffer, int buf_len, int pos)
{

	void *buffer = NULL;
	int err = 0;
	int ver = 0;
	struct file *lower_ver_file = NULL;
	struct dentry *dentry = file->f_path.dentry;
	int bytes = 0;
	fmode_t prev_mode;
	int max_ver = 0;
	int i = 0;
	int version_num = 0;
	loff_t file_pos = (loff_t) pos;
	int max_ver_count = -1;
	int count = 0;

	buffer = kmalloc(MAX_BKP_NUM_LEN, GFP_KERNEL);

	/* generate appropriate version num from user argument version */
	err = bkpfs_getxattr_version(dentry, file->f_inode,
				BKPFS_XATTR_MAX_VERSION, buffer, MAX_BKP_NUM_LEN);
	if (err > 0)
		max_ver = *(int *)buffer;
	else {
		if (err == -ENODATA)
			err = -EINVAL;
		goto out;
	}

	if (strcmp(version, "newest") != 0) {
		if (strcmp(version, "oldest") != 0) {
			/* convert version to version_num */
			i = 0;
			while (version[i] != '\0' &&
				(version[i] >= '0' && version[i] <= '9')) {
				version_num = version_num*10 + (version[i]-'0');
				i++;
			}
			if (version[i] != '\0') {
				err = -EINVAL;
				goto out;
			}

			/* check if version > max_ver or version count */
			max_ver_count = bkpfs_get_super_ver(
						file->f_inode->i_sb);
			if (max_ver_count == -1)
				max_ver_count = BKPFS_MAX_VERSIONS;
			count = bkpfs_get_version_count(file);
			if (count < 0) {
				err = count;
				goto out;
			}
			if (version_num > max_ver_count ||
					version_num > count) {
				err = -EINVAL;
				goto out;
			}
		} else {
			version_num = 1;
		}

		err = bkpfs_getxattr_version(dentry, file->f_inode,
				BKPFS_XATTR_MIN_VERSION, buffer, MAX_BKP_NUM_LEN);
		if (err > 0) {
			ver = *(int *)buffer;
			ver = ver + version_num - 1;
		} else {
			if (err == -ENODATA)
				err = -EINVAL;
			goto out;
		}
	} else {
		ver = max_ver;
	}

	/* get the required version file to be read from */
	lower_ver_file = bkpfs_get_version_file(file, ver);
	if (IS_ERR_OR_NULL(lower_ver_file)) {
		pr_info("Failed to fetch the required version file\n");
		err = PTR_ERR(lower_ver_file);
		goto out;
	}

	/* read data from file and store in res buffer */
	prev_mode = lower_ver_file->f_mode;
	lower_ver_file->f_mode |= FMODE_READ;

	bytes = vfs_read(lower_ver_file, (char __user *) res_buffer, buf_len,
								&file_pos);
	lower_ver_file->f_mode = prev_mode;


	/* update our inode atime upon a successful lower read */
	if (bytes >= 0) {
		err = bytes;
		if (file_pos == lower_ver_file->f_inode->i_size)
			err = 0;
	}
	/* dentry open is called for the file in get version file */
	if (filp_close(lower_ver_file, NULL))
		pr_info("Failed to close the version file\n");
out:
	kfree(buffer);

	return err;

}

/*
 * 0 on success and restored file name stored in res_buffer,
 * errno on failure
 */
static int bkpfs_restore_version_file(struct file *file, char *version,
					char __user *res_buffer, int buf_len)
{

	void *buffer = NULL;
	int err = 0;
	int max_ver = 0;
	int ver = 0;
	int i = 0;
	int version_num = 0;
	struct file *lower_file = NULL;
	struct file *lower_ver_file = NULL;
	fmode_t prev_mode;
	const char *restore_filename;
	int max_ver_count = -1;
	int count = 0;

	/* generate appropriate version num from user argument version */
	buffer = kmalloc(MAX_BKP_NUM_LEN, GFP_KERNEL);
	err = bkpfs_getxattr_version(file->f_path.dentry, file->f_inode,
				BKPFS_XATTR_MAX_VERSION, buffer, MAX_BKP_NUM_LEN);
	if (err > 0)
		max_ver = *(int *)buffer;
	else {
		if (err == -ENODATA)
			err = -EINVAL;
		goto out;
	}
	if (strcmp(version, "newest") != 0) {

		/* convert version to version_num */
		i = 0;
		while (version[i] != '\0' &&
				(version[i] >= '0' && version[i] <= '9')) {
			version_num = version_num*10 + (version[i]-'0');
			i++;
		}
		if (version[i] != '\0') {
			err = -EINVAL;
			goto out;
		}

		/* check if version > max_ver or version count */
		max_ver_count = bkpfs_get_super_ver(file->f_inode->i_sb);
		if (max_ver_count == -1)
			max_ver_count = BKPFS_MAX_VERSIONS;
		count = bkpfs_get_version_count(file);
		if (count < 0) {
			err = count;
			goto out;
		}
		if (version_num > max_ver_count || version_num > count) {
			err = -EINVAL;
			goto out;
		}

		err = bkpfs_getxattr_version(file->f_path.dentry, file->f_inode,
				BKPFS_XATTR_MIN_VERSION, buffer, MAX_BKP_NUM_LEN);
		if (err > 0) {
			ver = *(int *)buffer;
			ver = ver + version_num - 1;
		} else {
			if (err == -ENODATA)
				err = -EINVAL;
			goto out;
		}
	} else {
		ver = max_ver;
	}

	/* get the required version file to be restored */
	lower_ver_file = bkpfs_get_version_file(file, ver);
	if (IS_ERR_OR_NULL(lower_ver_file)) {
		pr_info("Failed to get the required version file\n");
		err = PTR_ERR(lower_ver_file);
		goto out;
	}

	/* get new original file to restore data in */
	lower_file = bkpfs_create_restore_file(file, ver);
	if (IS_ERR_OR_NULL(lower_file)) {
		pr_info("Failed to create the restore file\n");
		err = PTR_ERR(lower_file);
		goto out_file;
	}

	/* copy data from version file to restore file */
	prev_mode = lower_ver_file->f_mode;
	lower_ver_file->f_mode |= FMODE_READ;

	err = bkpfs_copy_file_data(lower_ver_file, lower_file,
					lower_ver_file->f_inode->i_size, 0);
	lower_ver_file->f_mode = prev_mode;
	if (err < 0) {
		pr_info("Failed to restore the version file\n");
		goto out_res_file;
	}

	restore_filename = lower_file->f_path.dentry->d_name.name;
	err = copy_to_user(res_buffer, restore_filename,
					strlen(restore_filename));

out_res_file:
	if (filp_close(lower_file, NULL))
		pr_info("Failed to close the restored file\n");

out_file:
	if (filp_close(lower_ver_file, NULL))
		pr_info("Failed to close the version file\n");

out:
	kfree(buffer);

	return err;

}

static ssize_t bkpfs_write(struct file *file, const char __user *buf,
			    size_t count, loff_t *ppos)
{

	int err;
	struct file *lower_file;
	struct dentry *dentry = file->f_path.dentry;
	int copy_file = 0;
	int ver_count = 0;
	int max_ver_count;
	int bytes = 0;
	int ver = -1;

	lower_file = bkpfs_lower_file(file);
	bytes = vfs_write(lower_file, buf, count, ppos);

	/* update our inode times+sizes upon a successful lower write */
	if (bytes >= 0) {
		fsstack_copy_inode_size(d_inode(dentry),
					file_inode(lower_file));
		fsstack_copy_attr_times(d_inode(dentry),
					file_inode(lower_file));
	} else {
		pr_info("Error from original file write\n");
		return bytes;
	}

	/* check if new version needs to be created or not */
	copy_file = bkpfs_may_create_version(file, count);
	if (copy_file == 1) {
		/* unlink old version if version count exceeeds max versions */
		ver_count = bkpfs_get_version_count(file);
		if (ver_count < 0) {
			err = ver_count;
			goto out;
		}
		max_ver_count = bkpfs_get_super_ver(file->f_inode->i_sb);
		if (max_ver_count == -1)
			max_ver_count = BKPFS_MAX_VERSIONS;
		while (ver_count >= max_ver_count) {
			ver = bkpfs_update_min_version(file, 1);
			if (ver < 0) {
				err = ver;
				goto out;
			}
			err = bkpfs_unlink_version_file(file, ver);
			if (err) {
				bkpfs_update_min_version(file, -1);
				goto out;
			}
			ver_count--;
		}
		/* copy data from file to new version file and create backup */
		err = bkpfs_copy_to_version_file(file);
	} else if (copy_file < 0) {
		err = copy_file;
	}

out:
	if (err < 0)
		pr_info("Failed to create the backup file\n");

	return bytes;
}

struct bkpfs_getdents_callback {
	struct dir_context ctx;
	struct dir_context *caller;
	struct super_block *sb;
	int filldir_called;
	int entries_written;
};


/*
 * handle . and .. entry in directory
 * returns 1 if . or .. else 0
 */
static int is_dot_dotdot(const char *name, int name_size)
{
	if (name_size == 1 && name[0] == '.')
		return 1;
	else if (name_size == 2 && name[0] == '.' && name[1] == '.')
		return 1;

	return 0;
}

/*
 * filter version files to hide in ls or rm file
 * returns -EINVAL for version files
 * else 0 for other files/dir
 */
static int bkpfs_filter_version_files(const char *name, int name_size)
{
	int ret = 0;
	int j = 0;

	/* handle . and .. in dir */
	if (is_dot_dotdot(name, name_size))
		goto out;

	/* file name cannot be version file (.name_v#) */
	if (name_size < 4)
		goto out;

	/* check for version file */
	if (name[0] == '.') {
		j = name_size-1;
		while (j >= 0 && (name[j] >= '0' && name[j] <= '9'))
			j--;
		if (j != name_size-1 && j >= 1) {
			if (name[j] == 'v' && name[j-1] == '_')
				ret = -EINVAL;
		}
	}

out:
	return ret;
}

static int
bkpfs_filldir(struct dir_context *ctx, const char *lower_name,
		 int lower_namelen, loff_t offset, u64 ino, unsigned int d_type)
{

	struct bkpfs_getdents_callback *buf =
		container_of(ctx, struct bkpfs_getdents_callback, ctx);
	int rc;

	buf->filldir_called++;
	rc = bkpfs_filter_version_files(lower_name, lower_namelen);
	if (rc) {
		if (rc == -EINVAL) {
			return 0;
		}
		return rc;
	}

	buf->caller->pos = buf->ctx.pos;
	rc = !dir_emit(buf->caller, lower_name, lower_namelen, ino, d_type);
	if (!rc)
		buf->entries_written++;

	return rc;
}

static int bkpfs_readdir(struct file *file, struct dir_context *ctx)
{
	int err;
	struct file *lower_file = NULL;
	struct inode *inode = file_inode(file);
	struct bkpfs_getdents_callback buf = {
		.ctx.actor = bkpfs_filldir,
		.caller = ctx,
		.sb = inode->i_sb,
	};

	lower_file = bkpfs_lower_file(file);
	err = iterate_dir(lower_file, &buf.ctx);
	file->f_pos = lower_file->f_pos;
	ctx->pos = buf.ctx.pos;

	if (err < 0)
		goto out;
	if (buf.filldir_called && !buf.entries_written)
		goto out;

	if (err >= 0)		/* copy the atime */
		fsstack_copy_attr_atime(inode,
					file_inode(lower_file));

out:
	return err;
}

/*
 * translated user memory to kernel memory with validation
 * 0 on success, errno on failure
 */
static int memory_translate(void *dest, void *src, int n_bytes)
{
	int err = -1;
	int rem_bytes = -1;

	err = access_ok(VERIFY_READ, src, n_bytes);
	if (!err)
		return -EFAULT;

	rem_bytes = copy_from_user(dest, src, n_bytes);
	if (rem_bytes)
		return -EAGAIN;

	return 0;
}

static long bkpfs_unlocked_ioctl(struct file *file, unsigned int cmd,
				  unsigned long arg)
{
	long err = -ENOTTY;
	struct file *lower_file;
	char *buffer = NULL;
	int ver = 0;
	unsigned long readargs_size = 0;
	void *kaddr_void = NULL;
	struct ioctl_readargs *kaddr = NULL;
	char *version;
	struct ioctl_readargs __user *karg = (struct ioctl_readargs *)arg;
	int count = 0;

	lower_file = bkpfs_lower_file(file);

	/* XXX: use vfs_ioctl if/when VFS exports it */
	if (!lower_file || !lower_file->f_op)
		goto origfile_out;
	if (lower_file->f_op->unlocked_ioctl)
		err = lower_file->f_op->unlocked_ioctl(lower_file, cmd, arg);

	/* some ioctls can change inode attributes (EXT2_IOC_SETFLAGS) */
	if (!err)
		fsstack_copy_attr_all(file_inode(file),
				      file_inode(lower_file));

origfile_out:

	err = 0;

	count = bkpfs_get_version_count(file);
	if (count < 0) {
		err = count;
		return err;
	}
	if (count == 0 && (cmd != BKPFS_VER_LIST)) {
		err = -EINVAL;
		return err;
	}

	/* memory translation for the struct readargs  */
	readargs_size = sizeof(struct ioctl_readargs);

	kaddr_void = kmalloc(readargs_size, GFP_KERNEL);

	err = memory_translate(kaddr_void, (void *) arg, readargs_size);
	if (err) {
		if (err == -EFAULT)
			pr_info("Bad address received for struct readargs\n");
		else if (err == -EAGAIN)
			pr_info("Struct readargs could not be copied\n");
		else
			pr_info("Failed to copy readargs struct to kernel\n");
		goto out;
	}

	kaddr = kaddr_void;
	pr_info("Struct readargs copied to kernel\n");

	if (kaddr->version_len == 0 || kaddr->buf_len == 0) {
		err = -EINVAL;
		kfree(kaddr_void);
		return err;
	}

	/* memory translation for version */
	version = kmalloc(kaddr->version_len + 1, GFP_KERNEL);

	err = memory_translate((void *) version, (void *) kaddr->version,
							kaddr->version_len);
	if (err) {
		if (err == -EFAULT)
			pr_info("Bad address received for version\n");
		else if (err == -EAGAIN)
			pr_info("Version could not be copied\n");
		else
			pr_info("Failed to copy version to kernel\n");
		goto out;
	}

	version[kaddr->version_len] = '\0';

	switch (cmd) {
	/* list versions */
	case BKPFS_VER_LIST:
		buffer = kzalloc(kaddr->buf_len, GFP_KERNEL);

		err = bkpfs_list_file_versions(file, buffer);
		if (err > 0)
			err = copy_to_user((void __user *) karg->buffer,
					(void *) buffer, kaddr->buf_len);
		if (err)
			pr_info("Failed to fetch the version list\n");

		kfree(buffer);
		break;
	/* delete version */
	case BKPFS_VER_DEL:
		if (strcmp(version, "oldest") == 0) {
			ver = bkpfs_update_min_version(file, 1);
			if (ver < 0) {
				err = ver;
			} else {
				err = bkpfs_unlink_version_file(file, ver);
				if (err)
					bkpfs_update_min_version(file, -1);
			}
		} else if (strcmp(version, "newest") == 0) {
			ver = bkpfs_update_max_version(file, -1) + 1;
			if (ver < 0)
				err = ver;
			else
				err = bkpfs_unlink_version_file(file, ver);
			if (err)
				bkpfs_update_max_version(file, 1);
		} else if (strcmp(version, "all") == 0) {
			err = bkpfs_unlink_all_versions(file);
		} else {
			pr_info("Invalid option in delete\n");
			err = -EINVAL;
		}

		if (err)
			pr_info("Failed to delete the version file(s)\n");
		break;
	/* view version */
	case BKPFS_VER_VIEW:
		err = bkpfs_get_version_file_data(file, version,
			(char __user *) karg->buffer, karg->buf_len, karg->pos);
		if (err > 0) {
			pr_info("Bytes copied : %ld\n", err);
			karg->pos += err;
		} else if (err < 0) {
			pr_info("Failed to read data from the version file\n");
		} else {
			pr_info("End of file reached\n");
			karg->pos = 0;
		}
		break;
	/* restore version file */
	case BKPFS_VER_RESTORE:
		err = bkpfs_restore_version_file(file, version,
				(char __user *) karg->buffer, karg->buf_len);
		if (err)
			pr_info("Failed to restore the version file\n");

		break;
	default:
		pr_info("Inside default ioctl cmd\n");
		err = -EINVAL;
		break;
	}

out:
	kfree(version);
	kfree(kaddr_void);

	return err;

}

#ifdef CONFIG_COMPAT
static long bkpfs_compat_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	long err = -ENOTTY;
	struct file *lower_file;

	lower_file = bkpfs_lower_file(file);

	/* XXX: use vfs_ioctl if/when VFS exports it */
	if (!lower_file || !lower_file->f_op)
		goto out;
	if (lower_file->f_op->compat_ioctl)
		err = lower_file->f_op->compat_ioctl(lower_file, cmd, arg);

out:
	return err;
}
#endif

static int bkpfs_mmap(struct file *file, struct vm_area_struct *vma)
{
	int err = 0;
	bool willwrite;
	struct file *lower_file;
	const struct vm_operations_struct *saved_vm_ops = NULL;

	/* this might be deferred to mmap's writepage */
	willwrite = ((vma->vm_flags | VM_SHARED | VM_WRITE) == vma->vm_flags);

	/*
	 * File systems which do not implement ->writepage may use
	 * generic_file_readonly_mmap as their ->mmap op.  If you call
	 * generic_file_readonly_mmap with VM_WRITE, you'd get an -EINVAL.
	 * But we cannot call the lower ->mmap op, so we can't tell that
	 * writeable mappings won't work.  Therefore, our only choice is to
	 * check if the lower file system supports the ->writepage, and if
	 * not, return EINVAL (the same error that
	 * generic_file_readonly_mmap returns in that case).
	 */
	lower_file = bkpfs_lower_file(file);
	if (willwrite && !lower_file->f_mapping->a_ops->writepage) {
		err = -EINVAL;
		pr_info(
		"bkpfs: lower file system does not support writeable mmap\n");
		goto out;
	}

	/*
	 * find and save lower vm_ops.
	 *
	 * XXX: the VFS should have a cleaner way of finding the lower vm_ops
	 */
	if (!BKPFS_F(file)->lower_vm_ops) {
		err = lower_file->f_op->mmap(lower_file, vma);
		if (err) {
			pr_info("bkpfs: lower mmap failed %d\n", err);
			goto out;
		}
		saved_vm_ops = vma->vm_ops; /* save: came from lower ->mmap */
	}

	/*
	 * Next 3 lines are all I need from generic_file_mmap.  I definitely
	 * don't want its test for ->readpage which returns -ENOEXEC.
	 */
	file_accessed(file);
	vma->vm_ops = &bkpfs_vm_ops;

	file->f_mapping->a_ops = &bkpfs_aops; /* set our aops */
	if (!BKPFS_F(file)->lower_vm_ops) /* save for our ->fault */
		BKPFS_F(file)->lower_vm_ops = saved_vm_ops;

out:
	return err;
}

static int bkpfs_open(struct inode *inode, struct file *file)
{
	int err = 0;
	struct file *lower_file = NULL;
	struct path lower_path;

	/* don't open unhashed/deleted files */
	if (d_unhashed(file->f_path.dentry)) {
		err = -ENOENT;
		goto out_err;
	}

	file->private_data =
		kzalloc(sizeof(struct bkpfs_file_info), GFP_KERNEL);
	if (!BKPFS_F(file)) {
		err = -ENOMEM;
		goto out_err;
	}


	/* open lower object and link bkpfs's file struct to lower's */
	bkpfs_get_lower_path(file->f_path.dentry, &lower_path);
	lower_file = dentry_open(&lower_path, file->f_flags, current_cred());
	path_put(&lower_path);
	if (IS_ERR(lower_file)) {
		err = PTR_ERR(lower_file);
		lower_file = bkpfs_lower_file(file);
		if (lower_file) {
			bkpfs_set_lower_file(file, NULL);
			fput(lower_file); /* fput calls dput for lower_dentry */
		}
	} else {
		bkpfs_set_lower_file(file, lower_file);
	}

	if (err)
		kfree(BKPFS_F(file));
	else
		fsstack_copy_attr_all(inode, bkpfs_lower_inode(inode));
out_err:
	return err;
}

static int bkpfs_flush(struct file *file, fl_owner_t id)
{
	int err = 0;
	struct file *lower_file = NULL;
	int copy_file = 0;
	int ver_count = 0;
	int max_ver_count;
	int ver = -1;

	/* check if new version needs to be created or not */
	copy_file = bkpfs_may_create_version(file, -1);
	if (copy_file == 1) {
		/* unlink old version if version count exceeeds max versions */
		ver_count = bkpfs_get_version_count(file);
		if (ver_count < 0) {
			err = ver_count;
			goto out;
		}
		max_ver_count = bkpfs_get_super_ver(file->f_inode->i_sb);
		if (max_ver_count == -1)
			max_ver_count = BKPFS_MAX_VERSIONS;

		while (ver_count >= max_ver_count) {
			ver = bkpfs_update_min_version(file, 1);
			if (ver < 0) {
				err = ver;
				goto out;
			}
			err = bkpfs_unlink_version_file(file, ver);
			if (err) {
				bkpfs_update_min_version(file, -1);
				goto out;
			}
			ver_count--;
		}
		/* copy data from file to new version file and create backup */
		err = bkpfs_copy_to_version_file(file);
	} else if (copy_file < 0) {
		err = copy_file;
	}

out:
	if (err < 0)
		pr_info("Failed to create the backup file\n");
	err = 0;

	lower_file = bkpfs_lower_file(file);
	if (lower_file && lower_file->f_op && lower_file->f_op->flush) {
		filemap_write_and_wait(file->f_mapping);
		err = lower_file->f_op->flush(lower_file, id);
	}

	return err;
}

/* release all lower object references & free the file info structure */
static int bkpfs_file_release(struct inode *inode, struct file *file)
{
	struct file *lower_file;

	lower_file = bkpfs_lower_file(file);
	if (lower_file) {
		bkpfs_set_lower_file(file, NULL);
		fput(lower_file);
	}

	kfree(BKPFS_F(file));
	return 0;
}

static int bkpfs_fsync(struct file *file, loff_t start, loff_t end,
			int datasync)
{
	int err;
	struct file *lower_file;
	struct path lower_path;
	struct dentry *dentry = file->f_path.dentry;

	err = __generic_file_fsync(file, start, end, datasync);
	if (err)
		goto out;
	lower_file = bkpfs_lower_file(file);
	bkpfs_get_lower_path(dentry, &lower_path);
	err = vfs_fsync_range(lower_file, start, end, datasync);
	bkpfs_put_lower_path(dentry, &lower_path);
out:
	return err;
}

static int bkpfs_fasync(int fd, struct file *file, int flag)
{
	int err = 0;
	struct file *lower_file = NULL;

	lower_file = bkpfs_lower_file(file);
	if (lower_file->f_op && lower_file->f_op->fasync)
		err = lower_file->f_op->fasync(fd, lower_file, flag);

	return err;
}

/*
 * Bkpfs cannot use generic_file_llseek as ->llseek, because it would
 * only set the offset of the upper file.  So we have to implement our
 * own method to set both the upper and lower file offsets
 * consistently.
 */
static loff_t bkpfs_file_llseek(struct file *file, loff_t offset, int whence)
{
	int err;
	struct file *lower_file;

	err = generic_file_llseek(file, offset, whence);
	if (err < 0)
		goto out;

	lower_file = bkpfs_lower_file(file);
	err = generic_file_llseek(lower_file, offset, whence);

out:
	return err;
}

/*
 * Bkpfs read_iter, redirect modified iocb to lower read_iter
 */
ssize_t
bkpfs_read_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	int err;
	struct file *file = iocb->ki_filp, *lower_file;

	lower_file = bkpfs_lower_file(file);
	if (!lower_file->f_op->read_iter) {
		err = -EINVAL;
		goto out;
	}

	get_file(lower_file); /* prevent lower_file from being released */
	iocb->ki_filp = lower_file;
	err = lower_file->f_op->read_iter(iocb, iter);
	iocb->ki_filp = file;
	fput(lower_file);
	/* update upper inode atime as needed */
	if (err >= 0 || err == -EIOCBQUEUED)
		fsstack_copy_attr_atime(d_inode(file->f_path.dentry),
					file_inode(lower_file));
out:
	return err;
}

/*
 * Bkpfs write_iter, redirect modified iocb to lower write_iter
 */
ssize_t
bkpfs_write_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	int err;
	struct file *file = iocb->ki_filp, *lower_file;

	lower_file = bkpfs_lower_file(file);
	if (!lower_file->f_op->write_iter) {
		err = -EINVAL;
		goto out;
	}

	get_file(lower_file); /* prevent lower_file from being released */
	iocb->ki_filp = lower_file;
	err = lower_file->f_op->write_iter(iocb, iter);
	iocb->ki_filp = file;
	fput(lower_file);
	/* update upper inode times/sizes as needed */
	if (err >= 0 || err == -EIOCBQUEUED) {
		fsstack_copy_inode_size(d_inode(file->f_path.dentry),
					file_inode(lower_file));
		fsstack_copy_attr_times(d_inode(file->f_path.dentry),
					file_inode(lower_file));
	}
out:
	return err;
}

const struct file_operations bkpfs_main_fops = {
	.llseek		= generic_file_llseek,
	.read		= bkpfs_read,
	.write		= bkpfs_write,
	.unlocked_ioctl	= bkpfs_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= bkpfs_compat_ioctl,
#endif
	.mmap		= bkpfs_mmap,
	.open		= bkpfs_open,
	.flush		= bkpfs_flush,
	.release	= bkpfs_file_release,
	.fsync		= bkpfs_fsync,
	.fasync		= bkpfs_fasync,
	.read_iter	= bkpfs_read_iter,
	.write_iter	= bkpfs_write_iter,
};

/* trimmed directory options */
const struct file_operations bkpfs_dir_fops = {
	.llseek		= bkpfs_file_llseek,
	.read		= generic_read_dir,
	.iterate	= bkpfs_readdir,
	.unlocked_ioctl	= bkpfs_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= bkpfs_compat_ioctl,
#endif
	.open		= bkpfs_open,
	.release	= bkpfs_file_release,
	.flush		= bkpfs_flush,
	.fsync		= bkpfs_fsync,
	.fasync		= bkpfs_fasync,
};
