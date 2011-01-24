
namespace eval ::blt::ComboEntry {
    variable _private
    array set _private {
	activeItem      {}
	afterId		-1
	b1		""
	lastFocus       {}
	mouseMoved      0
	oldGrab         {}
	postingButton   {}
	arrowRelief	raised
	trace		0
	lastX		-1
    }
    proc trace { mesg } {
	variable _private
	if { $_private(trace) } {
	    puts stderr $mesg
	}
    }
}

bind ComboEntry <Enter> {
    # Do nothing
}

bind ComboEntry <Leave> {
    %W activate off
}

# Standard Motif bindings:

bind ComboEntry <Motion> {
    %W activate [%W identify %x %y] 
}

bind ComboEntry <ButtonPress-1> {
    ::blt::ComboEntry::ButtonPress %W %x %y
}

bind ComboEntry <ButtonRelease-1> {
    blt::ComboEntry::trace "ComboEntry %W at %X,%Y <ButtonRelease-1> state=[%W cget -state], grab=[grab current]"
    after cancel $blt::ComboEntry::_private(afterId)
    switch -- [%W identify -root %X %Y] {
	"arrow" {
	    blt::ComboEntry::trace "invoke"
	    %W invoke
	}
	"close"	{
	    blt::ComboEntry::trace "button invoke"
	    %W button invoke
	}
	default { 
	    blt::ComboEntry::trace "unpost"
	    blt::ComboEntry::UnpostMenu %W
	}	
    }
    if { [info exists blt::ComboEntry::_private(arrowRelief)] } {
	%W configure -arrowrelief $blt::ComboEntry::_private(arrowRelief)
    }
}

bind ComboEntry <B1-Motion> {
    if { $blt::ComboEntry::_private(b1) != "arrow" } {
	%W selection to [%W closest %x]
    }
}
bind ComboEntry <B1-Enter> {
    after cancel $blt::ComboEntry::_private(afterId)
    set blt::ComboEntry::_private(afterId) -1
}
bind ComboEntry <B1-Leave> {
    blt::ComboEntry::trace "ComboEntry B1-Leave"
    if { $blt::ComboEntry::_private(b1) == "text" } {
	set blt::ComboEntry::_private(lastX) %x
	blt::ComboEntry::AutoScan %W
    }
}
bind ComboEntry <KeyPress-Down> {
    if { [%W cget -state] != "disabled" } {
	grab %W
	::blt::ComboEntry::PostMenu %W
    }
}
bind ComboEntry <Double-1> {
    blt::ComboEntry::trace "Double-1"
    if { [%W identify %x %y] == "arrow" } {
	::blt::ComboEntry::ButtonPress %W %x %y
    } else {
	%W icursor [%W closest %x]
	%W selection range \
	    [string wordstart [%W get] [%W index previous]] \
	    [string wordend   [%W get] [%W index insert]]
	%W icursor sel.last
	%W see insert
    }
}
bind ComboEntry <Triple-1> {
    blt::ComboEntry::trace "Triple-1"
    if { [%W identify %x %y] != "arrow" } {
	%W selection range 0 end
	%W icursor sel.last
    }
}
bind ComboEntry <Shift-1> {
    %W selection adjust @%x
}
bind ComboEntry <ButtonPress-2> {
    %W scan mark %x
}
bind ComboEntry <ButtonRelease-2> {
    if { abs([%W scan mark] - %x) <= 3 } {
	catch { 
	    %W insert insert [selection get]
	    %W see insert
	}
    }
}
bind ComboEntry <B2-Motion> {
    %W scan dragto %x
}
bind ComboEntry <Control-1> {
    %W icursor @%x
}
bind ComboEntry <KeyPress-Left> {
    if { [%W selection present] } {
	%W icursor sel.last
	%W selection clear
    } 
    %W icursor previous
    %W see insert
}
bind ComboEntry <KeyPress-Right> {
    if { [%W selection present] } {
	%W icursor sel.last
	%W selection clear
    } 
    %W icursor next
    %W see insert
}

bind ComboEntry <Shift-Left> {
    if {![%W selection present]} {
	%W selection range previous insert 
    } else {
	%W selection adjust previous
    }
    %W icursor previous
    %W see insert
}

bind ComboEntry <Shift-Right> {
    if {![%W selection present]} {
	%W selection range insert next
    } else {
	%W selection adjust next
    }
    %W icursor next
    %W see insert
}

bind ComboEntry <Shift-Control-Left> {
    set previous [string wordstart [%W get] [%W index previous]]
    if {![%W selection present]} {
	%W selection range $previous insert 
    } else {
	%W selection adjust $previous
    }
    %W icursor $previous
    %W see insert
}

bind ComboEntry <Shift-Control-Right> {
    set next [string wordend [%W get] [%W index insert]]
    if {![%W selection present]} {
	%W selection range insert $next
    } else {
	%W selection adjust $next
    }
    %W icursor $next
    %W see insert
}

bind ComboEntry <Home> {
    %W icursor 0
    %W see insert
}

bind ComboEntry <Shift-Home> {
    if {![%W selection present]} {
	%W selection range 0 insert
    } else {
	%W selection adjust 0
    }
    %W icursor 0
    %W see insert
}

bind ComboEntry <End> {
    %W icursor end
    %W see insert
}

bind ComboEntry <Shift-End> {
    if {![%W selection present]} {
	%W selection range insert end
    } else {
	%W selection adjust end
    }
    %W icursor end
    %W see insert
}

bind ComboEntry <Delete> {
    if {[%W selection present]} {
	%W delete sel.first sel.last
    } else {
	%W delete insert next
    }
}

bind ComboEntry <BackSpace> {
    if {[%W selection present]} {
	%W delete sel.first sel.last
    } else {
	%W delete previous insert 
	%W see insert
    }
}

bind ComboEntry <Control-space> {
    %W selection from insert
}

bind ComboEntry <Select> {
    %W selection from insert
}

bind ComboEntry <Control-Shift-space> {
    %W selection adjust insert
}

bind ComboEntry <Shift-Select> {
    %W selection adjust insert
}

bind ComboEntry <Control-slash> {
    %W selection range 0 end
}

bind ComboEntry <Control-backslash> {
    %W selection clear
}

bind ComboEntry <Control-z> {
    %W undo
    %W see insert
}

bind ComboEntry <Control-Z> {
    %W redo
    %W see insert
}

bind ComboEntry <Control-y> {
    %W redo
    %W see insert
}

bind ComboEntry <<Cut>> {
    if { [%W selection present] } {
	clipboard clear -displayof %W
	clipboard append -displayof %W [selection get]
	%W delete sel.first sel.last
    }
}

bind ComboEntry <<Copy>> {
    if { [%W selection present] } {
	clipboard clear -displayof %W
	clipboard append -displayof %W [selection get]
    }
}

bind ComboEntry <<Paste>> {
    %W insert insert [selection get]
    %W see insert
}

bind ComboEntry <<Clear>> {
    %W delete sel.first sel.last
}


bind Entry <<PasteSelection>> {
    if { $tk_strictMotif || 
	 ![info exists tk::ComboEntry::_private(mouseMoved)] || 
	 !$tk::ComboEntry::_private(mouseMoved)} {
	tk::EntryPaste %W %x
    }
}

# Paste
bind ComboEntry <Control-v> {
    %W insert insert [selection get]
}

# Cut
bind ComboEntry <Control-x> {
    if { [%W selection present] } {
	clipboard clear -displayof %W
	clipboard append -displayof %W [selection get]
	%W delete sel.first sel.last
    }
}
# Copy
bind ComboEntry <Control-c> {
    if { [%W selection present] } {
	clipboard clear -displayof %W
	clipboard append -displayof %W [selection get]
    }
}

bind ComboEntry <Return> {
    %W invoke 
}

bind ComboEntry <KeyPress> {
    if { [string compare %A {}] == 0 } {
	continue
    }
    if { [%W selection present] } {
	%W delete sel.first sel.last
    }
    %W insert insert %A
    %W see insert
}

# Additional emacs-like bindings:

bind ComboEntry <Control-a> {
    %W icursor 0
    %W see insert
}

bind ComboEntry <Control-b> {
    %W icursor previous 
    %W see insert
}

bind ComboEntry <Control-d> {
    if { [%W selection present] } {
	%W delete sel.first sel.last
    } else {
	%W delete insert next
    }
}

bind ComboEntry <Control-e> {
    %W icursor end
    %W see insert
}

bind ComboEntry <Control-f> {
    %W icursor next
    %W see insert
}

bind ComboEntry <Control-h> {
    if { [%W selection present] } {
	%W delete sel.first sel.last
    } else {
	%W delete previous insert 
	%W see insert
    }
}

bind ComboEntry <Control-k> {
    %W delete insert end
}

bind ComboEntry <Control-t> {
    set index [%W index insert]
    if { $index != 0 && $index != [%W index end] } {
	set a [string index [%W get] [%W index previous]]
	set b [string index [%W get] [%W index insert]]
	%W delete previous next
	%W insert insert "$b$a"
    }
}

bind ComboEntry <Alt-b> {
    %W icursor [string wordstart [%W get] [%W index previous]]
    %W see insert
}

bind ComboEntry <Alt-d> {
    %W delete insert [string wordend [%W get] [%W index insert]]
    %W see insert
}

bind ComboEntry <Alt-f> {
    %W icursor [string wordend [%W get] [%W index insert]]
    %W see insert
}

bind ComboEntry <Alt-BackSpace> {
    %W delete [string wordstart [%W get] [%W index previous]] insert
    %W see insert
}

bind ComboEntry <Alt-Delete> {
    %W delete insert [string wordend [%W get] [%W index insert]]
    %W see insert
}

####
bind ComboEntry <Meta-b> {
    %W icursor [string wordstart [%W get] [%W index previous]]
    %W see insert
}

bind ComboEntry <Meta-d> {
    %W delete insert [string wordend [%W get] [%W index insert]]
    %W see insert
}

bind ComboEntry <Meta-f> {
    %W icursor [string wordend [%W get] [%W index insert]]
    %W see insert
}

bind ComboEntry <Meta-BackSpace> {
    %W delete [string wordstart [%W get] [%W index previous]] insert
    %W see insert
}

bind ComboEntry <Meta-Delete> {
    %W delete insert [string wordend [%W get] [%W index insert]]
    %W see insert
}


# Ignore all Alt, Meta, and Control keypresses unless explicitly bound.
# Otherwise, if a widget binding for one of these is defined, the
# <KeyPress> class binding will also fire and insert the character,
# which is wrong.  Ditto for Escape, Return, and Tab.

bind ComboEntry <Alt-KeyPress> {
    # Do nothing.
}
bind ComboEntry <Meta-KeyPress> { 
    blt::ComboEntry::trace %K 
}
bind ComboEntry <Control-KeyPress> {
    # Do nothing.
}
bind ComboEntry <Escape> {
    # Do nothing.
}
bind ComboEntry <KP_Enter> {
    # Do nothing.
}
bind ComboEntry <Tab> {
    # Do nothing.
}
switch -- [tk windowingsystem] {
    "classic" - "aqua"  {
	bind ComboEntry <Command-KeyPress> {
	    # Do nothing.
	}
    }
}

proc ::blt::ComboEntry::AutoScan {w} {
    variable _private

    set x $_private(lastX)
    if { ![winfo exists $w] } {
	return
    }
    if { $x >= [winfo width $w] } {
	$w xview scroll 2 units
    } elseif { $x < 0 } {
	$w xview scroll -2 units
    }
    set _private(afterId) [after 50 [list blt::ComboEntry::AutoScan $w]]
}

proc ::blt::ComboEntry::SaveGrab { w } {
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

# ::blt::ComboEntry::RestoreOldGrab --
# Restores the grab to what it was before TkSaveGrabInfo was called.

proc ::blt::ComboEntry::RestoreOldGrab {} {
    variable _private

    if { $_private(oldGrab) != "" } {
    	# Be careful restoring the old grab, since it's window may not
	# be visible anymore.
	catch $_private(oldGrab)
	set _private(oldGrab) ""
    }
}

# ::blt::ComboEntry::PostMenu --
#
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

proc ::blt::ComboEntry::PostMenu { w } {
    variable _private

    trace "proc PostMenu $w, state=[$w cget -state]"
    if { [$w cget -state] == "disabled" } {
	return
    }
    if { [$w cget -state] == "posted" } {
	UnpostMenu $w
	return
    } 
    set menu [$w cget -menu]
    if { $menu == "" } {
	return
    }
    set cur $_private(postingButton)
    if { $cur != "" } {
	#
	UnpostMenu $cur
    }
    set _private(cursor) [$w cget -cursor]
    $w configure -cursor arrow
    
    set _private(postingButton) $w
    set _private(lastFocus) [focus]
    $menu activate none
    #blt::ComboEntry::GenerateMenuSelect $menu


    # If this looks like an option menubutton then post the menu so
    # that the current entry is on top of the mouse.  Otherwise post
    # the menu just below the menubutton, as for a pull-down.

    update idletasks
    if { [catch { $w post } msg] != 0 } {
	# Error posting menu (e.g. bogus -postcommand). Unpost it and
	# reflect the error.
	global errorInfo
	set savedInfo $errorInfo
	#
	UnpostMenu $w 
	error $msg $savedInfo
    }

    focus $menu
    set value [$w get]
    set index [$menu index $value]
    if { $index != -1 } {
	$menu see $index
	$menu activate $index
    }
    if { [winfo viewable $menu] } {
	SaveGrab $menu
	trace "setting global grab on $menu"
	#grab -global $menu
    }
}

# ::blt::ComboEntry::UnpostMenu --
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

proc ::blt::ComboEntry::UnpostMenu { w } {
    variable _private

    trace "proc UnpostMenu $w"
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
    set _private(postingButton) {}
    if { [info exists _private(cursor)] } {
	$w configure -cursor $_private(cursor)
    }
    if { [$w cget -state] != "disabled" } {
	#$w configure -state normal
    }
    set menu [$w cget -menu]
    if { $menu == "" } {
	return
    }
    trace MENU=$menu
    trace GRAB=[grab current $menu]

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

proc ::blt::ComboEntry::GenerateMenuSelect {menu} {
    if 0 {
    variable _private
    if { $_private(activeComboMenu) != $menu ||
	 $_private(activeItem) != [$menu index active] } {
	set _private(activeComboMenu) $menu
	set _private(activeItem) [$menu index active]
	event generate $menu <<MenuSelect>>
    }
    }
}

proc ::blt::ComboEntry::ButtonPress { w x y } {
    variable _private

    trace "blt::ComboEntry::ButtonPress $w state=[$w cget -state]"
    set _private(b1) [$w identify $x $y]
    trace "_private(b1) = \"$_private(b1)\""
    if { [$w cget -state] == "posted" } {
	trace "state = [$w cget -state]"
	$w unpost
	UnpostMenu $w
    } elseif { $_private(b1) == "arrow" } {
	set _private(arrowRelief) [$w cget -arrowrelief]
	trace "relief=$_private(arrowRelief)"
	if { [$w cget -state] != "disabled" } {
	    $w configure -arrowrelief sunken
	}
	grab -global $w
	PostMenu $w
    } else {
	trace "else: priv(v1)=$_private(b1) state=[$w cget -state]"
	focus $w
	$w icursor [$w closest $x]
	$w selection clear
	$w selection from insert
	$w configure -arrowrelief $_private(arrowRelief)
    }
}