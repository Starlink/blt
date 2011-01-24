
namespace eval ::blt::ComboMenu {
    variable _private
    array set _private {
        activeComboMenu ""
        activeItem      ""
        afterId         -1
        b1              ""
        lastFocus	""
        menuBar         ""
        oldGrab         ""
        popup           ""
        postingMenu     ""
        trace           0
    }
    proc trace { mesg } {
	variable _private 
	if { $_private(trace) } {
	    puts stderr $mesg
	}
    }
}

# -----------------------------------------------------------------------------

# Must set focus when mouse enters a menu, in order to allow
# mixed-mode processing using both the mouse and the keyboard.
# Don't set the focus if the event comes from a grab release,
# though:  such an event can happen after as part of unposting
# a cascaded chain of menus, after the focus has already been
# restored to wherever it was before menu selection started.

bind ComboMenu <FocusIn> {}

bind ComboMenu <Enter> { 
    blt::ComboMenu::trace "blt::ComboMenu %W <Enter>"
    focus %W
}

bind ComboMenu <Leave> { 
    blt::ComboMenu::trace "blt::ComboMenu %W <Leave> %s"
    if { %s == 0 } {
	#%W activate none 
    }
}

bind ComboMenu <Motion> { 
    #blt::ComboMenu::trace "blt::ComboMenu Motion %x,%y"
    %W activate @%x,%y 
    %W postcascade active
}

bind ComboMenu <ButtonRelease-1> { 
    blt::ComboMenu::trace "blt::ComboMenu %W at %x,%y ButtonRelease-1"
    set index [%W index @%x,%y]
    if { $index == -1 } {
	blt::ComboMenu::trace "ButtonRelease-1: failed to find item in %W"
	blt::ComboMenu::SimulateButtonRelease %W
    } else {
	%W activate $index
	blt::ComboMenu::trace "ButtonRelease-1: activated $index"
	if { [%W type active] == "cascade" } {
	    blt::ComboMenu::trace "ButtonRelease-1: posting cascade for $index"
	    %W postcascade active
	    blt::ComboMenu::PostMenu %W active
	} else {
	    blt::ComboMenu::trace "ButtonRelease-1: %W invoke active ($index)"
	    blt::ComboMenu::SimulateButtonRelease %W
	    update
	    %W invoke active
	}
    }
}

bind ComboMenu <ButtonPress-1> { 
    blt::ComboMenu::trace "blt::ComboMenu ButtonPress-1"
}

bind ComboMenu <ButtonPress-2> { 
    blt::ComboMenu::trace "blt::ComboMenu %W ButtonPress-2"
    set w [grab current]
    $w configure -cursor diamond_cross
    update
    %W scan mark %x %y
}
bind ComboMenu <B2-Motion> { 
    %W scan dragto %x %y
}

bind ComboMenu <ButtonRelease-2> { 
    blt::ComboMenu::trace "blt::ComboMenu %W ButtonRelease-2"
    set w [grab current]
    $w configure -cursor arrow
}

bind ComboMenu <ButtonPress-3> { 
    blt::ComboMenu::trace "blt::ComboMenu ButtonPress-3"
    blt::ComboMenu::UnpostMenu %W
}

bind xComboMenu <ButtonRelease-3> { 
    blt::ComboMenu::trace "blt::ComboMenu ButtonRelease-3"
    if { $blt::ComboMenu::_private(popup) != "" } {
	blt::ComboMenu::popup %W %X %Y
    }
}

bind ComboMenu <KeyPress-space> {
    if { [%W type active] == "cascade" } {
	%W postcascade active
	blt::ComboMenu::PostMenu %W active
    } else {
	blt::ComboMenu::SimulateButtonRelease %W
	%W invoke active
    }
}

bind ComboMenu <KeyRelease> {
    if { [string compare %A {}] == 0 } {
	continue
    }
    set index [%W find "%A" -underline]
    if { $index >= 0 } {
	%W activate $index
	%W see $index
    }
}

# KeyPress-Return -- 
#
#	If the menu item selected is a cascade menu, then post the cascade.
#	Otherwise tell the combobutton or comboentry that we've selected 
#	something by simulating a button release.  This will unpost all the
#	posted menus. Set the root coordinates of the event to be offscreen 
#	so that we don't inadvertantly lie over the arrow of the button.
#
bind ComboMenu <KeyPress-Return> {
    if { [%W type active] == "cascade" } {
	%W postcascade active
	blt::ComboMenu::PostMenu %W active
    } else {
	%W invoke active
	blt::ComboMenu::SimulateButtonRelease %W 
    }
}

bind ComboMenu <Escape> {
    blt::ComboMenu::SimulateButtonRelease %W
}

bind ComboMenu <Left> {
    set menu [winfo parent %W]
    if { [winfo class $menu] == "ComboMenu" } {
	$menu postcascade none
	focus $menu
    }
}

bind ComboMenu <Right> {
    if { [%W type active] == "cascade" } {
	%W postcascade active
	set menu [%W item cget active -menu]
	focus $menu
    }
}

bind ComboMenu <KeyPress-Up> {
    %W activate previous
    %W see active
}

bind ComboMenu <KeyPress-Down> {
    %W activate next
    %W see active
}

bind ComboMenu <KeyPress-Home> {
    %W activate first
    %W see active
}

bind ComboMenu <KeyPress-End> {
    %W activate end
    %W see active
}

bind ComboMenu <KeyPress-Prior> {
    %W yview scroll -1 page
    %W activate view.top
    %W see active
}

bind ComboMenu <KeyPress-Next> {
    %W yview scroll 1 page
    %W activate view.bottom
    %W see active
}

# The following bindings apply to all windows, and are used to implement
# keyboard menu traversal.

if { [tk windowingsystem] == "x11" } {
    bind all <Alt-KeyPress> {
	blt::ComboMenu::Traverse %W %A
    }
    bind all <F10> {
	blt::ComboMenu::FirstItem %W
    }
} else {
    bind ComboMenubutton <Alt-KeyPress> {
	blt::ComboMenu::Traverse %W %A
    }

    bind ComboMenubutton <F10> {
	blt::ComboMenu::First %W
    }
}

if { [tk windowingsystem] == "x11" } {
    bind ComboMenu <4> {
	%W yview scroll -5 units
    }
    bind ComboMenu <5> {
	%W yview scroll 5 units
    }
} else {
    bind ComboMenu <MouseWheel> {
	%W yview scroll [expr {- (%D / 120) * 4}] units
    }
}

# ::blt::ComboMenu::UnpostMenu --
#
# This procedure unposts a given menu, plus all of its ancestors up
# to (and including) a menubutton, if any.  It also restores various
# values to what they were before the menu was posted, and releases
# a grab if there's a menubutton involved.  Special notes:
# 1. It's important to unpost all menus before releasing the grab, so
#    that any Enter-Leave events (e.g. from menu back to main
#    application) have mode NotifyGrab.
# 2. Be sure to enclose various groups of commands in "catch" so that
#    the procedure will complete even if the menubutton or the menu
#    or the grab window has been deleted.
#
# Arguments:
# menu -		Name of a menu to unpost.  Ignored if there
#			is a posted menubutton.

proc ::blt::ComboMenu::UnpostMenu { menu } {
    variable _private

    trace "proc ComboMenu::UnpostMenu $menu"
    set mb $_private(postingMenu)
    set mb ""
    
    # Restore focus right away (otherwise X will take focus away when the menu
    # is unmapped and under some window managers (e.g. olvwm) we'll lose the
    # focus completely).
    
    catch {focus $_private(lastFocus)}
    set _private(lastFocus) ""
    
    # Unpost menu(s) and restore some stuff that's dependent on what was
    # posted.

    catch {
	if { $mb != "" } {
	    set menu [$mb cget -menu]
	    $menu unpost
	    set _private(postingMenu) {}
	    $mb configure -cursor $_private(cursor)
	    if  { [$mb cget -arrowrelief] == "sunken" } {
		$mb configure -arrowrelief $_private(relief)
	    }
	} elseif { $_private(popup) != "" } {
	    $_private(popup) unpost
	    set _private(popup) {}
	} else {
	    # We're in a cascaded sub-menu from a tearoff menu or popup.
	    # Unpost all the menus up to the toplevel one (but not including
	    # the top-level torn-off one) and deactivate the top-level torn
	    # off menu if there is one.
	    while {1} {
		set parent [winfo parent $menu]
		if { [winfo class $parent] != "ComboMenu" || 
		     ![winfo ismapped $parent]} {
		    break
		}
		$parent activate none
		$parent postcascade none
		ComboMenu::GenerateSelect $parent
		set menu $parent
	    }
	    $menu unpost
	}
    } 
    if { $mb != "" && [$mb cget -state] != "disabled" } {
	$mb configure -state normal
    }

    trace menuBar=$_private(menuBar)
    trace menu=$menu
    trace grab=[grab current $menu]

    # Release grab, if any, and restore the previous grab, if there
    # was one.
    if { $menu != "" } {
	set grab [grab current $menu]
	if { $grab != "" } {
	    grab release $grab
	}
    }
    RestoreOldGrab
    if { $_private(menuBar) != "" } {
	$_private(menuBar) configure -cursor $_private(cursor)
	set _private(menuBar) {}
    }
}

proc ::blt::ComboMenu::SimulateButtonRelease { w } {
    set grab [grab current]
    if { $grab != "$w" } {
	event generate $grab <ButtonRelease-1> -rootx -1000 -rooty -1000
    } else {
	$w unpost
	RestoreOldGrab
    }
}

proc ::blt::ComboMenu::PostMenu { w item } {
    variable _private

    trace "proc ComboMenu::PostMenu $w, item=$item"
    if { [$w item cget $item -state] == "disabled" } {
	return
    }

    set menu [$w item cget $item -menu]
    if { $menu == "" } {
	return
    }
    #$menu activate none
    set _private(lastFocus) [focus]
    GenerateSelect $menu


    # If this looks like an option menubutton then post the menu so
    # that the current entry is on top of the mouse.  Otherwise post
    # the menu just below the menubutton, as for a pull-down.

    update idletasks
    if { [catch { $w postcascade $item } msg] != 0 } {
	# Error posting menu (e.g. bogus -postcommand). Unpost it and
	# reflect the error.
	global errorInfo
	set savedInfo $errorInfo
	#
	#MbUnpost $w 
	error $msg $savedInfo
    }
    focus $menu
    $menu activate first
    if { [winfo viewable $menu] } {
	SaveGrab $menu
	trace "setting global grab on $menu"
	grab -global $menu
    }
}

# ::blt::xxxComboMenuUnpost --
# This procedure unposts a given menu, plus all of its ancestors up
# to (and including) a menubutton, if any.  It also restores various
# values to what they were before the menu was posted, and releases
# a grab if there's a menubutton involved.  Special notes:
# 1. It's important to unpost all menus before releasing the grab, so
#    that any Enter-Leave events (e.g. from menu back to main
#    application) have mode NotifyGrab.
# 2. Be sure to enclose various groups of commands in "catch" so that
#    the procedure will complete even if the menubutton or the menu
#    or the grab window has been deleted.
#
# Arguments:
# menu -		Name of a menu to unpost.  Ignored if there
#			is a posted menubutton.

proc ::blt::xxxComboMenuUnpost { w } {
    variable _private

    trace "proc ComboMenuUnpost $w"
    catch { 
	# Restore focus right away (otherwise X will take focus away when the
	# menu is unmapped and under some window managers (e.g. olvwm) we'll
	# lose the focus completely).
	focus $_private(lastFocus) 
    }
    set _private(lastFocus) ""

    # Unpost menu(s) and restore some stuff that's dependent on what was
    # posted.

    $w unpost
    #set _private(postingMenu) {}
    $w configure -cursor $_private(cursor)

    if { [$w cget -state] != "disabled" } {
	#$w configure -state normal
    }
    set menu [$w cget -menu]
    trace MENU=$menu
    trace GRAB=[grab current $menu]
    # Release grab, if any, and restore the previous grab, if there
    # was one.
    if { $menu != "" } {
	set grab [grab current $menu]
	if { $grab != "" } {
	    #grab release $grab
	}
    }
    RestoreOldGrab
}

# ::blt::SaveGrab --
# Sets the variable blt::_private(oldGrab) to record
# the state of any existing grab on the w's display.
#
# Arguments:
# w -			Name of a window;  used to select the display
#			whose grab information is to be recorded.

proc blt::ComboMenu::SaveGrab { w } {
    variable _private

    set grab [grab current $w]
    set _private(oldGrab) ""
    if { $grab != "" } {
	set type [grab status $grab]
	if { $type == "global" } {
	    #set _private(oldGrab) [list grab set -global $grab]
	} else {
	    #set _private(oldGrab) [list grab set $grab]
	}	    
    }
}

# ::blt::RestoreOldGrab --
# Restores the grab to what it was before TkSaveGrabInfo was called.
#

proc ::blt::ComboMenu::RestoreOldGrab {} {
    variable _private

    trace "RestoreOldGrab: current=[grab current] old=$_private(oldGrab)"
    if { $_private(oldGrab) != "" } {
    	# Be careful restoring the old grab, since it's window may not
	# be visible anymore.
	catch $_private(oldGrab)
	set _private(oldGrab) ""
    } else {
	grab release [grab current]
    }	
}

proc ::blt::xxxComboMenuSetFocus {menu} {
    variable _private
    if { $_private(lastFocus) == "" } {
	set _private(lastFocus) [focus]
    }
    focus $menu
}
    
proc ::blt::ComboMenu::GenerateSelect {menu} {
    variable _private

    if { $_private(activeComboMenu) == $menu &&  
	 $_private(activeItem) == [$menu index active] } {
	return
    }
    set _private(activeComboMenu) $menu
    set _private(activeItem) [$menu index active]
    event generate $menu <<MenuSelect>>
}


bind ComboMenu <B1-Enter> {
    after cancel $blt::ComboMenu::_private(afterId)
    set blt::ComboMenu::_private(afterId) -1
}

bind ComboMenu <B1-Leave> {
    blt::ComboMenu::trace "ComboMenu B1-Leave"
    blt::ComboMenu::AutoScroll %W %x %y
}

bind ComboMenu <Unmap> {
    after cancel $blt::ComboMenu::_private(afterId)
    set blt::ComboMenu::_private(afterId) -1
}

proc ::blt::ComboMenu::AutoScroll {w x y} {
    variable _private

    trace "autoscan $w $y"
    if { ![winfo exists $w] } {
	return
    }
    set i -1
    if { $y >= [winfo height $w] } {
	set i [$w next view.bottom]
    } elseif { $y < 0 } {
	set i [$w previous view.top]
    }
    if { $i > 0 } {
	trace $i
	$w activate $i
	$w see $i
    }
    set cmd [list blt::ComboMenu::AutoScroll $w $x $y]
    set _private(afterId) [after 50 $cmd]
}

proc blt::ComboMenu::ConfigureScrollbars { menu } {
    set ys [$menu cget -yscrollbar]
    if { $ys != "" } {
	if { [$menu cget -yscrollcommand] == "" } {
	    $menu configure -yscrollcommand [list $ys set]
	}
	if { [$ys cget -command] == "" } {
	    $ys configure -command [list $menu yview] -orient vertical \
		-highlightthickness 0
	}
    }
    set xs [$menu cget -xscrollbar]
    if { $xs != "" } {
	if { [$menu cget -xscrollcommand] == "" } {
	    $menu configure -xscrollcommand [list $xs set]
	}
	if { [$xs cget -command] == "" } {
	    $xs configure -command [list $menu xview] -orient horizontal \
		-highlightthickness 0
	}
    }
}

# ::tk_popup --
# This procedure pops up a menu and sets things up for traversing
# the menu and its submenus.
#
# Arguments:
# menu -		Name of the menu to be popped up.
# x, y -		Root coordinates at which to pop up the
#			menu.
# entry -		Index of a menu entry to center over (x,y).
#			If omitted or specified as {}, then menu's
#			upper-left corner goes at (x,y).

proc ::blt::ComboMenu::popup { menu x y } {
    variable _private

    blt::ComboMenu::trace "blt::ComboMenu::popup $menu $x $y"
    blt::ComboMenu::trace "blt::ComboMenu::popup popup=$_private(popup)"
    if { [grab current] == $menu } {
	blt::ComboMenu::trace "blt::ComboMenu::popup unposting $menu"
	UnpostMenu $menu
    } else {
	blt::ComboMenu::trace "blt::ComboMenu::popup posting $menu"
	$menu post $x $y popup
	if { [tk windowingsystem] == "x11" && [winfo viewable $menu] } {
	    set _private(activeComboMenu) $menu
	    set _private(popup) $menu
	    SaveGrab $menu
	    trace "popup: setting global grab on $menu"
	    grab -global $menu
	    focus $menu
	}
    }
}
