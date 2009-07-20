
namespace eval ::blt::legend {
    variable Priv 
    array set Priv {
	afterId ""
	set scroll 0
	set space   off
	set x 0
	set y 0
    }
}

# ----------------------------------------------------------------------
#
# Initialize --
#
#	Invoked by internally by Treeview_Init routine.  Initializes
#	the default bindings for the treeview widget entries.  These
#	are local to the widget, so they can't be set through the
#	widget's class bind tags.
#
# ----------------------------------------------------------------------
proc blt::LegendSelections { w } {
    if 0 {
    #
    # Active entry bindings
    #
    $w legend bind all <Enter> { 
	%W entry highlight current 
    }
    $w legend bind all <Leave> { 
	%W entry highlight "" 
    }
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
    
    $w legend bind all <ButtonPress-1> { 	
	blt::legend::SetSelectionAnchor %W current
	set blt::legend::Priv(scroll) 1
    }

    #
    # B1-Motion
    #
    #	For "multiple" mode only.  Saves the current location of the
    #	pointer for auto-scrolling.  Resets the selection mark.  
    #
    $w legend bind all <B1-Motion> { 
	set blt::legend::Priv(x) %x
	set blt::legend::Priv(y) %y
	set elem [%W legend get @%x,%y]
	if { $elem != "" } {
	    if { [%W legend cget -selectmode] == "multiple" } {
		%W legend selection mark $elem
	    } else {
		blt::legend::SetSelectionAnchor %W $elem
	    }
	}
    }

    #
    # ButtonRelease-1
    #
    #	For "multiple" mode only.  
    #
    $w legend bind all <ButtonRelease-1> { 
	if { [%W legend cget -selectmode] == "multiple" } {
	    %W legend selection anchor current
	}
	after cancel $blt::legend::Priv(afterId)
	set blt::legend::Priv(scroll) 0
    }

    #
    # Shift-ButtonPress-1
    #
    #	For "multiple" mode only.
    #

    $w legend bind all <Shift-ButtonPress-1> { 
	if { [%W legend cget -selectmode] == "multiple" && 
	     [%W legend selection present] } {
	    if { [%W legend get anchor] == "" } {
		%W legend selection anchor current
	    }
	    set elem [%W legend get anchor]
	    %W legend selection clearall
	    %W legend selection set $elem current
	} else {
	    blt::legend::SetSelectionAnchor %W current
	}
    }
    $w legend bind all <Shift-Double-ButtonPress-1> {
	# do nothing
    }
    $w legend bind all <Shift-B1-Motion> { 
	# do nothing
    }
    $w legend bind all <Shift-ButtonRelease-1> { 
	after cancel $blt::legend::Priv(afterId)
	set blt::legend::Priv(scroll) 0
    }

    #
    # Control-ButtonPress-1
    #
    #	For "multiple" mode only.  
    #
    $w legend bind all <Control-ButtonPress-1> { 
	if { [%W legend cget -selectmode] == "multiple" } {
	    set elem [%W legend get current]
	    %W legend selection toggle $elem
	    %W legend selection anchor $elem
	} else {
	    blt::legend::SetSelectionAnchor %W current
	}
    }
    $w legend bind all <Control-Double-ButtonPress-1> {
	# do nothing
    }
    $w legend bind all <Control-B1-Motion> { 
	# do nothing
    }
    $w legend bind all <Control-ButtonRelease-1> { 
	after cancel $blt::legend::Priv(afterId)
	set blt::legend::Priv(scroll) 0
    }

    $w legend bind all <Control-Shift-ButtonPress-1> { 
	if { [%W legend cget -selectmode] == "multiple" && 
	     [%W legend selection present] } {
	    if { [%W legend get anchor] == "" } {
		%W selection anchor current
	    }
	    if { [%W legend selection includes anchor] } {
		%W legend selection set anchor current
	    } else {
		%W legend selection clear anchor current
		%W legend selection set current
	    }
	} else {
	    blt::legend::SetSelectionAnchor %W current
	}
    }
    $w legend bind all <Control-Shift-Double-ButtonPress-1> {
	# do nothing
    }
    $w legend bind all <Control-Shift-B1-Motion> { 
	# do nothing
    }
    $w legend bind all <KeyPress-Up> {
	blt::legend::MoveFocus %W previous.row
	if { $blt::legend::Priv(space) } {
	    %W legend selection toggle focus
	}
    }
    $w legend bind all <KeyPress-Down> {
	blt::legend::MoveFocus %W next.row
	if { $blt::legend::Priv(space) } {
	    %W legend selection toggle focus
	}
    }
    $w legend bind all <KeyPress-Left> {
	blt::legend::MoveFocus %W previous.column
	if { $blt::legend::Priv(space) } {
	    %W legend selection toggle focus
	}
    }
    $w legend bind all <KeyPress-Right> {
	blt::legend::MoveFocus %W next.column
	if { $blt::legend::Priv(space) } {
	    %W legend selection toggle focus
	}
    }
    $w legend bind all <KeyPress-space> {
	if { [%W legend cget -selectmode] == "single" } {
	    if { [%W legend selection includes focus] } {
		%W legend selection clearall
	    } else {
		%W legend selection clearall
		%W legend selection set focus
	    }
	} else {
	    %W legend selection toggle focus
	}
	set blt::legend::Priv(space) on
    }

    $w legend bind all <KeyRelease-space> { 
	set blt::legend::Priv(space) off
    }
    $w legend bind all <KeyPress-Return> {
	blt::legend::MoveFocus %W focus
	set blt::legend::Priv(space) on
    }
    $w legend bind all <KeyRelease-Return> { 
	set blt::legend::Priv(space) off
    }
    $w legend bind all <KeyPress-Home> {
	blt::legend::MoveFocus %W first
    }
    $w legend bind all <KeyPress-End> {
	blt::tv::MoveFocus %W last
    }
}

proc blt::legend::SetSelectionAnchor { w tagOrId } {
    set elem [$w legend get $tagOrId]
    # If the anchor hasn't changed, don't do anything
    if { $elem != [$w legend get anchor] } {
	$w legend selection clearall
	$w legend focus $elem
	$w legend selection set $elem
	$w legend selection anchor $elem
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
proc blt::legend::MoveFocus { w elem } {
    catch {$w legend focus $elem} result
    puts stderr "result=$result elem=$elem"
    if { [$w legend cget -selectmode] == "single" } {
        $w legend selection clearall
        $w legend selection set focus
	$w legend selection anchor focus
    }
}


proc Blt_ActiveLegend { graph } {
    $graph legend bind all <Enter> [list blt::ActivateLegend $graph ]
    $graph legend bind all <Leave> [list blt::DeactivateLegend $graph]
    $graph legend bind all <ButtonPress-1> [list blt::HighlightLegend $graph]
}

proc Blt_Crosshairs { graph } {
    blt::Crosshairs $graph 
}

proc Blt_ResetCrosshairs { graph state } {
    blt::Crosshairs $graph "Any-Motion" $state
}

proc Blt_ZoomStack { graph } {
    blt::ZoomStack $graph
}

proc Blt_PrintKey { graph } {
    blt::PrintKey $graph
}

proc Blt_ClosestPoint { graph } {
    blt::ClosestPoint $graph
}

#
# The following procedures that reside in the "blt" namespace are
# supposed to be private.
#

proc blt::ActivateLegend { graph } {
    set elem [$graph legend get current]
    $graph legend activate $elem
}
proc blt::DeactivateLegend { graph } {
    set elem [$graph legend get current]
    $graph legend deactivate $elem
}

proc blt::HighlightLegend { graph } {
    set elem [$graph legend get current]
    if { $elem != ""  } {
      set relief [$graph element cget $elem -labelrelief]
      if { $relief == "flat" } {
	$graph element configure $elem -labelrelief raised
	$graph element activate $elem
      } else {
	$graph element configure $elem -labelrelief flat
	$graph element deactivate $elem
      }
   }
}

proc blt::Crosshairs { graph {event "Any-Motion"} {state "on"}} {
    $graph crosshairs $state
    bind crosshairs-$graph <$event>   {
	%W crosshairs configure -position @%x,%y 
    }
    bind crosshairs-$graph <Leave>   {
	%W crosshairs off
    }
    bind crosshairs-$graph <Enter>   {
	%W crosshairs on
    }
    $graph crosshairs configure -color red
    if { $state == "on" } {
	blt::AddBindTag $graph crosshairs-$graph
    } elseif { $state == "off" } {
	blt::RemoveBindTag $graph crosshairs-$graph
    }
}

proc blt::InitStack { graph } {
    global zoomInfo
    set zoomInfo($graph,interval) 100
    set zoomInfo($graph,afterId) 0
    set zoomInfo($graph,A,x) {}
    set zoomInfo($graph,A,y) {}
    set zoomInfo($graph,B,x) {}
    set zoomInfo($graph,B,y) {}
    set zoomInfo($graph,stack) {}
    set zoomInfo($graph,corner) A
}

proc blt::ZoomStack { graph {start "ButtonPress-1"} {reset "ButtonPress-3"} } {
    global zoomInfo zoomMod
    
    blt::InitStack $graph
    
    if { [info exists zoomMod] } {
	set modifier $zoomMod
    } else {
	set modifier ""
    }
    bind zoom-$graph <${modifier}${start}> { blt::SetZoomPoint %W %x %y }
    bind zoom-$graph <${modifier}${reset}> { 
	if { [%W inside %x %y] } { 
	    blt::ResetZoom %W 
	}
    }
    blt::AddBindTag $graph zoom-$graph
}

proc blt::PrintKey { graph {event "Shift-ButtonRelease-3"} } {
    bind print-$graph <$event>  { Blt_PostScriptDialog %W }
    blt::AddBindTag $graph print-$graph
}

proc blt::ClosestPoint { graph {event "Control-ButtonPress-2"} } {
    bind closest-point-$graph <$event>  {
	blt::FindElement %W %x %y
    }
    blt::AddBindTag $graph closest-point-$graph
}

proc blt::AddBindTag { widget tag } {
    set oldTagList [bindtags $widget]
    if { [lsearch $oldTagList $tag] < 0 } {
	bindtags $widget [linsert $oldTagList 0  $tag]
    }
}

proc blt::RemoveBindTag { widget tag } {
    set oldTagList [bindtags $widget]
    set index [lsearch $oldTagList $tag]
    if { $index >= 0 } {
	bindtags $widget [lreplace $oldTagList $index $index]
    }
}

proc blt::FindElement { graph x y } {
    array set info [$graph element closest $x $y -interpolate yes]
    if { ![info exists info(name)] } {
	beep
	return
    }
    # --------------------------------------------------------------
    # find(name)		- element Id
    # find(index)		- index of closest point
    # find(x) find(y)		- coordinates of closest point
    #				  or closest point on line segment.
    # find(dist)		- distance from sample coordinate
    # --------------------------------------------------------------
    set markerName "bltClosest_$info(name)"
    catch { $graph marker delete $markerName }
    $graph marker create text -coords { $info(x) $info(y) } \
	-name $markerName \
	-text "$info(name): $info(dist)\nindex $info(index)" \
	-font "Arial 6" \
	-anchor center -justify left \
	-yoffset 0 -bg {} 

    set coords [$graph invtransform $x $y]
    set nx [lindex $coords 0]
    set ny [lindex $coords 1]

    $graph marker create line -coords "$nx $ny $info(x) $info(y)" \
	-name line.$markerName 

    blt::FlashPoint $graph $info(name) $info(index) 10
    blt::FlashPoint $graph $info(name) [expr $info(index) + 1] 10
}

proc blt::FlashPoint { graph name index count } {
    if { $count & 1 } {
        $graph element deactivate $name 
    } else {
        $graph element activate $name $index
    }
    incr count -1
    if { $count > 0 } {
	after 200 blt::FlashPoint $graph $name $index $count
	update
    } else {
	eval $graph marker delete [$graph marker names "bltClosest_*"]
    }
}

proc blt::GetCoords { graph x y index } {
    global zoomInfo
    if { [$graph cget -invertxy] } {
	set zoomInfo($graph,$index,x) $y
	set zoomInfo($graph,$index,y) $x
    } else {
	set zoomInfo($graph,$index,x) $x
	set zoomInfo($graph,$index,y) $y
    }
}

proc blt::MarkPoint { graph index } {
    global zoomInfo
    if { [llength [$graph xaxis use]] > 0 } {
	set x [$graph xaxis invtransform $zoomInfo($graph,$index,x)]
    } else if { [llength [$graph x2axis use]] > 0 } {
	set x [$graph x2axis invtransform $zoomInfo($graph,$index,x)]
    }
    if { [llength [$graph yaxis use]] > 0 } {
	set y [$graph yaxis invtransform $zoomInfo($graph,$index,y)]
    } else if { [llength [$graph y2axis use]] > 0 } {
	set y [$graph y2axis invtransform $zoomInfo($graph,$index,y)]
    }
    set marker "zoomText_$index"
    set text [format "x=%.4g\ny=%.4g" $x $y] 

    if [$graph marker exists $marker] {
     	$graph marker configure $marker -coords { $x $y } -text $text 
    } else {
    	$graph marker create text -coords { $x $y } -name $marker \
   	    -font "mathmatica1 10" \
	    -text $text -anchor center -bg {} -justify left
    }
}

proc blt::DestroyZoomTitle { graph } {
    global zoomInfo

    if { $zoomInfo($graph,corner) == "A" } {
	catch { $graph marker delete "zoomTitle" }
    }
}

proc blt::PopZoom { graph } {
    global zoomInfo

    set zoomStack $zoomInfo($graph,stack)
    if { [llength $zoomStack] > 0 } {
	set cmd [lindex $zoomStack 0]
	set zoomInfo($graph,stack) [lrange $zoomStack 1 end]
	eval $cmd
	blt::ZoomTitleLast $graph
	busy hold $graph
	update
	busy release $graph
	after 2000 "blt::DestroyZoomTitle $graph"
    } else {
	catch { $graph marker delete "zoomTitle" }
    }
}

# Push the old axis limits on the stack and set the new ones

proc blt::PushZoom { graph } {
    global zoomInfo
    eval $graph marker delete [$graph marker names "zoom*"]
    if { [info exists zoomInfo($graph,afterId)] } {
	after cancel $zoomInfo($graph,afterId)
    }
    set x1 $zoomInfo($graph,A,x)
    set y1 $zoomInfo($graph,A,y)
    set x2 $zoomInfo($graph,B,x)
    set y2 $zoomInfo($graph,B,y)

    if { ($x1 == $x2) || ($y1 == $y2) } { 
	# No delta, revert to start
	return
    }
    set cmd {}
    foreach margin { xaxis yaxis x2axis y2axis } {
	foreach axis [$graph $margin use] {
	    set min [$graph axis cget $axis -min] 
	    set max [$graph axis cget $axis -max]
	    set logscale  [$graph axis cget $axis -logscale]
	    # Save the current scale (log or linear) so that we can restore
	    # it.  This is for the case where the user changes to logscale
	    # while zooming.  A previously pushed axis limit could be
	    # negative.  It seems better for popping the zoom stack to restore
	    # a previous view (not convert the ranges).
	    set c [list $graph axis configure $axis]
	    lappend c -min $min -max $max -logscale $logscale
	    append cmd "$c\n"
	}
    }

    # This effectively pushes the command to reset the graph to the current
    # zoom level onto the stack.  This is useful if the new axis ranges are
    # bad and we need to reset the zoom stack.
    set zoomInfo($graph,stack) [linsert $zoomInfo($graph,stack) 0 $cmd]

    foreach margin { xaxis x2axis } {
	if { [$graph $margin cget -hide] } {
	    continue;		# Don't set zoom on axes not displayed.
	}
	foreach axis [$graph $margin use] {
	    set min [$graph axis invtransform $axis $x1]
	    set max [$graph axis invtransform $axis $x2]
	    if { ![SetAxisRanges $graph $axis $min $max] } {
		blt::PopZoom $graph
		bell
		return
	    }
	}
    }
    foreach margin { yaxis y2axis } {
	if { [$graph $margin cget -hide] } {
	    continue;		# Don't set zoom on axes not displayed. 
	}
	foreach axis [$graph $margin use] {
	    set min [$graph axis invtransform $axis $y1]
	    set max [$graph axis invtransform $axis $y2]
	    if { ![SetAxisRanges $graph $axis $min $max] } {
		blt::PopZoom $graph
		bell
		return
	    }
	}
    }
    busy hold $graph 
    update;				# This "update" redraws the graph
    busy release $graph
}

proc blt::SetAxisRanges { graph axis min max } {
    if { $min > $max } { 
	set tmp $max; set max $min; set min $tmp
    }
    if { [catch { $graph axis configure $axis -min $min -max $max }] != 0 } {
	return 0
    }
    return 1
}

#
# This routine terminates either an existing zoom, or pops back to
# the previous zoom level (if no zoom is in progress).
#

proc blt::ResetZoom { graph } {
    global zoomInfo 

    if { ![info exists zoomInfo($graph,corner)] } {
	blt::InitStack $graph 
    }
    eval $graph marker delete [$graph marker names "zoom*"]

    if { $zoomInfo($graph,corner) == "A" } {
	# Reset the whole axis
	blt::PopZoom $graph
    } else {
	global zoomMod

	if { [info exists zoomMod] } {
	    set modifier $zoomMod
	} else {
	    set modifier "Any-"
	}
	set zoomInfo($graph,corner) A
	blt::RemoveBindTag $graph select-region-$graph
    }
}

option add *zoomTitle.font	  "Arial 14"
option add *zoomTitle.shadow	  yellow4
option add *zoomTitle.foreground  yellow1
option add *zoomTitle.coords	  "-Inf Inf"

proc blt::ZoomTitleNext { graph } {
    global zoomInfo
    set level [expr [llength $zoomInfo($graph,stack)] + 1]
    if { [$graph cget -invertxy] } {
	set coords "-Inf -Inf"
    } else {
	set coords "-Inf Inf"
    }
    $graph marker create text -name "zoomTitle" -text "Zoom #$level" \
	-coords $coords -bindtags "" -anchor nw
}

proc blt::ZoomTitleLast { graph } {
    global zoomInfo

    set level [llength $zoomInfo($graph,stack)]
    if { $level > 0 } {
     	$graph marker create text -name "zoomTitle" -anchor nw \
	    -text "Zoom #$level" 
    }
}


proc blt::SetZoomPoint { graph x y } {
    global zoomInfo zoomMod
    if { ![info exists zoomInfo($graph,corner)] } {
	blt::InitStack $graph
    }
    blt::GetCoords $graph $x $y $zoomInfo($graph,corner)
    if { [info exists zoomMod] } {
	set modifier $zoomMod
    } else {
	set modifier "Any-"
    }
    bind select-region-$graph <${modifier}Motion> { 
	blt::GetCoords %W %x %y B
	#blt::MarkPoint $graph B
	blt::Box %W
    }
    if { $zoomInfo($graph,corner) == "A" } {
	if { ![$graph inside $x $y] } {
	    return
	}
	# First corner selected, start watching motion events

	#blt::MarkPoint $graph A
	blt::ZoomTitleNext $graph 

	blt::AddBindTag $graph select-region-$graph
	set zoomInfo($graph,corner) B
    } else {
	# Delete the modal binding
	blt::RemoveBindTag $graph select-region-$graph
	blt::PushZoom $graph 
	set zoomInfo($graph,corner) A
    }
}

option add *zoomOutline.dashes		4	
option add *zoomTitle.anchor		nw
option add *zoomOutline.lineWidth	2
option add *zoomOutline.xor		yes

proc blt::MarchingAnts { graph offset } {
    global zoomInfo

    incr offset
    # wrap the counter after 2^16
    set offset [expr $offset & 0xFFFF]
    if { [$graph marker exists zoomOutline] } {
	$graph marker configure zoomOutline -dashoffset $offset 
	set interval $zoomInfo($graph,interval)
	set id [after $interval [list blt::MarchingAnts $graph $offset]]
	set zoomInfo($graph,afterId) $id
    }
}

proc blt::Box { graph } {
    global zoomInfo

    if { $zoomInfo($graph,A,x) > $zoomInfo($graph,B,x) } { 
	set x1 [$graph xaxis invtransform $zoomInfo($graph,B,x)]
	set y1 [$graph yaxis invtransform $zoomInfo($graph,B,y)]
	set x2 [$graph xaxis invtransform $zoomInfo($graph,A,x)]
	set y2 [$graph yaxis invtransform $zoomInfo($graph,A,y)]
    } else {
	set x1 [$graph xaxis invtransform $zoomInfo($graph,A,x)]
	set y1 [$graph yaxis invtransform $zoomInfo($graph,A,y)]
	set x2 [$graph xaxis invtransform $zoomInfo($graph,B,x)]
	set y2 [$graph yaxis invtransform $zoomInfo($graph,B,y)]
    }
    set coords { $x1 $y1 $x2 $y1 $x2 $y2 $x1 $y2 $x1 $y1 }
    if { [$graph marker exists "zoomOutline"] } {
	$graph marker configure "zoomOutline" -coords $coords 
    } else {
	set X [lindex [$graph xaxis use] 0]
	set Y [lindex [$graph yaxis use] 0]
	$graph marker create line -coords $coords -name "zoomOutline" \
	    -mapx $X -mapy $Y
	set interval $zoomInfo($graph,interval)
	set id [after $interval [list blt::MarchingAnts $graph 0]]
	set zoomInfo($graph,afterId) $id
    }
}


proc Blt_PostScriptDialog { graph } {
    set top $graph.top
    toplevel $top

    foreach var { center landscape maxpect preview decorations padx 
	pady paperwidth paperheight width height colormode } {
	global $graph.$var
	set $graph.$var [$graph postscript cget -$var]
    }
    set row 1
    set col 0
    label $top.title -text "PostScript Options"
    blt::table $top $top.title -cspan 7
    foreach bool { center landscape maxpect preview decorations } {
	set w $top.$bool-label
	label $w -text "-$bool" -font "courier 12"
	blt::table $top $row,$col $w -anchor e -pady { 2 0 } -padx { 0 4 }
	set w $top.$bool-yes
	global $graph.$bool
	radiobutton $w -text "yes" -variable $graph.$bool -value 1
	blt::table $top $row,$col+1 $w -anchor w
	set w $top.$bool-no
	radiobutton $w -text "no" -variable $graph.$bool -value 0
	blt::table $top $row,$col+2 $w -anchor w
	incr row
    }
    label $top.modes -text "-colormode" -font "courier 12"
    blt::table $top $row,0 $top.modes -anchor e  -pady { 2 0 } -padx { 0 4 }
    set col 1
    foreach m { color greyscale } {
	set w $top.$m
	radiobutton $w -text $m -variable $graph.colormode -value $m
	blt::table $top $row,$col $w -anchor w
	incr col
    }
    set row 1
    frame $top.sep -width 2 -bd 1 -relief sunken
    blt::table $top $row,3 $top.sep -fill y -rspan 6
    set col 4
    foreach value { padx pady paperwidth paperheight width height } {
	set w $top.$value-label
	label $w -text "-$value" -font "courier 12"
	blt::table $top $row,$col $w -anchor e  -pady { 2 0 } -padx { 0 4 }
	set w $top.$value-entry
	global $graph.$value
	entry $w -textvariable $graph.$value -width 8
	blt::table $top $row,$col+1 $w -cspan 2 -anchor w -padx 8
	incr row
    }
    blt::table configure $top c3 -width .125i
    button $top.cancel -text "Cancel" -command "destroy $top"
    blt::table $top $row,0 $top.cancel  -width 1i -pady 2 -cspan 3
    button $top.reset -text "Reset" -command "destroy $top"
    #blt::table $top $row,1 $top.reset  -width 1i
    button $top.print -text "Print" -command "blt::ResetPostScript $graph"
    blt::table $top $row,4 $top.print  -width 1i -pady 2 -cspan 2
}

proc blt::ResetPostScript { graph } {
    foreach var { center landscape maxpect preview decorations padx 
	pady paperwidth paperheight width height colormode } {
	global $graph.$var
	set old [$graph postscript cget -$var]
	if { [catch {$graph postscript configure -$var [set $graph.$var]}] != 0 } {
	    $graph postscript configure -$var $old
	    set $graph.$var $old
	}
    }
    $graph postscript output "out.ps"
    puts stdout "wrote file \"out.ps\"."
    flush stdout
}
