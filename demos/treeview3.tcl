#!../src/bltwish

package require BLT
#source scripts/demo.tcl

set saved [pwd]

#blt::bltdebug 100

blt::treeview .tv  \
    -yscrollcommand { .vs set } \
    -xscrollcommand { .hs set } 
scrollbar .vs -orient vertical -command { .tv yview }
scrollbar .hs -orient horizontal -command { .tv xview }
blt::table . \
    0,0 .tv  -fill both \
    0,1 .vs -fill y \
    1,0 .hs -fill x

blt::table configure . c1 r1 -resize none

proc DoFind { entry } {
    global fileList
    lappend fileList $entry
    #puts "$entry"
    if { [file type $entry] == "directory" } { 
	foreach f [lsort [glob -nocomplain $entry/*]] {
	    DoFind $f
	} 
    }
}

proc Find { dir } {
    global fileList
    set fileList {}
    DoFind $dir 
    return $fileList
}
proc GetAbsolutePath { dir } {
    set saved [pwd]
    cd $dir
    set path [pwd] 
    cd $saved
    return $path
}

set top [GetAbsolutePath .]
set trim "$top"

.tv configure -separator "/" -autocreate yes  -trim $trim

.tv entry configure root -label "$top"
.tv configure -bg white -alternatebackground grey95
set fileList [Find $top]
eval .tv insert end $fileList

#.tv insert 0 fred
focus .tv

set nodes [.tv find -glob -name *.tcl]
set img [image create picture -file ~/lines.gif]
eval .tv entry configure $nodes -foreground red -icons $img
set img [image create picture -file ~/cartoon.png]
.tv entry configure 1 -icon $img
cd $saved

