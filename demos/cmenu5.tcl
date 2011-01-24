
set imgData {
    R0lGODlhEAANAMIAAAAAAH9/f///////AL+/vwAA/wAAAAAAACH5BAEAAAUALAAAAAAQAA0A
    AAM8WBrM+rAEQWmIb5KxiWjNInCkV32AJHRlGQBgDA7vdN4vUa8tC78qlrCWmvRKsJTquHkp
    ZTKAsiCtWq0JADs=
}

set icon2 [image create picture -file images/blt98.gif]
set icon [image create picture -data $imgData]
set bg [blt::bgpattern create gradient -low  grey100 -high grey90 \
	-dir x -jitter yes -log yes -relativeto self]

set image ""
option add *ComboEntry.takeFocus 1

if { [file exists ../library] } {
    set blt_library ../library
}

#    -postcommand {.e.m configure -width [winfo width .e] ; update} \
set myIcon ""
blt::comboentry .e \
    -image $image \
    -textvariable myText1 \
    -iconvariable myIcon1 \
    -textwidth 6 \
    -menu .e.m \
    -menuanchor se \
    -exportselection yes \
    -xscrollcommand { .s set }  \
    -command "puts {button pressed}"

#    -bg $bg 

blt::combomenu .e.m  \
    -textvariable myText1 \
    -iconvariable myIcon1 \
    -font { Arial 8 } -acceleratorfont { Arial 8 } \
    -yscrollbar .e.m.ybar \
    -xscrollbar .e.m.xbar

blt::tk::scrollbar .e.m.xbar
blt::tk::scrollbar .e.m.ybar

set onOff 1
set wwho ""
foreach {item type} { 
    Undo	command 
    X1		checkbutton 
    Y1		radiobutton
    Redo	checkbutton
    Cut		command
    Copy	checkbutton
    X2		checkbutton
    Y2		radiobutton
    Paste	checkbutton
    separator	separator
    "Select" checkbutton
    X3		checkbutton
    Y3		radiobutton
    Find	cascade
    Replace	checkbutton
} {
    set char [string range $item 0 0] 
    .e.m add \
	-type $type \
	-text $item \
	-accel "Ctrl+$char" \
	-underline 0 \
	-tag "$type [string tolower $char]" \
	-icon $icon \
	-variable $item \
	-value $item \

}
.e.m item configure radiobutton -variable "wwho"
set X1 1
set Redo 1
set wwho2 ""
blt::tk::scrollbar .s -orient vertical -command { .e xview } 

bind ComboEntry <3> {
    grab release [grab current]
}

blt::table . \
    0,0 .e -fill x -anchor n

blt::table configure . r0 -resize none
blt::table configure . r1 -resize expand
