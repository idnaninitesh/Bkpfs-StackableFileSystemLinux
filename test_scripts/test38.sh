#!/bin/sh:
# test call for listing files on a file with iterative open,write,close calls > PAGE_SIZE
# made in the file
# expected : return 0(success) and print the list of version files
# multiple version files in installment of 16 bytes write(each iter) since close also created
# backup if file has been modified
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
# execute create_iter_write to write iteratively to the file > PAGE_SIZE bytes
.././create_iter_opclose $test_file
# execute bkpctl to list versions
.././bkpctl -l $test_file
retval=$?
if test $retval != 0 ; then
        echo bkpctl failed with error: $retval
        exit $retval
else
        echo bkpctl program successful
fi
'''
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
'''
