
proc find { tree parent dir } {
    global count 
    set saved [pwd]

    cd $dir
    foreach f [glob -nocomplain *] {
	set name [file tail $f]
	if { [file type $f] == "directory" } {
	    set node [$tree insert $parent -label $name]
	    find $tree $node $f
	}
    }
    cd $saved
}

set tree [blt::tree create]
set path ../..
find $tree root $path
$tree label root [file normalize $path]
if { [file exists ../library] } {
    set blt_library ../library
}
puts [$tree label 0]
#    -postcommand {.e.m configure -width [winfo width .e] ; update} \
set myIcon ""
blt::combobutton .b \
    -width 200  \
    -font { arial 10 bold } \
    -indicatoron no \
    -textvariable myText1 \
    -iconvariable myIcon1 \
    -menu .b.m \
    -menuanchor se \
    -command "puts {button pressed}"

blt::combotree .b.m \
    -tree $tree \
    -borderwidth 1 \
    -font { arial 10 } \
    -textvariable myText1 \
    -iconvariable myIcon1 \
    -separator / \
    -width 400 \
    -height -200 \
    -xscrollcommand { .b.m.xbar set } \
    -yscrollcommand { .b.m.ybar set } \
    -yscrollbar .b.m.ybar \
    -xscrollbar .b.m.xbar

blt::tk::scrollbar .b.m.xbar -orient horizontal -command { .b.m xview } \
    -width 17
blt::tk::scrollbar .b.m.ybar -orient vertical -command { .b.m yview } \
    -width 17 

focus .b

blt::table . \
    .b -fill x 