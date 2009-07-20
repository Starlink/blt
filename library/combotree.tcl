
# ======================================================================
#
# combotree.tcl
#
# ----------------------------------------------------------------------
# Bindings for the BLT combotree widget
# ----------------------------------------------------------------------
#
#   AUTHOR:  George Howlett
#            Bell Labs Innovations for Lucent Technologies
#            gah@lucent.com
#            http://www.tcltk.com/blt
#
#      RCS:  $Id: combotree.tcl,v 1.4 2009/02/23 04:49:19 ghowlett Exp $
#
# ----------------------------------------------------------------------
# Copyright (c) 1998  Lucent Technologies, Inc.
# ----------------------------------------------------------------------
#
# Permission to use, copy, modify, and distribute this software and its
# documentation for any purpose and without fee is hereby granted,
# provided that the above copyright notice appear in all copies and that
# both that the copyright notice and warranty disclaimer appear in
# supporting documentation, and that the names of Lucent Technologies
# any of their entities not be used in advertising or publicity
# pertaining to distribution of the software without specific, written
# prior permission.
#
# Lucent Technologies disclaims all warranties with regard to this
# software, including all implied warranties of merchantability and
# fitness.  In no event shall Lucent be liable for any special, indirect
# or consequential damages or any damages whatsoever resulting from loss
# of use, data or profits, whether in an action of contract, negligence
# or other tortuous action, arising out of or in connection with the use
# or performance of this software.
#
# ======================================================================

namespace eval blt::tv {
    set afterId ""
    set scroll 0
    set column ""
    set space   off
    set x 0
    set y 0
}

image create picture ::blt::normalOpenFolder -data {
    R0lGODlhEAANAMIAAAAAAH9/f///////AL+/vwAA/wAAAAAAACH5BAEAAAUALAAAAAAQAA0A
    AAM8WBrM+rAEQWmIb5KxiWjNInCkV32AJHRlGQBgDA7vdN4vUa8tC78qlrCWmvRKsJTquHkp
    ZTKAsiCtWq0JADs=
}
image create picture ::blt::normalCloseFolder -data {
    R0lGODlhEAANAMIAAAAAAH9/f///////AL+/vwAA/wAAAAAAACH5BAEAAAUALAAAAAAQAA0A
    AAM1WBrM+rAEMigJ8c3Kb3OSII6kGABhp1JnaK1VGwjwKwtvHqNzzd263M3H4n2OH1QBwGw6
    nQkAOw==
}

image create picture ::blt::activeOpenFolder -data {
    R0lGODlhEAANAMIAAAAAAH9/f/////+/AL+/vwAA/wAAAAAAACH5BAEAAAUALAAAAAAQAA0A
    AAM8WBrM+rAEQWmIb5KxiWjNInCkV32AJHRlGQBgDA7vdN4vUa8tC78qlrCWmvRKsJTquHkp
    ZTKAsiCtWq0JADs=
}

image create picture ::blt::activeCloseFolder -data {
    R0lGODlhEAANAMIAAAAAAH9/f/////+/AL+/vwAA/wAAAAAAACH5BAEAAAUALAAAAAAQAA0A
    AAM1WBrM+rAEMigJ8c3Kb3OSII6kGABhp1JnaK1VGwjwKwtvHqNzzd263M3H4n2OH1QBwGw6
    nQkAOw==
}

# ----------------------------------------------------------------------
#
# InitComboTree --
#
#	Invoked by internally by Combotree_Init routine.  Initializes
#	the default bindings for the combotree widget entries.  These
#	are local to the widget, so they can't be set through the
#	widget's class bind tags.
#
# ----------------------------------------------------------------------
proc blt::InitComboTree { w } {
    #
    # Active entry bindings
    #
    $w bind Entry <Enter> { 
	puts stderr "Entry <Enter> [%W index current]"
	%W activate current
	%W entry highlight current 
    }
    $w bind Entry <Leave> { 
	puts stderr "Entry <Leave> [%W index current]"
	%W activate none
	%W entry highlight "" 
    }

    #
    # Button bindings
    #
    $w button bind all <ButtonRelease-1> {
	set index [%W nearest %x %y blt::tv::who]
	if { [%W index current] == $index && $blt::tv::who == "button" } {
	    %W see -anchor nw current
	    %W toggle current
	}
    }
    $w button bind all <Enter> {
	%W button highlight current
    }
    $w button bind all <B1-Enter> {
	%W button highlight current
    }
    $w button bind all <Leave> {
	%W button highlight ""
    }

    #
    # ButtonPress-1
    #
    #	Performs the following operations:
    #
    #	1. Clears the previous selection.
    #	2. Selects the current entry.
    #	3. Sets the focus to this entry.
    #	4. Scrolls the entry into view.
    #	5. Sets the selection anchor to this entry, just in case
    #	   this is "multiple" mode.
    #
    
    #
    # ButtonRelease-1
    #
    #	For "multiple" mode only.  
    #
    $w bind Entry <ButtonRelease-3> { 
	puts stderr "Entry <ButtonRelease> [%W index current]"
	%W invoke active
	event generate [grab current] <ButtonRelease-1> 
	after cancel $blt::tv::afterId
	set blt::tv::scroll 0
    }
    $w bind Entry <ButtonRelease-1> { 
	puts stderr "Entry <ButtonRelease-1> [%W index current]"
	%W invoke active
	event generate [grab current] <ButtonRelease-1> 
	after cancel $blt::tv::afterId
	set blt::tv::scroll 0
    }

    $w bind Entry <Double-ButtonPress-1> {
	%W see -anchor nw active
	%W toggle active
    }

    #
    # Shift-ButtonPress-1
    #
    #	For "multiple" mode only.
    #

    $w bind Entry <Shift-ButtonPress-1> { 
	if { [%W cget -selectmode] == "multiple" && [%W selection present] } {
	    if { [%W index anchor] == "" } {
		%W selection anchor current
	    }
	    set index [%W index anchor]
	    %W selection clearall
	    %W selection set $index current
	} else {
	    blt::tv::SetSelectionAnchor %W current
	}
    }
    $w bind Entry <Shift-Double-ButtonPress-1> {
	# do nothing
    }
    $w bind Entry <Shift-B1-Motion> { 
	# do nothing
    }
    $w bind Entry <Shift-ButtonRelease-1> { 
	after cancel $blt::tv::afterId
	set blt::tv::scroll 0
    }

    #
    # Control-ButtonPress-1
    #
    #	For "multiple" mode only.  
    #
    $w bind Entry <Control-ButtonPress-1> { 
	if { [%W cget -selectmode] == "multiple" } {
	    set index [%W index current]
	    %W selection toggle $index
	    %W selection anchor $index
	} else {
	    blt::tv::SetSelectionAnchor %W current
	}
    }
    $w bind Entry <Control-Double-ButtonPress-1> {
	# do nothing
    }
    $w bind Entry <Control-B1-Motion> { 
	# do nothing
    }
    $w bind Entry <Control-ButtonRelease-1> { 
	after cancel $blt::tv::afterId
	set blt::tv::scroll 0
    }

    $w bind Entry <Control-Shift-ButtonPress-1> { 
	if { [%W cget -selectmode] == "multiple" && [%W selection present] } {
	    if { [%W index anchor] == "" } {
		%W selection anchor current
	    }
	    if { [%W selection includes anchor] } {
		%W selection set anchor current
	    } else {
		%W selection clear anchor current
		%W selection set current
	    }
	} else {
	    blt::tv::SetSelectionAnchor %W current
	}
    }
    $w bind Entry <Control-Shift-Double-ButtonPress-1> {
	# do nothing
    }
    $w bind Entry <Control-Shift-B1-Motion> { 
	# do nothing
    }

    $w bind Entry <Shift-ButtonPress-3> { 
	puts stderr "entry bind buttonpress-3 %W %X %Y"
	blt::tv::EditColumn %W %X %Y
    }
}

# ----------------------------------------------------------------------
#
# AutoScroll --
#
#	Invoked when the user is selecting elements in a combotree
#	widget and drags the mouse pointer outside of the widget.
#	Scrolls the view in the direction of the pointer.
#
# ----------------------------------------------------------------------
proc blt::tv::AutoScroll { w } {
    if { ![winfo exists $w] } {
	return
    }
    set x $blt::tv::x
    set y $blt::tv::y

    set index [$w nearest $x $y]

    if {$y >= [winfo height $w]} {
	$w yview scroll 1 units
	set neighbor down
    } elseif {$y < 0} {
	$w yview scroll -1 units
	set neighbor up
    } else {
	set neighbor $index
    }
    blt::tv::SetSelectionAnchor $w $neighbor
    set ::blt::tv::afterId [after 50 blt::tv::AutoScroll $w]
}

proc blt::tv::SetSelectionAnchor { w tagOrId } {
    set index [$w index $tagOrId]
    # If the anchor hasn't changed, don't do anything
    if { $index != [$w index anchor] } {
	#$w selection clearall
	$w see $index
	#$w focus $index
	#$w selection set $index
	#$w selection anchor $index
    }
}

# ----------------------------------------------------------------------
#
# MoveFocus --
#
#	Invoked by KeyPress bindings.  Moves the active selection to
#	the entry <where>, which is an index such as "up", "down",
#	"prevsibling", "nextsibling", etc.
#
# ----------------------------------------------------------------------
proc blt::tv::MoveFocus { w index } {
    $w activate $index
    $w see active
}

# ----------------------------------------------------------------------
#
# MovePage --
#
#	Invoked by KeyPress bindings.  Pages the current view up or
#	down.  The <where> argument should be either "top" or
#	"bottom".
#
# ----------------------------------------------------------------------
proc blt::tv::MovePage { w where } {

    # If the focus is already at the top/bottom of the window, we want
    # to scroll a page. It's really one page minus an entry because we
    # want to see the last entry on the next/last page.
    if { [$w index focus] == [$w index view.$where] } {
        if {$where == "top"} {
	    $w yview scroll -1 pages
	    $w yview scroll 1 units
        } else {
	    $w yview scroll 1 pages
	    $w yview scroll -1 units
        }
    }
    update

    # Adjust the entry focus and the view.  Also activate the entry.
    # just in case the mouse point is not in the widget.
    $w entry highlight view.$where
    $w focus view.$where
    $w see view.$where
    if { [$w cget -selectmode] == "single" } {
        $w selection clearall
        $w selection set focus
    }
}

# ----------------------------------------------------------------------
#
# NextMatch --
#
#	Invoked by KeyPress bindings.  Searches for an entry that
#	starts with the letter <char> and makes that entry active.
#
# ----------------------------------------------------------------------
proc blt::tv::NextMatch { w key } {
    if {[string match {[ -~]} $key]} {
	set last [$w index focus]
	set next [$w index next]
	while { $next != $last } {
	    set label [$w entry cget $next -label]
	    set label [string index $label 0]
	    if { [string tolower $label] == [string tolower $key] } {
		break
	    }
	    set next [$w index -at $next next]
	}
	$w focus $next
	if {[$w cget -selectmode] == "single"} {
	    $w selection clearall
	    $w selection set focus
	}
	$w see focus
    }
}

# 
# ButtonPress assignments
#
#	B1-Enter	start auto-scrolling
#	B1-Leave	stop auto-scrolling
#	ButtonPress-2	start scan
#	B2-Motion	adjust scan
#	ButtonRelease-2 stop scan
#

bind ComboTree <ButtonPress-2> {
    set blt::tv::cursor [%W cget -cursor]
    %W configure -cursor hand1
    %W scan mark %x %y
}

bind ComboTree <B2-Motion> {
    %W scan dragto %x %y
}

bind ComboTree <ButtonRelease-2> {
    %W configure -cursor $blt::tv::cursor
}

bind ComboTree <B1-Leave> {
    if { $blt::tv::scroll } {
	blt::tv::AutoScroll %W 
    }
}

bind ComboTree <B1-Enter> {
    after cancel $blt::tv::afterId
}

# 
# KeyPress assignments
#
#	Up			
#	Down
#	Shift-Up
#	Shift-Down
#	Prior (PageUp)
#	Next  (PageDn)
#	Left
#	Right
#	space		Start selection toggle of entry currently with focus.
#	Return		Start selection toggle of entry currently with focus.
#	Home
#	End
#	F1
#	F2
#	ASCII char	Go to next open entry starting with character.
#
# KeyRelease
#
#	space		Stop selection toggle of entry currently with focus.
#	Return		Stop selection toggle of entry currently with focus.


bind ComboTree <KeyPress-Up> {
    %W activate up
    %W see active
}

bind ComboTree <KeyPress-Down> {
    %W activate down
    %W see active
}

bind ComboTree <Shift-KeyPress-Up> {
    %W activate prevsibling
    %W see active
}

bind ComboTree <Shift-KeyPress-Down> {
    %W activate nextsibling
    %W see active
}

bind ComboTree <KeyPress-Prior> {
    %W activate top
    %W see active
}

bind ComboTree <KeyPress-Next> {
    %W activate bottom
    %W see active
}

bind ComboTree <KeyPress-Left> {
    %W close active
}

bind ComboTree <KeyPress-Right> {
    %W open active
    %W see active
}

bind ComboTree <KeyPress-space> {
    %W invoke active
}

bind ComboTree <KeyPress-Return> {
    %W invoke active
}

bind ComboTree <KeyPress> {
    blt::tv::NextMatch %W %A
}

bind ComboTree <KeyPress-Home> {
    %W open top
    %W see active -anchor w
}

bind ComboTree <KeyPress-End> {
    blt::tv::MoveFocus %W bottom
}

bind ComboTree <KeyPress-F1> {
    %W open -recurse root
}

bind ComboTree <KeyPress-F2> {
    eval %W close -r [%W entry children root] 
}

if {[string equal "x11" [tk windowingsystem]]} {
    bind ComboTree <4> {
	%W yview scroll -5 units
    }
    bind ComboTree <5> {
	%W yview scroll 5 units
    }
} else {
    bind ComboTree <MouseWheel> {
	%W yview scroll [expr {- (%D / 120) * 4}] units
    }
}

