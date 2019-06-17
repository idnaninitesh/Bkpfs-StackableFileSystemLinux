#!/bin/sh:
# test call for viewing oldest and newest on a file with one version created
# expected behavior :
# first list shows 1 version (1)
# first view and second view produce the same output
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
# execute create_one_version to create one version
.././create_one_version $test_file
# execute bkpctl to view oldest version
.././bkpctl -v oldest $test_file
retval=$?
if test $retval != 0 ; then
        echo bkpctl failed with error: $retval
        exit $retval
else
        echo bkpctl program successful
fi
# execute bkpctl to view newest version
.././bkpctl -v newest $test_file
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
