#!../src/bltwish

if { [info exists env(BLT_LIBRARY)] } {
   lappend auto_path $env(BLT_LIBRARY)
}
package require BLT

source scripts/demo.tcl

set normalBg [blt::bgpattern create texture -low grey85 -high grey88]
set activeBg [blt::bgpattern create gradient -low grey40 -high grey95 \
	-jitter yes -log no -opacity 80]
set normalBg white
set activeBg grey95
# option add *Axis.activeBackground $activeBg
# option add *Legend.activeBackground $activeBg

set graph .g
blt::graph .g \
    -bg $normalBg \
    -plotrelief solid \
    -plotborderwidth 0 \
    -relief solid \
    -plotpadx 0 -plotpady 0 \
    -borderwidth 0

blt::htext .header \
    -text {\
This is an example of the graph widget.  It displays two-variable data 
with assorted line attributes and symbols.  To create a postscript file 
"xy.ps", press the %%
    blt::tk::button $htext(widget).print -text print -command {
        puts stderr [time {
	    blt::busy hold .
	    update
	    .g postscript output demo1.eps 
	    update
	    blt::busy release .
	    update
        }]
    } 
    $htext(widget) append $htext(widget).print
%% button.}

source scripts/graph1.tcl

blt::htext .footer \
    -text {Hit the %%
blt::tk::button $htext(widget).quit -text quit -command { exit } 
$htext(widget) append $htext(widget).quit 
%% button when you've seen enough.%%
label $htext(widget).logo -bitmap BLT
$htext(widget) append $htext(widget).logo 
%%}

proc MultiplexView { args } { 
    eval .g axis view y $args
    eval .g axis view y2 $args
}

blt::tk::scrollbar .xbar \
    -command { .g axis view x } \
    -orient horizontal \
    -highlightthickness 0
blt::tk::scrollbar .ybar \
    -command MultiplexView \
    -orient vertical -highlightthickness 0
blt::table . \
    0,0 .header -cspan 3 -fill x \
    1,0 .g  -fill both -cspan 3 -rspan 3 \
    2,3 .ybar -fill y  -padx 0 -pady 0 \
    4,1 .xbar -fill x \
    5,0 .footer -cspan 3 -fill x

blt::table configure . c3 r0 r4 r5 -resize none

.g postscript configure \
    -center yes \
    -maxpect yes \
    -landscape yes \

.g axis configure x \
    -scrollcommand { .xbar set }  \
    -scrollmax 10 \
    -scrollmin 2  \
    -activeforeground red3 \
    -activebackground white \
    -title "X ayis" \
    -tickinterior no

.g axis configure y \
    -scrollcommand { .ybar set } \
    -scrollmax 1000 \
    -activeforeground red3 \
    -activebackground white \
    -scrollmin -100  \
    -rotate 90 \
    -title "Y ayis" \
    -tickinterior no

.g axis configure y2 \
    -scrollmin 0.0 -scrollmax 1.0 \
    -hide yes \
    -rotate -90 \
    -title "Y2"

.g legend configure \
    -relief flat -bd 0 \
    -activerelief flat \
    -activeborderwidth 1  \
    -position right -anchor ne -bg ""

.g configure -plotpadx 0 -plotpady 0 -plotborderwidth 1 -plotrelief solid

.g pen configure "activeLine" \
    -showvalues y
.g configure -halo 50
.g element bind all <Enter> {
    eval %W legend deactivate [%W element names]
    %W legend activate [%W element get current]
}
.g configure -plotpady { 0 0 } 

.g element bind all <Leave> {
    %W legend deactivate [%W element get current]
}
.g axis bind all <Enter> {
    set axis [%W axis get current]
    %W axis activate $axis
    %W axis focus $axis
}
.g axis bind all <Leave> {
    set axis [%W axis get current]
    %W axis deactivate $axis
    %W axis focus ""
}
.g configure -leftvariable left 
trace variable left w "UpdateTable .g"
proc UpdateTable { graph p1 p2 how } {
    blt::table configure . c0 -width [$graph extents leftmargin]
    blt::table configure . c2 -width [$graph extents rightmargin]
    blt::table configure . r1 -height [$graph extents topmargin]
    blt::table configure . r3 -height [$graph extents bottommargin]
}

set image1 [image create picture -file bitmaps/sharky.xbm]
set image2 [image create picture -file images/buckskin.gif]
set bg1 [blt::bgpattern create solid -color blue -opacity 30]
set bg2 [blt::bgpattern create solid -color green -opacity 40]
set bg3 [blt::bgpattern create solid -color pink -opacity 40]
.g element configure line1 -areabackground $bg1 -areaforeground blue 
#.g element configure line2 -areabackground $bg2
#.g element configure line3 -areabackground $bg3
.g configure -title "Graph Title"

if { $tcl_platform(platform) == "windows" } {
    if 0 {
        set name [lindex [blt::printer names] 0]
        set printer {Lexmark Optra E310}
	blt::printer open $printer
	blt::printer getattrs $printer attrs
	puts $attrs(Orientation)
	set attrs(Orientation) Landscape
	set attrs(DocumentName) "This is my print job"
	blt::printer setattrs $printer attrs
	blt::printer getattrs $printer attrs
	puts $attrs(Orientation)
	after 5000 {
	    $graph print2 $printer
	    blt::printer close $printer
	}
    } else {
	after 5000 {
	    $graph print2 
	}
    }	
    if 1 {
	after 2000 {
	    $graph snap -format emf CLIPBOARD
	}
    }
}

focus .g
.g xaxis bind <Left>  { 
    .g xaxis view scroll -1 units 
} 

.g xaxis bind <Right> { 
    .g xaxis view scroll 1 units 
}

.g yaxis bind <Up>  { 
    .g yaxis view scroll -1 units 
} 

.g yaxis bind <Down> { 
    .g yaxis view scroll 1 units 
}

.g axis bind all <ButtonPress-1> { 
    set b1(x) %x
    set b1(y) %y
}

.g xaxis bind <B1-Motion> { 
    set dist [expr %x - $b1(x)]
    .g xaxis view scroll $dist pixels
    set b1(x) %x
}

.g yaxis bind <B1-Motion> { 
    set dist [expr %y - $b1(y)]
    .g yaxis view scroll $dist pixels
    set b1(y) %y
}

blt::LegendSelections .g
.g legend configure -selectmode multiple