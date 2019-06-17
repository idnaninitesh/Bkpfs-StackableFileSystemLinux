#!/bin/sh:
# test call for restoring version 1 and newest on a file with one version created
# expected behavior :
# show the list of version files (1)
# newest file name (test_file_v1) exist should be false
# newest file should be restored
# newest file name (test_file_v1) exist should be true
# version 1 file should be restored (same as newest)
# version 1 file name (test_file_v1) exist should be true
set -x
test_dir=$PWD/../../test_dir
test_file=$BKPFS_MNT_DIR/$BKPFS_TEST_FILE
bkpfs_src_dir=$PWD/../../fs/bkpfs
ver='_v1'
restore_file=$BKPFS_MNT_DIR/$BKPFS_TEST_FILE$ver
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
# execute bkpctl to list versions
.././bkpctl -l $test_file
retval=$?
if test $retval != 0 ; then
        echo bkpctl failed with error: $retval
        exit $retval
else
        echo bkpctl program successful
fi
# check if restored file is created or not
if [ ! -f $restore_file ]; then
        echo 'Restored file does not exist'
else
	ls -l $restore_file
fi
# execute bkpctl to restore newest version
.././bkpctl -r newest $test_file
retval=$?
if test $retval != 0 ; then
        echo bkpctl failed with error: $retval
        exit $retval
else
        echo bkpctl program successful
fi
# check if restored file is created or not
if [ ! -f $restore_file ]; then
        echo 'Restored file does not exist'
else
	ls -l $restore_file
fi
# execute bkpctl to restore version 1
.././bkpctl -r 1 $test_file
retval=$?
if test $retval != 0 ; then
        echo bkpctl failed with error: $retval
        exit $retval
else
        echo bkpctl program successful
fi
# check if restored file is created or not
if [ ! -f $restore_file ]; then
        echo 'Restored file does not exist'
else
	ls -l $restore_file
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
