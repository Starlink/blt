namespace eval blt {
    namespace eval Scrollset {
	#empty
    }
}

proc blt::Scrollset::Init { scrollset } {
    set xscrollbar [$scrollset cget -xscrollbar]
    set yscrollbar [$scrollset cget -yscrollbar]
    set slave [$scrollset cget -window]
    if { $slave != "" } {
	set yscrollcmd [$scrollset cget -yscrollcommand]
	if { $yscrollcmd == "" } {
	    set yscrollcmd [list $slave yview]
	}
	if { [catch $yscrollcmd] == 0 } {
	    $slave configure -yscrollcommand [list $scrollset set y]
	}
	set xscrollcmd [$scrollset cget -xscrollcommand]
	if { $xscrollcmd == "" } {
	    set xscrollcmd [list $slave xview]
	}
	if { [catch $xscrollcmd] == 0 } {
	    $slave configure -xscrollcommand [list $scrollset set x]
	}
    }
    if { $xscrollbar != "" } {
	$xscrollbar configure -command [list $scrollset xview] 
    }
    if { $yscrollbar != "" } {
	$yscrollbar configure -command [list $scrollset yview] \
	    -orient vertical 
    }
}