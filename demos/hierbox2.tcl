#!../src/bltwish

package require BLT
source scripts/demo.tcl

proc AddDirEntries { w dir } {
    if { [file isdirectory $dir] } {
	set files [glob -nocomplain $dir/*] 
	eval $w insert end [lsort $files]
	set subdirs [glob -nocomplain $dir/*/]
	eval $w entry configure [lsort $subdirs] -button yes
    }
}

set imageList {}
foreach f [glob ./images/mini-*.gif] {
    lappend imageList [image create picture -file $f]
}

set top ../

#option add *Hierbox.Tile	bgTexture
option add *Hierbox.TileOffset  yes

option add *forceGadgets	no
option add *Hierbox.openCommand	{ 
    AddDirEntries %W "$top/%P"
}
option add *Hierbox.closeCommand {
    eval %W delete %n 0 end
}

image create picture openFolder -file images/open.gif
image create picture closeFolder -file images/close.gif

option add *Hierbox.icons "closeFolder openFolder"

#option add *Hierbox.Button.activeForeground red
#option add *Hierbox.bindTags "Label all"

hierbox .h  \
    -selectmode multiple \
    -hideroot yes \
    -yscrollcommand { .vs set } \
    -xscrollcommand { .hs set }

.h button configure -activebackground grey92
scrollbar .vs -orient vertical -command { .h yview }
scrollbar .hs -orient horizontal -command { .h xview }
button .test -text Test -command {
    set index [.h curselection]
    set names [eval .h get -full $index]
    puts "selected names are $names"
}

button .quit -text Quit -command { exit 0 }

blt::table . \
    0,0 .h  -fill both \
    2,0 .quit \
    0,1 .vs -fill y 1,0 .hs -fill x \
    3,0 .test

blt::table configure . c1 r1 r2 r3 -resize none

.h configure -separator "/" -trim $top \
    -allowduplicates no 

#.h entry configure 0 -label [file tail $top] 

AddDirEntries .h $top
focus .h
set nodes [.h find -glob -name *.c]
eval .h entry configure $nodes -labelcolor red 

wm protocol . WM_DELETE_WINDOW { destroy . }
#blt::bltdebug 100