set imgData {
    R0lGODlhEAANAMIAAAAAAH9/f///////AL+/vwAA/wAAAAAAACH5BAEAAAUALAAAAAAQAA0A
    AAM8WBrM+rAEQWmIb5KxiWjNInCkV32AJHRlGQBgDA7vdN4vUa8tC78qlrCWmvRKsJTquHkp
    ZTKAsiCtWq0JADs=
}

set icon [image create picture -data $imgData]
set icon2 [image create picture -file images/blt98.gif]
set bg [blt::bgpattern create gradient -high  grey98 -low grey85 \
	-dir x -jitter yes -log yes -relativeto self]

blt::combomenu .m  \
    -bg $bg \
    -textvariable myText \
    -iconvariable myIcon \
    -font { Arial 9 bold } \
    -activebackground lightblue1 \
    -acceleratorfont { Arial 8 } \
    -borderwidth 2 \
    -disabledforeground grey45  \
    -disabledbackground grey85  \
    -disabledacceleratorforeground grey45  \
    -height 200 \
    -relief raised 

#pack .m -fill both -expand yes

set onOff 0
set wwho ""
foreach item { Undo X1 Y1 Redo Cut Copy X2 Y2 Paste "Select All" X3 Y3 Find Replace } {
    set char [string range $item 0 0] 
    .m add \
	-text $item \
	-type checkbutton \
	-accel "Ctrl+$char" \
	-underline 0 \
	-tag [string tolower $char] \
	-icon $icon \
	-variable onOff \
	-onvalue 1 -offvalue 0 -value $item
}

.m item configure Undo -type command
.m item configure Cut -type command
.m item configure Find -type cascade -state disabled
.m item configure Redo -type command
.m item configure Undo -type command 
.m item configure Paste -type separator -state disabled
.m item configure x -state disabled 
.m item configure y -type radiobutton -variable wwho 

bind ComboMenu <Enter> { %W activate @%x,%y }
bind ComboMenu <Leave> { %W activate "" }

bind ComboMenu <Motion> { %W activate @%x,%y }
bind ComboMenu <ButtonRelease-1> { 
    %W invoke active 
    puts text=$myText
    puts icon=$myIcon
}

bind ComboMenu <ButtonPress-1> { puts stderr [%W index next] }

