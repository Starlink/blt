#!../src/bltwish

package require BLT
source scripts/demo.tcl

proc SortColumn { column } {
    set old [.t sort cget -column] 
    set decreasing 0
    if { "$old" == "$column" } {
	set decreasing [.t sort cget -decreasing]
	set decreasing [expr !$decreasing]
    }
    .t sort configure -decreasing $decreasing -column $column -mode integer
    if { ![.t cget -flat] } {
	.t configure -flat yes
    }
    .t sort auto yes

    blt::busy hold .t
    update
    blt::busy release .t
}

proc FormatSize { size } {
   set string ""
   while { $size > 0 } {
       set rem [expr $size % 1000]
       set size [expr $size / 1000]
       if { $size > 0 } {
           set rem [format "%03d" $rem]
       } 
       if { $string != "" } {
           set string "$rem,$string"
       } else {
           set string "$rem"
       }
   } 
   return $string
}

array set modes {
   0	---
   1    --x
   2    -w-
   3    -wx
   4    r-- 
   5    r-x
   6    rw-
   7    rwx
}

proc FormatMode { mode } {
   global modes

   set mode [format %o [expr $mode & 07777]]
   set owner $modes([string index $mode 0])
   set group $modes([string index $mode 1])
   set world $modes([string index $mode 2])

   return "${owner}${group}${world}"
}

proc Find { tree parent dir } {
    global count 
    set saved [pwd]

    cd $dir
    foreach f [glob -nocomplain *] {
	set name [file tail $f]
	if { [catch { file stat $f info }] != 0 } {
	    set node [$tree insert $parent -label $name]
	} else {
	    if 0 {
	    if { $info(type) == "file" } {
		set info(type) [list @::blt::tv::normalOpenFolder $name]
	    } else {
		set info(type) "@::blt::tv::normalOpenFolder "
	    }
	    }
	    set info(mtime) [clock format $info(mtime) -format "%b %d, %Y"]
	    set info(atime) [clock format $info(atime) -format "%b %d, %Y"]
	    set info(ctime) [clock format $info(ctime) -format "%b %d, %Y"]
            set info(size)  [FormatSize $info(size)]
	    set info(mode)  [FormatMode $info(mode)]
	    set node [$tree insert $parent -label $name -data [array get info]]
	}
	incr count
	if { [file type $f] == "directory" } {
	    Find $tree $node $f
	}
    }
    cd $saved
}


proc GetAbsolutePath { dir } {
    set saved [pwd]
    cd $dir
    set path [pwd] 
    cd $saved
    return $path
}

option add *TreeView.focusOutSelectForeground white
option add *TreeView.focusOutSelectBackground grey80
#option add *TreeView.Button.activeBackground pink
option add *TreeView.Button.activeBackground grey90
#option add *TreeView.Button.background grey95
option add *TreeView.Column.background grey90
option add *TreeView.CheckBoxStyle.activeBackground white
if 0 {
if 1 {
    option add *TreeView.Column.titleFont { Arial 10 }
    option add *TreeView.text.font { Monotype.com 10 Bold } 
    option add *TreeView.CheckBoxStyle.font Courier-10
    option add *TreeView.ComboBoxStyle.font Helvetica-10
    option add *TreeView.TextBoxStyle.font {Arial 10 bold }
} else {
    option add *TreeView.Column.titleFont { Arial 14 }
}
}
button .b -font { Helvetica 11 bold }
set top [GetAbsolutePath ..]
#set top [GetAbsolutePath /home/gah]
set trim "$top"

set tree [blt::tree create]    
blt::tk::scrollbar .vs -orient vertical -command { .t yview }
blt::tk::scrollbar .hs -orient horizontal -command { .t xview }

blt::treeview .t \
    -width 0 \
    -yscrollcommand { .vs set } \
    -xscrollcommand { .hs set } \
    -selectmode multiple \
    -separator / \
    -tree $tree 

.t column configure treeView -text ""  -edit yes
#file
.t column insert 0 mtime atime gid 
.t column insert end nlink mode type ctime uid ino size dev
.t column configure uid -background \#eaeaff -relief raised 
.t column configure mtime -hide no -bg \#ffeaea -relief raised
.t column configure size gid nlink uid ino dev -justify left -edit yes
.t column configure size type -justify left -edit yes
.t column configure treeView -hide no -edit yes \
	-icon ::blt::tv::normalOpenFolder
focus .t


blt::table . \
    0,0 .t  -fill both \
    0,1 .vs -fill y \
    1,0 .hs -fill x
blt::table configure . c1 r1 -resize none

set count 0
Find $tree root $top
puts "$count entries"

$tree find root -glob *.c -addtag "c_files"
$tree find root -glob *.h -addtag "header_files"
$tree find root -glob *.tcl -addtag "tcl_files"

.t entry configure "c_files" -foreground green4
.t entry configure "header_files" -foreground cyan4
.t entry configure "tcl_files" -foreground red4 

.t column bind all <ButtonRelease-3> {
    %W configure -flat no
}

foreach column [.t column names] {
    .t column configure $column -command [list SortColumn $column]
}

# Create a second treeview that shares the same tree.
if 0 {
toplevel .top
blt::treeview .top.t2 -tree $tree -yscrollcommand { .top.sbar set }
scrollbar .top.sbar -command { .top.t2 yview }
pack .top.t2 -side left -expand yes -fill both
pack .top.sbar -side right -fill y
}

#.t style configure text -background #F8fbF8 -selectbackground #D8fbD8 

.t style checkbox check \
    -onvalue 100 -offvalue "50" \
    -showvalue yes 

.t style combobox combo  \
	-icon ::blt::tv::normalOpenFolder

.t column configure uid -style combo 
.t column configure gid -style check


