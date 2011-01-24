
# button.tcl --
#
# This file defines the default bindings for Tk label, button,
# checkbutton, and radiobutton widgets and provides procedures
# that help in implementing those bindings.
#
# RCS: @(#) $Id: pushbutton.tcl,v 1.3 2010/08/29 16:32:24 ghowlett Exp $
#
# Copyright (c) 1992-1994 The Regents of the University of California.
# Copyright (c) 1994-1996 Sun Microsystems, Inc.
# Copyright (c) 2002 ActiveState Corporation.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

namespace eval blt::Button {
    array set _private {
	afterId -1
	repeated  ""
	window ""
	trace		0
	buttonWindow ""
    }
    proc trace { mesg } {
	variable _private
	if { $_private(trace) } {
	    puts stderr $mesg
	}
    }
}

#-------------------------------------------------------------------------
# The code below creates the default class bindings for buttons.
#-------------------------------------------------------------------------

switch -glob -- [tk windowingsystem] {
    "classic" - "aqua" {
	source [file join $blt_library macButton.tcl]
    }
    "x11" {
	source [file join $blt_library unixButton.tcl]
    }
    "win*" {
	source [file join $blt_library winButton.tcl]
    }
}

##################
# Shared routines
##################

# Invoke --
#
# The procedure below is called when a button is invoked through
# the keyboard.  It simulates a press of the button via the mouse.
#
# Arguments:
# w -		The name of the widget.

proc ::blt::Button::Invoke w {
    if {[$w cget -state] != "disabled"} {
	set oldRelief [$w cget -relief]
	set oldState [$w cget -state]
	$w configure -state active -relief sunken
	update idletasks
	after 100
	$w configure -state $oldState -relief $oldRelief
	uplevel #0 [list $w invoke]
    }
}

# ::blt::Button::AutoInvoke --
#
#	Invoke an auto-repeating button, and set it up to continue to repeat.
#
# Arguments:
#	w	button to invoke.
#
# Results:
#	None.
#
# Side effects:
#	May create an after event to call ::blt::Button::AutoInvoke.

proc ::blt::Button::AutoInvoke {w} {
    variable _private
    after cancel $_private(afterId)
    set delay [$w cget -repeatinterval]
    if {$_private(window) eq $w} {
	incr _private(repeated)
	uplevel #0 [list $w invoke]
    }
    if {$delay > 0} {
	set _private(afterId) [after $delay [list blt::Button::AutoInvoke $w]]
    }
}

# ::blt::Button::CheckRadioInvoke --
# The procedure below is invoked when the mouse button is pressed in
# a checkbutton or radiobutton widget, or when the widget is invoked
# through the keyboard.  It invokes the widget if it
# isn't disabled.
#
# Arguments:
# w -		The name of the widget.
# cmd -		The subcommand to invoke (one of invoke, select, or deselect).

proc ::blt::Button::CheckRadioInvoke {w {cmd invoke}} {
    if {[$w cget -state] != "disabled"} {
	uplevel #0 [list $w $cmd]
    }
}

bind TkButton <space> {
    blt::Button::Invoke %W
}

bind TkCheckbutton <space> {
    blt::Button::CheckRadioInvoke %W
}

bind TkRadiobutton <space> {
    blt::Button::CheckRadioInvoke %W
}

bind TkPushbutton <space> {
    blt::Button::CheckRadioInvoke %W
}

bind TkButton <FocusIn> {
}

bind TkButton <Enter> {
    blt::Button::Enter %W
}
bind TkButton <Leave> {
    blt::Button::Leave %W
}
bind TkButton <1> {
    blt::Button::Down %W
}
bind TkButton <ButtonRelease-1> {
    blt::Button::Up %W
}

bind TkCheckbutton <FocusIn> {
}

bind TkCheckbutton <Leave> {
    blt::Button::Leave %W
}

bind TkRadiobutton <FocusIn> {}

bind TkRadiobutton <Leave> {
    blt::Button::Leave %W
}

bind TkPushbutton <FocusIn> {}

bind TkPushbutton <Leave> {
    blt::Button::Leave %W
}

