#!../src/bltwish

package require BLT
source scripts/demo.tcl
source scripts/stipples.tcl

option add *graph.x.Title "X Axis Label"
option add *graph.y.Title "Y Axis Label"
option add *graph.title "A Simple Barchart"
option add *graph.Axis.tickFont { Courier 10 } 
option add *graph.Element.Relief raised

set visual [winfo screenvisual .] 
if { $visual != "staticgray" && $visual != "grayscale" } {
    option add *graph.LineMarker.color yellow
    option add *graph.Element.Background white
    option add *graph.Legend.activeForeground pink
    option add *print.background yellow
    option add *quit.background red
    option add *graph.background palegreen
    option add *graph.plotBackground lightblue
}

blt::htext .htext -text \
{   This is an example of the barchart widget.  The barchart has 
    many components; x and y axis, legend, crosshairs, elements, etc.  
    To create a postscript file "bar.ps", press the %%
    set w $htext(widget)
    button $w.print -text {Print} -command {
	$graph postscript output bar.ps
    } 
    $w append $w.print

%% button.  
%%

    set graph [blt::barchart .htext.graph]
    $graph configure \
	-relief raised \
	-bd 2
    $graph xaxis configure \
	-stepsize 0 
    $w append $graph -fill both -padx 4

%%
    Hit the %%

    button $w.quit -text quit -command exit
    $w append $w.quit 

%% button when you've seen enough.%%

    label $w.logo -bitmap BLT
    $w append $w.logo -padx 20

%% }

set names { One Two Three Four Five Six Seven Eight }
if { $visual == "staticgray" || $visual == "grayscale" } {
    set fgcolors { white white white white white white white white }
    set bgcolors { black black black black black black black black }
} else {
    set fgcolors { yellow orange red magenta purple blue cyan green }
    set bgcolors { yellow4 orange4 red4 magenta4 purple4 blue4 cyan4 green4 }
}

set numColors [llength $names]

set tcl_precision 15
blt::vector create x
blt::vector create y
x seq -5.0 5.0 50
y expr sin(x)
set barWidth 0.19

$graph element create sin -relief raised -bd 1 -x x -y y  -barwidth $barWidth
blt::table . .htext -fill both
	
wm min . 0 0

Blt_ZoomStack $graph
Blt_Crosshairs $graph
Blt_ActiveLegend $graph
Blt_ClosestPoint $graph
