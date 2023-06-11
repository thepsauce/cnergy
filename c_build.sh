#!/bin/bash

if [ $# -ne 0 ] && [ "$1" = "-update" ]
then
	set -e
	LASTDIR="$(pwd)"
	SEARCHFAILED=true

	GITDIR=$(find ~ -type d -name "C-Shell-Builder")
	mkdir -p ~/mylibs/bin
	if [ -d "$GITDIR/.git" ]
	then
		cd "$GITDIR"
		SEARCHFAILED=false
		git pull
	else
		git clone https://github.com/MordorHD/C-Shell-Builder.git
		GITDIR="C-Shell-Builder"
	fi

	cd ~/mylibs/bin
	gcc -O3 "$GITDIR/fsub.c" -o fsub
	cp "$GITDIR/c_build.sh" ./

	if $SEARCHFAILED
	then
		rm -rf C-Shell-Builder
	fi

	cd "$LASTDIR"
	exit 0
fi

project_name=$(basename "$(pwd)")
project_name=${project_name,,}
if [ ! -f .project ]
then
	read -n 1 -p "First call of c, do you want to initialize the project '$project_name'? [yn] " yn
	echo
	case $yn in
		y) ;;
		*) echo "Cancelled" ; exit ;;
	esac
	mkdir src
	mkdir include
	mkdir build
	mkdir tests
	echo -e "#include \"$project_name.h\"\n\nint\nmain(int argc, char **argv)\n{\n\treturn 0;\n}\n" > src/main.c
	echo -e "#ifndef INCLUDED_${project_name^^}_H\n#define INCLUDED_${project_name^^}_H\n\n\n#endif" > include/$project_name.h
	touch .project
	exit 0
fi

# Time the build
START_TIME=$(date +%s.%N)
# flags for building
gcc_flags=
# linker options
gcc_links=
# if the program should be executed at the end
do_execute=false
# program arguments for the program to be executed
args=
# if the out file should be rebuilt
do_update=false
# if everything should be rebuilt
do_rebuild=false
# -test [name] can be used to test a program instead of running main.c
test_program=
# if the program runs longer than this time, it's terminated
max_runtime=
# exclude the specified items from the .project file
exclude=
home_install=
# flags 
while [ $# -ne 0 ]
do
	case "$1" in
	-x) do_execute=true ;;
	-B) do_rebuild=true ;;
	-test)
		shift
		if [ $# -eq 0 ] ; then echo "expected test program after -test" ; exit 1 ; fi
		test_program="$1"
		;;
	-time)
		shift
		if [ $# -eq 0 ] ; then echo "expected time value (e.g. 1s or 2h (see timeout --help)) after -time" ; exit 1 ; fi
		max_runtime="$1"
		;;
	-e)
		shift
		if [ $# -eq 0 ] ; then echo "expected comma separated exclude list after -e" ; exit 1 ; fi
		do_rebuild=true
		exclude="$1"
		;;
	-l* | -I* | -L*) gcc_links="$gcc_links $1" ;;
	-g | -pg | -O* | -f* | -W* | -D* | -U* | -m* | -std=*) gcc_flags="$gcc_flags $1" ;;
	--home-install)
		shift
		if [ $# -eq 0 ] || ( [ "$1" != "all" ] && [ "$1" != "include" ] && [ "$1" != "bin" ] )
		then echo "expected either 'all', 'include' or 'bin' after '--home-install'" ; exit 1 ; fi
		if [ "$1" = "all" ]
		then
			home_install="include bin"
		else
			home_install="$1"
		fi
		;;
	--) shift ; break ;;
	-*) echo "can not recognize flag $1" ; exit 1 ;;
	*) break ;;
	esac
	shift
done

while [ $# -ne 0 ]
do
	args="$args \"$1\""
	shift
done

if [ -n "$test_program" ] && $do_execute
then
	echo "-x and -t are conflicting"
	exit 1
fi

# Collect project information from the .project file
old_gcc_flags=
old_gcc_links=
if [ -f .project ]
then
	exec 6<.project
	read old_gcc_flags <&6
	read old_gcc_links <&6
	# Check if any gcc flags have changed
	for f in $gcc_flags
	do
		case "$old_gcc_flags" in
		*$f*) ;;
		*)
			old_gcc_flags="$old_gcc_flags $f"
			do_rebuild=true
			;;
		esac
	done
	for f in $gcc_links
	do
		case "$old_gcc_links" in
		*$f*) ;;
		*)
			old_gcc_links="$old_gcc_links $f"
			do_update=true
			;;
		esac
	done
	IFS=,
	for f in $exclude
	do
		old_gcc_flags=${old_gcc_flags/$f}
		old_gcc_links=${old_gcc_links/$f}
	done
	unset IFS
	gcc_flags="$old_gcc_flags"
	gcc_links="$old_gcc_links"
else
	do_rebuild=true
fi
# Write back the information
echo -e "$gcc_flags\n$gcc_links" > .project

# Check if any header file changed
headers=$(find include -name "*.h")
for h in $headers
do
	if [ "$h" -nt "out" ]
	then
		do_rebuild=true
		break
	fi
done

# If the main header file changed, rebuild it
if $do_rebuild
then
	echo "gcc \"include/$project_name.h\" -Iinclude"
	gcc "include/$project_name.h" -Iinclude
elif [ "include/$project_name.h" -nt "out" ]
then
	do_rebuild=true
	echo "gcc \"include/$project_name.h\" -Iinclude"
	gcc "include/$project_name.h" -Iinclude
fi

# Collect source files and build all object files
sources=$(find src -name "*.c")
objects=
for s in $sources
do
	o=build/${s:4}.o
	objects="$objects $o"
	if $do_rebuild || [ $s -nt $o ]
	then
		mkdir -p $(dirname "$o")
		echo "gcc $gcc_flags -c $s -o $o -Iinclude"
		if !  gcc $gcc_flags -c $s -o $o -Iinclude ; then exit 1 ; fi
		do_update=true
	fi
done

# Building final program
if $do_update || $do_rebuild
then
	echo "gcc $gcc_flags $objects -o out $gcc_links"
	if !  gcc $gcc_flags $objects -o out $gcc_links ; then exit 1 ; fi
	END_TIME=$(date +%s.%N)
	ELAPSED_TIME=$(fsub $END_TIME $START_TIME)
	echo -e "build time: \e[36m$ELAPSED_TIME\e[0m seconds"
else
	echo "Everything is up to date!"
fi

# program to execute
program=

# Building test program if needed
if [ -n "$test_program" ]
then
	# exclude main object and include test object
	objects="${objects/'build/main.c.o'/} build/$test_program.c.o"
	echo "gcc $gcc_flags -c tests/$test_program.c -o build/$test_program.c.o -Iinclude"
	if !  gcc $gcc_flags -c tests/$test_program.c -o build/$test_program.c.o -Iinclude ; then exit 1 ; fi
	echo "gcc $gcc_flags $objects -o test $gcc_links"
	if !  gcc $gcc_flags $objects -o test $gcc_links ; then exit 1 ; fi
	program=test
elif $do_execute
then
	program=out
fi

# Home installing if wanted
for i in  $home_install
do
	case $i in
	include )
		mkdir -p "$HOME/mylibs/include/"
		cp -i include/*.h "$HOME/mylibs/include/"
		;;
	bin )
		mkdir -p "$HOME/mylibs/bin/"
		cp -i out "$HOME/mylibs/bin/$project_name"
		;;
	esac
done
if [ -n "$home_install" ]
then
	echo "Finished home install '$home_install'"
fi

# Executing if wanted
if [ -n "$program" ]
then
	START_TIME=$(date +%s.%N)
	if [ -n "$max_runtime" ]
	then
		timeout "$max_runtime" ./$program $args
	else
		./$program $args
	fi
	EXIT_CODE=$?
	END_TIME=$(date +%s.%N)
	ELAPSED_TIME=$(fsub $END_TIME $START_TIME)
	echo -e "exit code: \e[36m$EXIT_CODE\e[0m; elapsed time: \e[36m$ELAPSED_TIME\e[0m seconds"
fi
