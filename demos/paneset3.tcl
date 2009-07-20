
package require BLT
source scripts/demo.tcl

set pictures [glob -nocomplain "*.jpg"]
set autocolors {
#0000cd
#cd0000
#00cd00
#3a5fcd
#cdcd00
#cd1076
#009acd
#00c5cd
#a2b5cd
#7ac5cd
#66cdaa
#a2cd5a
#cd9b9b
#cdba96
#cd3333
#cd6600
#cd8c95
#cd00cd
#9a32cd
#6ca6cd
#9ac0cd
#9bcd9b
#00cd66
#cdc673
#cdad00
#cd5555
#cd853f
#cd7054
#cd5b45
#cd6889
#cd69c9
#551a8b
}

proc Move { w pane } {
#    puts stderr "w=$w pane=$pane"
    .ps see $pane
}

blt::paneset .ps  -bg grey -width 600 -scrollcommand { .s set } 
    #-scrolldelay 10 -scrollincrement 10

for { set i 0 } { $i < 32 } { incr i } {
    set color [lindex $autocolors $i]
    blt::graph .ps.g$i -bg $color -width 500
    set pane [.ps add -window .ps.g$i -fill both -sashcolor $color]
    bind .ps.g$i <ButtonPress-1>  [list Move %W $pane]
    bind .ps.g$i <ButtonPress-2>  [list Move %W pane0]
    bind .ps.g$i <ButtonPress-3>  [list Move %W pane35]
}
blt::tk::scrollbar .s -command { .ps view } -orient horizontal

.ps bind Sash <Enter> { .ps sash activate current } 
.ps bind Sash <Leave> { .ps sash activate none } 

.ps bind Sash <ButtonPress-1> {
    %W sash anchor %x %y
}
.ps bind Sash <B1-Motion> {
    %W sash mark %x %y
}
.ps bind Sash <ButtonRelease-1> {
    %W sash set %x %y
}

blt::table . \
    0,0 .ps -fill both \
    1,0 .s -fill x 

blt::table configure . r1 -resize none

focus .ps
after 5000 {
    #.ps pane configure 1 -size { 0 10000 10 }
    focus .
    #.ps see pane2
    #.ps size pane2 1i
}

