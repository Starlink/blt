namespace eval blt::Filmstrip {
    set buttonPressed 0
    proc Initialize {} {
    }
}

bind FilmstripHandle <Enter> { 
    if { !$blt::Filmstrip::buttonPressed } { 
	%W activate 
    } 
} 
bind FilmstripHandle <Leave> { 
    if { !$blt::Filmstrip::buttonPressed } { 
	%W deactivate
    } 
}
bind FilmstripHandle <KeyPress-Left> { 
    %W move -10 0 
}
bind FilmstripHandle <KeyPress-Right> { 
    %W move 10  0 
}
bind FilmstripHandle <KeyPress-Up> { 
    %W move 0 -10 
}
bind FilmstripHandle <KeyPress-Down> { 
    %W move 0 10 
}
bind FilmstripHandle <Shift-KeyPress-Left> { 
    %W move -100 0 
}
bind FilmstripHandle <Shift-KeyPress-Right> { 
    %W move 100  0 
}
bind FilmstripHandle <Shift-KeyPress-Up> { 
    %W move 0 -100 
}
bind FilmstripHandle <Shift-KeyPress-Down> { 
    %W move 0 100
}
bind FilmstripHandle <ButtonPress-1> { 
    set blt::Filmstrip::buttonPressed 1
    %W anchor %X %Y 
}
bind FilmstripHandle <B1-Motion> { 
    %W mark %X %Y 
}
bind FilmstripHandle <ButtonRelease-1> { 
    set blt::Filmstrip::buttonPressed 0
    %W set %X %Y 
}

