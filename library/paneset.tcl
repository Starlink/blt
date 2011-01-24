namespace eval blt::Paneset {
    set buttonPressed 0
    proc Initialize {} {
    }
}

bind PanesetSash <Enter> { 
    if { !$blt::Paneset::buttonPressed } { 
	%W activate 
    } 
} 
bind PanesetSash <Leave> { 
    if { !$blt::Paneset::buttonPressed } { 
	%W deactivate
    } 
}
bind PanesetSash <KeyPress-Left> { 
    %W move -10 0 
}
bind PanesetSash <KeyPress-Right> { 
    %W move 10  0 
}
bind PanesetSash <KeyPress-Up> { 
    %W move 0 -10 
}
bind PanesetSash <KeyPress-Down> { 
    %W move 0 10 
}
bind PanesetSash <ButtonPress-1> { 
    set blt::Paneset::buttonPressed 1
    %W anchor %X %Y 
}
bind PanesetSash <B1-Motion> { 
    %W mark %X %Y 
}
bind PanesetSash <ButtonRelease-1> { 
    set blt::Paneset::buttonPressed 0
    %W set %X %Y 
}
