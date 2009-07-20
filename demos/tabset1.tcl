#!../src/bltwish

package require BLT
source scripts/demo.tcl

image create picture bgTile -file ./images/chalk.gif
image create picture label1 -file ./images/mini-book1.gif
image create picture label2 -file ./images/mini-book2.gif

blt::tabset .t \
    -outerrelief raised \
    -outerborderwidth 0 \
    -highlightthickness 0 \
    -scrollcommand { .s set } \
    -width 3i 

.t insert end First \
    -image label1 \
    -anchor center \
    -selectbackground darkolivegreen2  

foreach page { Again Next another test of a widget } {
    .t insert end $page \
	-anchor center \
	-selectbackground darkolivegreen2 \
	-image label2 
}

blt::tk::scrollbar .s -command { .t view } -orient horizontal
blt::table . \
    .t 0,0 -fill both \
    .s 1,0 -fill x 

blt::table configure . r1 -resize none
focus .t

