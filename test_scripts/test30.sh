#!/bin/sh:
# test call for multiple ops i.e. delete newest, restore newest, delete newest,
# restore newest for file multiple ver
# expected behavior :
# first list shows 4 versions (1-4)
# delete newest i.e. v4
# second list shows 3 versions (1-3) since 4 was the newest version
# file exist test_file_v3 should return false
# restore newest should restore v3
# file exist test_file_v3 should return true
# delete newest i.e. v3
# third list shows 2 versions (1-2) since 3 was the newest version
# file exist test_file_v2 should return false
# restore newest should restore v2
# file exist test_file_v2 should return true
set -x
test_dir=$PWD/../../test_dir
test_file=$BKPFS_MNT_DIR/$BKPFS_TEST_FILE
bkpfs_src_dir=$PWD/../../fs/bkpfs
ver='_v3'
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
# execute create_mul_versions to create multiple version
.././create_mul_versions $test_file
# execute bkpctl to list versions
.././bkpctl -l $test_file
retval=$?
if test $retval != 0 ; then
        echo bkpctl failed with error: $retval
        exit $retval
else
        echo bkpctl program successful
fi
# execute bkpctl to delete newest version
.././bkpctl -d newest $test_file
retval=$?
if test $retval != 0 ; then
        echo bkpctl failed with error: $retval
        exit $retval
else
        echo bkpctl program successful
fi
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
# execute bkpctl to delete newest version
.././bkpctl -d newest $test_file
retval=$?
if test $retval != 0 ; then
        echo bkpctl failed with error: $retval
        exit $retval
else
        echo bkpctl program successful
fi

# execute bkpctl to list versions
.././bkpctl -l $test_file
retval=$?
if test $retval != 0 ; then
        echo bkpctl failed with error: $retval
        exit $retval
else
        echo bkpctl program successful
fi
ver='_v2'
restore_file=$BKPFS_MNT_DIR/$BKPFS_TEST_FILE$ver
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
