package require BLT
package require Tk
set paned [panedwindow .panedwindow]

set a [frame $paned.a -bg red]
set tree [::blt::treeview $a.tree\
         -relief flat\
         -selectmode single\
         -exportselection false]
$tree configure -yscrollcommand "$a.y set" -xscrollcommand "$a.x set"
scrollbar $a.y -command "$tree yview"
scrollbar $a.x -command "$tree xview" -orient horizontal
grid $tree -row 0 -column 1 -sticky nsew
grid $a.y -row 0 -column 2 -sticky ns
grid $a.x -row 1 -column 1 -sticky we
grid columnconfigure $a 1 -weight 1
grid rowconfigure $a 0 -weight 1
$tree column configure treeView -width 0
$tree insert end hola -label hola
pack $paned -fill both -expand true

$tree configure -width 0
set m [frame $paned.m -bg yellow]
$paned add $a
$paned add $m

update
::update idletasks
$paned sash place 0 180 1
puts [$tree configure -width]
puts [winfo geometry $tree]
puts [$paned configure]
