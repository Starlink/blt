dnl BLT_RUN_WITH_OUTPUT(VARIABLE, PROGRAM,)
AC_DEFUN(BLT_RUN_WITH_OUTPUT,
[AC_REQUIRE([AC_PROG_CC])dnl
if test "$cross_compiling" = yes; then
  ifelse([$3], ,
    [errprint(__file__:__line__: warning: [AC_TRY_RUN_WITH_OUTPUT] called without default to
 allow cross compiling
)dnl
  AC_MSG_ERROR(can not run test program while cross compiling)],
  [$3])
else
cat > conftest.$ac_ext <<EOF
[#]line __oline__ "configure"
[#include "confdefs.h"
#ifdef __cplusplus
extern "C" void exit(int);
#endif
]dnl
[$2]
EOF
eval $ac_link
if test -s conftest && (./conftest > ./conftest.stdout; exit) 2>/dev/null; then
   $1=`cat ./conftest.stdout`
else
   $1=""
fi
fi
rm -fr conftest*])

dnl BLT_GET_SYMBOL(VARIABLE, SYMBOL, FILE)
AC_DEFUN(BLT_GET_SYMBOL,
[AC_REQUIRE([AC_PROG_AWK])dnl
cat > conftest.awk <<EOF
[/^# *define *]$2[[ \t]]/ { print [\$][3] }
EOF
$1=`${AWK} -f conftest.awk "$3"`
rm -rf conftest*])

dnl BLT_CHECK_LIBRARY(NAME, SPEC, SYMBOL, WITH, EXTRALIBS)
AC_DEFUN(BLT_CHECK_LIBRARY,
[  
  if test "$4" != "no" ; then
    save_LDFLAGS="${LDFLAGS}"
    if test "$4" = "yes" ; then
      lib_spec="-l$2"
      dir=""
      LDFLAGS="${lib_spec} ${save_LDFLAGS}"
      AC_CHECK_LIB([$2], [$3], [found="yes"],[found="no"])
      if test "${found}" = "no" ; then
	lib_spec="-L${dir} -l$2 $5"
        dir=$exec_prefix/lib
        LDFLAGS="${lib_spec} ${save_LDFLAGS}"
        AC_CHECK_LIB([$2], [$3], [found="yes"],[found="no"])
        if test "${found}" = "yes" ; then
	  $1_LIB_DIR="$dir"
	fi
      fi
    else 
      for dir in $4 $4/lib ; do 
        lib_spec="-L${dir} -l$2 $5"
        LDFLAGS="${lib_spec} ${save_LDFLAGS}"
        AC_CHECK_LIB([$2], [$3],[found="yes"],[found="no"])
        if test "${found}" = "yes" ; then
	  $1_LIB_DIR="$dir"
	  break
        fi
      done
    fi
    if test "${found}" = "yes" ; then
      AC_DEFINE([HAVE_LIB$1], 1, 
        [Define to 1 if you have the `$1' library (-l$2).])
      aix_lib_specs="${aix_lib_specs} ${lib_spec}"
      $1_LIB_SPEC=${lib_spec}
      if test "x${dir}" != "x" ; then
        loader_run_path="${loader_run_path}:${dir}"
      fi
    fi
    LDFLAGS=${save_LDFLAGS}
  fi
])


dnl BLT_CHECK_HEADER(NAME, SPEC, WITH, DEF)
AC_DEFUN(BLT_CHECK_HEADER,
[  
  if test "$3" != "no" ; then 
    new_CPPFLAGS=""
    if test "$3" != "yes" ; then
      for dir in $3 $3/include ; do
        if test -r "${dir}/$2" ; then
	  new_CPPFLAGS="-I${dir}"
	  $1_INC_DIR="$dir"
	  break
        fi
      done
    else 
      for dir in $prefix $prefix/include ; do
        if test -r "${dir}/$2" ; then
	  new_CPPFLAGS="-I${dir}"
	  $1_INC_DIR="$dir"
	  break
        fi
      done
    fi     
    save_CPPFLAGS=${CPPFLAGS}
    CPPFLAGS="$4 ${new_CPPFLAGS}"
    AC_CHECK_HEADERS($2, [$1_INC_SPEC="${new_CPPFLAGS}"], [$1_INC_SPEC=""])
    CPPFLAGS=${save_CPPFLAGS}
  fi
])
