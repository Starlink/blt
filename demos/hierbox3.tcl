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

image create picture openFolder -file images/open.gif
image create picture closeFolder -file images/close.gif
option add *Hierbox.icons "closeFolder openFolder"

#option add *Hierbox.openCommand { AddDirEntries %W "$top/%P" }
#option add *Hierbox.closeCommand { eval %W delete %n 0 end }

hierbox .h  \
    -allowduplicates no \
    -hideroot yes \
    -yscrollcommand { .vs set } \
    -xscrollcommand { .hs set }

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

blt::table configure . c1 r1 r2 -resize none

set top ../
.h configure -separator "/" -trim $top -autocreate yes  
#.h entry configure 0 -label [file tail $top] 

catch { exec du $top } files
foreach f [split $files \n ] {
    .h insert end [lindex $f 1] -text [lindex $f 0] -button auto
}

focus .h
