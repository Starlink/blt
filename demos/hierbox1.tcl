#!../src/bltwish

package require BLT
#source scripts/demo.tcl

set saved [pwd]

#blt::bltdebug 100

set imagedir images
image create picture bgTexture -file $imagedir/rain.gif

set imageList {}
foreach f [glob $imagedir/mini-*.gif] {
    lappend imageList [image create picture -file $f]
}

#option add *TreeView.Tile	bgTexture
option add *TreeView.ScrollTile  yes

option add *xTreeview.openCommand	{
    set path /home/gah/src/blt/%P
    if { [file isdirectory $path] } {
	cd $path
	set files [glob -nocomplain * */. ]
	if { $files != "" } {
	    eval %W insert -at %n end $files
	}
    }
}

option add *xTreeView.closeCommand {
    eval %W delete %n 0 end
}

blt::treeview .h  \
    -yscrollcommand { .vs set } \
    -xscrollcommand { .hs set } 

scrollbar .vs -orient vertical -command { .h yview }
scrollbar .hs -orient horizontal -command { .h xview }
blt::table . \
    0,0 .h  -fill both \
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

.h configure -separator "/" -autocreate yes  -trim $trim

.h entry configure root -label "$top"
.h configure -bg grey90
update

set fileList [Find $top]
eval .h insert end $fileList
.h configure -bg white -alternatebackground grey95

# %n => %#
# no -activebackground
# no -image 

focus .h

# -labelcolor == -foreground
set nodes [.h find -glob -name *.c]
eval .h entry configure $nodes -foreground red 

cd $saved

