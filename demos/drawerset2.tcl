
package require BLT
source scripts/demo.tcl

blt::graph .g -bg \#CCCCFF -height 800
set w [blt::drawerset .g -side top -sashthickness 20]
blt::barchart .g.b -bg \#FFCCCC -height 300
blt::barchart .g.b2 -bg \#CCFFCC -height 300
$w add -window .g.b -fill x -resize both
$w add -window .g.b2 -fill y
$w open pane0
set pressed 0
bind PanesetSash <Enter> { if { !$pressed } { %W activate } } 
bind PanesetSash <Leave> { if { !$pressed } { %W deactivate } } 
bind PanesetSash <KeyPress-Left> { %W move -10 0 }
bind PanesetSash <KeyPress-Right> { %W move 10  0 }
bind PanesetSash <KeyPress-Up> { %W move 0 -10 }
bind PanesetSash <KeyPress-Down> { %W move 0 10 }
bind PanesetSash <Shift-KeyPress-Left> { %W move -100 0 }
bind PanesetSash <Shift-KeyPress-Right> { %W move 100  0 }
bind PanesetSash <Shift-KeyPress-Up> { %W move 0 -100 }
bind PanesetSash <Shift-KeyPress-Down> { %W move 0 100 }
bind PanesetSash <ButtonPress-1> { 
    set pressed 1 
    %W anchor %X %Y 
}
bind PanesetSash <B1-Motion> { %W mark %X %Y }
bind PanesetSash <ButtonRelease-1> { 
    set pressed 0
    %W set %X %Y 
}

blt::table . \
    0,0 .g -fill both 
