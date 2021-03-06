#!../src/bltwish

package require BLT
#source scripts/demo.tcl

set visual [winfo screenvisual .] 
if { $visual == "staticgray"  || $visual == "grayscale" } {
    set activeBg black
    set normalBg white
    set bitmapFg black
    set bitmapBg white
    option add *top.background 		white
} else {
    option add *htext.foreground navyblue
    if { $tk_version >= 4.0 } {
	set file1 ./images/clouds.gif
	set file2 ./images/chalk.gif
	image create picture texture1 -file $file1
	image create picture texture2 -file $file2
	set bg1 [blt::bgpattern create tile -image texture1]
	set bg2 [blt::bgpattern create tile -image texture2]
	option add *htext.foreground black
	option add *htext.background $bg1
	option add *htext.selectBackground gold1
    }
}
option add *highlightThickness 0

proc Blt_FindPattern { htext } {
    toplevel .search
    wm title .search "Text search"
    label .search.label1 -text "Enter Pattern"
    entry .search.entry -relief sunken
    label .search.clear -text "Clear" \
	-command ".search.entry delete 0 end"
    label .search.cancel -text "Cancel" \
	-command "destroy .search; focus $htext"
    label .search.search -text "Search" -command "Blt_Search&Move $htext"
    bind .search.entry <Return> "Blt_Search&Move $htext"
    blt::table .search \
	.search.label1 	0,0 -padx 4 \
	.search.entry 	0,1 -cspan 2 -pady 4 -padx 4 -reqwidth 3i \
	.search.search  3,0 -reqwidth .75i -anchor w -padx 10 -pady 5  \
	.search.clear	3,1 -reqwidth .75i -anchor center -padx 10 -pady 5 \
	.search.cancel	3,2 -reqwidth .75i -anchor e -padx 10 -pady 5 
    focus .search.entry
    bind .search <Visibility> { raise .search }
}
       
set last 0
set lastPattern {}

proc Blt_Search&Move { h } {
    global last
    global lastPattern


    set pattern [.search.entry get]
    if { [string compare $pattern $lastPattern] != 0 } {
	set last 0
	set lastPattern $pattern
    }
    if { $pattern == "" } {
	return
    }
	
    set indices [$h search $pattern $last end]
    if { $indices == "" } {
	bell
    } else {
	set first [lindex $indices 0]
	set last [lindex $indices 1]
	$h selection range $first $last
	$h gotoline $first
	incr last
    }
}

# Create horizonatal and vertical scrollbars
scrollbar .vscroll -command { .htext yview } -orient vertical 
scrollbar .hscroll -command { .htext xview } -orient horizontal

# Create the hypertext widget 
blt::htext .htext -file ./htext.txt  \
    -yscrollcommand { .vscroll set } \
    -xscrollcommand { .hscroll set } \
    -yscrollunits 10m -xscrollunits .25i \
    -height 6i 


blt::table . \
    .htext 0,0 -fill both \
    .vscroll 0,1 -fill y \
    .hscroll 1,0 -fill x 

blt::table configure . r1 c1 -resize none

bind .htext <B1-Motion> {
    %W select to @%x,%y
}
bind .htext <1> {
    %W select from @%x,%y
    %W select to @%x,%y
}

bind .htext <Shift-1> {
    %W select word @%x,%y
}
bind .htext <Meta-1> {
    %W select line @%x,%y
}
bind .htext <Control-1> {
    puts stderr [%W select index @%x,%y]
}

bind .htext <B2-Motion> {
    %W scan dragto @%x,%y
}
bind .htext <2> {
    %W scan mark @%x,%y
}

bind .htext <3> {
    %W select adjust @%x,%y
}

bind .htext <Control-p> { 
    set line [%W gotoline]
    if { $line == 0 } {
	bell
    } else {
	set line [expr $line-1]
	%W gotoline $line.0
    }
}
bind .htext <Control-n> { 
    set line [%W gotoline]
    incr line
    if { [%W gotoline $line.0] != $line } {
	bell
    }
}

bind .htext <Control-v> { 
    %W yview [expr [%W yview]+10]
}

bind .htext <Meta-v> { 
    %W yview [expr [%W yview]-10]
}

bind .htext <Alt-v> { 
    %W yview [expr [%W yview]-10]
}

bind .htext <Any-q> {
    exit 0
}
bind .htext <Control-s> {
    Blt_FindPattern %W
}

wm min . 0 0
focus .htext
