#!/bin/sh:
# do insmod and mount as root, create file as root, remove read permsission
# from the file as root and then login to student (su - student) and run
# the script
# test call for listing files on a file which does not have read permissions
# expected : return error as the file does not have read permissions
set -x
test_dir=$PWD/../../test_dir
test_file=$BKPFS_MNT_DIR/$BKPFS_TEST_FILE
bkpfs_src_dir=$PWD/../../fs/bkpfs
# execute bkpctl to list version files
.././bkpctl -l $test_file
retval=$?
if test $retval != 0 ; then
        echo bkpctl failed with error: $retval
        exit $retval
else
        echo bkpctl program successful
fi
