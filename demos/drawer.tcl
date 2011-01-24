namespace eval blt::Drawer {
    set buttonPressed 0
    proc Initialize {} {
    }
}

bind DrawerHandle <Enter> { 
    if { !$blt::Drawer::buttonPressed } { 
	%W activate 
    } 
} 
bind DrawerHandle <Leave> { 
    if { !$blt::Drawer::buttonPressed } { 
	%W deactivate
    } 
}
bind DrawerHandle <KeyPress-Left> { 
    %W move -10 0 
}
bind DrawerHandle <KeyPress-Right> { 
    %W move 10  0 
}
bind DrawerHandle <KeyPress-Up> { 
    %W move 0 -10 
}
bind DrawerHandle <KeyPress-Down> { 
    %W move 0 10 
}
bind DrawerHandle <ButtonPress-1> { 
    set blt::Paneset::buttonPressed 1
    %W anchor %X %Y 
}
bind DrawerHandle <B1-Motion> { 
    %W mark %X %Y 
}
bind DrawerHandle <ButtonRelease-1> { 
    set blt::Paneset::buttonPressed 0
    %W set %X %Y 
}

