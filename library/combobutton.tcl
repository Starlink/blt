
namespace eval blt::ComboButton {
    variable privateData
    array set privateData {
	activeMenu  {}
	posted      {}
        activeItem  {}
        cursor      {}
        focus       {}
        oldGrab     {}
    }
}

bind ComboButton <Enter> {
    %W activate yes
}

bind ComboButton <Leave> {
    %W activate no
}

bind ComboButton <B1-Motion> {
    puts stderr "ComboButton <B1-Motion> %W state=[%W cget -state]"
    blt::ComboButton::NextButton %W %X %Y
}

bind ComboButton <B1-Leave> {
    puts stderr "ComboButton <B1-Leave> state=[%W cget -state]"
}

# Standard Motif bindings:

bind ComboButton <ButtonPress-1> {
    puts stderr "ComboButton <ButtonPress-1> state=[%W cget -state]"
    if { [%W cget -state] == "posted" } {
	puts stderr "unpost"
	%W unpost
	blt::ComboButton::UnpostMenu %W
    } else {
	grab -global %W
	blt::ComboButton::PostMenu %W
    }
}

bind ComboButton <ButtonRelease-1> {
    puts stderr "ComboButton <ButtonRelease-1> state=[%W cget -state]"
    if { [winfo containing -display  %W %X %Y] == "%W" } {
	puts stderr "invoke"
	%W invoke
    } else { 
	%W activate off
	puts stderr "unpost"
	blt::ComboButton::UnpostMenu %W
    }	
}

bind ComboButton <KeyPress-Down> {
    if { [%W cget -state] != "disabled" } {
	blt::ComboButton::PostMenu %W
    }
}

# Ignore all Alt, Meta, and Control keypresses unless explicitly bound.
# Otherwise, if a widget binding for one of these is defined, the
# <KeyPress> class binding will also fire and insert the character,
# which is wrong.  Ditto for Escape, Return, and Tab.

bind ComboButton <Alt-KeyPress> {# nothing}
bind ComboButton <Meta-KeyPress> { puts %K }
bind ComboButton <Control-KeyPress> {# nothing}
bind ComboButton <Escape> {# nothing}
#bind ComboButton <KP_Enter> {# nothing}
bind ComboButton <Tab> {# nothing}
if {[string equal [tk windowingsystem] "classic"] || 
    [string equal [tk windowingsystem] "aqua"]} {
    bind ComboButton <Command-KeyPress> {# nothing}
}


proc blt::ComboButton::SaveGrab { w } {
    variable privateData

    set grab [grab current $w]
    set privateData(oldGrab) ""
    if { $grab != "" } {
	set type [grab status $grab]
	if { $type == "global" } {
	    #set privateData(oldGrab) [list grab set -global $grab]
	} else {
	    #set privateData(oldGrab) [list grab set $grab]
	}	    
    } 
}

# ::blt::ComboButton::RestoreOldGrab --
# Restores the grab to what it was before TkSaveGrabInfo was called.
#

proc ::blt::ComboButton::RestoreOldGrab {} {
    variable privateData

    if { $privateData(oldGrab) != "" } {
    	# Be careful restoring the old grab, since it's window may not be
    	# visible anymore.
	catch $privateData(oldGrab)
	set privateData(oldGrab) ""
    }
}

# ::blt::ComboButton::PostMenu --
# Given a menubutton, this procedure does all the work of posting
# its associated menu and unposting any other menu that is currently
# posted.
#
# Arguments:
# w -			The name of the menubutton widget whose menu
#			is to be posted.
# x, y -		Root coordinates of cursor, used for positioning
#			option menus.  If not specified, then the center
#			of the menubutton is used for an option menu.

proc ::blt::ComboButton::PostMenu { cbutton } {
    variable privateData

    puts stderr "proc ComboButton::PostMenu $cbutton, state=[$cbutton cget -state]"
    if { [$cbutton cget -state] == "disabled" } {
	return
    }
    if { [$cbutton cget -state] == "posted" } {
	UnpostMenu $cbutton
	return
    } 
    set menu [$cbutton cget -menu]
    if { $menu == "" } {
	return
    }
    set last $privateData(posted)
    if { $last != "" } {
	UnpostMenu $last
    }
    set privateData(cursor) [$cbutton cget -cursor]
    $cbutton configure -cursor arrow
    
    set privateData(posted) $cbutton
    set privateData(focus) [focus]
    $menu activate none
    GenerateMenuSelect $menu

    # If this looks like an option menubutton then post the menu so
    # that the current entry is on top of the mouse.  Otherwise post
    # the menu just below the menubutton, as for a pull-down.

    update idletasks
    if { [catch { $cbutton post } msg] != 0 } {
	# Error posting menu (e.g. bogus -postcommand). Unpost it and
	# reflect the error.
	global errorInfo
	set savedInfo $errorInfo
	#
	UnpostMenu $cbutton 
	error $msg $savedInfo
    }

    focus $menu
    if { [winfo viewable $menu] } {
	SaveGrab $menu
	puts stderr "setting global grab on $menu"
	#grab -global $menu
    }
}

# ::blt::ComboButton::UnpostMenu --
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

proc ::blt::ComboButton::UnpostMenu { cbutton } {
    variable privateData

    puts stderr "proc ComboButton::UnpostMenu $cbutton"

    # Restore focus right away (otherwise X will take focus away when the
    # menu is unmapped and under some window managers (e.g. olvwm) we'll
    # lose the focus completely).
    catch { focus $privateData(focus) }
    set privateData(focus) ""

    # Unpost menu(s) and restore some stuff that's dependent on what was
    # posted.
    puts stderr "unposting $cbutton"
    $cbutton unpost
    set privateData(posted) {}
    if { [info exists privateData(cursor)] } {
	$cbutton configure -cursor $privateData(cursor)
    }
    if { [$cbutton cget -state] != "disabled" } {
	#$cbutton configure -state normal
    }
    set menu [$cbutton cget -menu]
    puts MENU=$menu
    puts GRAB=[grab current $menu]
    # Release grab, if any, and restore the previous grab, if there
    # was one.
    if { $menu != "" } {
	set grab [grab current $menu]
	if { $grab != "" } {
	    grab release $grab
	}
    }
    RestoreOldGrab
}

proc blt::ComboButton::GenerateMenuSelect {menu} {
    variable privateData

    set item [$menu index active]
    if { $privateData(activeMenu) != $menu || 
	 $privateData(activeItem) != $item } {
	set privateData(activeMenu) $menu
	set privateData(activeItem) $item
	event generate $menu <<MenuSelect>>
    }
}

proc blt::ComboButton::NextButton { cbutton rootx rooty } {
    variable privateData

    set next [winfo containing -display $cbutton $rootx $rooty]
    if { $next == "" || $next == $privateData(posted) || 
	 [winfo class $next] != "ComboButton" } {
	return
    }
    # Release the current combobutton, including removing the grab.
    event generate $cbutton <ButtonRelease-1>
    grab -global $next
    # Simulate pressing the new combobutton widget.
    event generate $next <ButtonPress-1>
}
