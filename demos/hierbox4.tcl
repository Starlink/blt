#!../src/bltwish

package require BLT
source scripts/demo.tcl

proc AddDirEntries { w dir } {
    if { [file isdirectory $dir] } {
	set files [glob -nocomplain $dir/*] 
	eval $w insert end [lsort $files]
	set subdirs [glob -nocomplain $dir/*/]
	eval $w entry configure [lsort $subdirs] -gadget yes
    }
}

#blt::bltdebug 100

image create picture openFolder -file images/open.gif
image create picture closeFolder -file images/close.gif

option add *Hierbox.icons "closeFolder openFolder"

option add *Hierbox.cursor crosshair
option add *Hierbox.Button.Relief solid
option add *Hierbox.Button.ActiveBackground white
option add *Hierbox.Button.Background white

hierbox .h  \
    -yscrollcommand { .vs set } \
    -xscrollcommand { .hs set } \
    -activebackground lightskyblue1 \
    -selectbackground lightskyblue2

scrollbar .vs -orient vertical -command { .h yview }
scrollbar .hs -orient horizontal -command { .h xview }
button .test -text Test -command {
    set index [.h curselection]
    set names [eval .h get -full $index]
    puts "selected names are $names"
}

blt::table . \
    0,0 .h  -fill both \
    0,1 .vs -fill y \
    1,0 .hs -fill x \
    2,0 .test 

blt::table configure . c1 r1 -resize none

.h configure -autocreate yes  -font { Helvetica 34 }
focus .h
.h insert end { The Quick Brown Fox Jumped Over the }
.h entry configure root -label {[Root]}
.h insert end { The\nQuick\nBrown\nFox\nJumped\nOver\nthe }

.h configure -focusedit yes
