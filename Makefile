all: bkpctl create_mul_versions create_no_versions create_one_version create_write_pagesize create_iter_write create_iter_opclose

bkpctl: bkpctl.c
	gcc -Wall -Werror -I ../include/ bkpctl.c -o bkpctl

create_mul_versions: create_mul_versions.c
	gcc -Wall -Werror create_mul_versions.c -o create_mul_versions

create_no_versions: create_no_versions.c
	gcc -Wall -Werror create_no_versions.c -o create_no_versions

create_one_version: create_one_version.c
	gcc -Wall -Werror create_one_version.c -o create_one_version

create_write_pagesize: create_write_pagesize.c
	gcc -Wall -Werror create_write_pagesize.c -o create_write_pagesize

create_iter_write: create_iter_write.c
	gcc -Wall -Werror create_iter_write.c -o create_iter_write

create_iter_opclose: create_iter_opclose.c
	gcc -Wall -Werror create_iter_opclose.c -o create_iter_opclose

clean:
	rm -f bkpctl
	rm -f create_mul_versions
	rm -f create_no_versions
	rm -f create_one_version
	rm -f create_write_pagesize
	rm -f create_iter_write
	rm -f create_iter_opclose
