#!/bin/sh:
# test call to indicate what happens when ver count exceeds max versions(=5)
# expected behavior:
# first list command shows version 1 to 4
# next list command shows version 4-8 since version 1,2,3 are removed when
# 6,7,8 are added respectively
set -x
test_dir=$PWD/../../test_dir
test_file=$BKPFS_MNT_DIR/$BKPFS_TEST_FILE
bkpfs_src_dir=$PWD/../../fs/bkpfs
# check if dir is mounted then unmount it
if grep -qs $BKPFS_MNT_DIR /proc/mounts ; then
	umount $BKPFS_MNT_DIR
fi
# unload the module if it already exists
if lsmod | grep -qs bkpfs ; then
	rmmod bkpfs
fi
# clean or create the dirs
if [ ! -d $BKPFS_MNT_DIR ]; then
        mkdir $BKPFS_MNT_DIR
else
	rm -rf $BKPFS_MNT_DIR/*
	rm -rf $BKPFS_MNT_DIR/.*_v*
fi
if [ ! -d $test_dir ]; then
	mkdir $test_dir
else
	rm -rf $test_dir/*
	rm -rf $test_dir/.*_v*
fi
# insert the new module for consistency
if ! lsmod | grep -qs bkpfs ; then
	insmod $bkpfs_src_dir/bkpfs.ko
fi
# mount the dir anew
if ! grep -qs $BKPFS_MNT_DIR /proc/mounts ; then
	mount -t bkpfs -o maxver=$BKPFS_MNT_VERSIONS $test_dir $BKPFS_MNT_DIR
fi
# execute create_mul_versions to create multiple versions
.././create_mul_versions $test_file
# execute bkpctl to list version for 1st call
echo 'First list call'
.././bkpctl -l $test_file
retval=$?
if test $retval != 0 ; then
        echo bkpctl failed with error: $retval
        exit $retval
else
        echo bkpctl program successful
fi
# execute create_mul_versions to create versions again
.././create_mul_versions $test_file
# execute bkpctl to list version for 2nd call
echo 'Second list call'
.././bkpctl -l $test_file
retval=$?
if test $retval != 0 ; then
        echo bkpctl failed with error: $retval
        exit $retval
else
        echo bkpctl program successful
fi
# cleanup after script is completed
if grep -qs $BKPFS_MNT_DIR /proc/mounts ; then
	umount $BKPFS_MNT_DIR
fi
if lsmod | grep -qs bkpfs ; then
	rmmod bkpfs
fi
if [ -d $BKPFS_MNT_DIR ]; then
	rm -rf $BKPFS_MNT_DIR/*
	rm -rf $BKPFS_MNT_DIR/.*_v*
fi
if [ -d $test_dir ]; then
	rm -rf $test_dir/*
	rm -rf $test_dir/.*_v*
fi
