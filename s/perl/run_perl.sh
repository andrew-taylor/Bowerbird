#!/bin/sh

case "$0" in
*/*) perl_dir=`dirname "$0"` ;;
*)  saved_ifs="$IFS"
    IFS=:
	for d in $PATH
    do
      IFS="$saved_ifs"
      test -x "$d/$0" && perl_dir="$d" && break
    done
    ;;
esac

perl_dir="$perl_dir/../perl"
program_name="`basename $0`"
script="$perl_dir/$program_name.pl"
ls -l $script
if test ! -r "$script"
then
	echo "$0: internal error: can not find script $program_name.pl" 1>&2
	exit 1
fi

exec perl -w "-I$perl_dir" "$script" "$@"
