
package require BLT
source scripts/demo.tcl

blt::paneset .vps  -bg green -orient vertical -activesashcolor grey50 
blt::paneset .vps.hps  -bg green -orient horizontal

blt::graph .vps.g 
blt::barchart .vps.hps.b 
blt::barchart .vps.hps.b2

.vps add -window .vps.g -fill both  
.vps add -window .vps.hps -fill both -resize expand
.vps.hps add -window .vps.hps.b -fill both 
.vps.hps add -window .vps.hps.b2 -fill both

proc SetupBindings { w } {
    $w bind Sash <Enter> { %W sash activate current } 
    $w bind Sash <Leave> { %W sash activate none } 
    $w bind Sash <ButtonPress-1> { %W sash anchor %x %y }
    $w bind Sash <B1-Motion> { %W sash mark %x %y }
    $w bind Sash <ButtonRelease-1> { %W sash set %x %y }
}

SetupBindings .vps
SetupBindings .vps.hps

pack .vps -fill both -expand yes
focus .vps
after 5000 {
    #.ps pane configure 1 -size { 0 10000 10 }
    focus .
}