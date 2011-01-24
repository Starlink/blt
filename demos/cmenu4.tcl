package require BLT

blt::tk::button .quit -command exit -text "Quit"
blt::tk::button .cancel -command exit -text "Cancel"
blt::tk::button .cont -command exit -text "Continue"

blt::combomenu .m  \
    -textvariable myText1  \
    -iconvariable myIcon1 \
    -background grey90 \
    -relief raised \
    -font "arial 9" 

set image1 [image create picture -file ~/neeshub/indeed/src/icons/sensor/hidden/user.png]
set image2 [image create picture -file ~/neeshub/indeed/src/icons/sensor/user.png]
set image3 [image create picture -width 20 -height 20]
$image3 copy $image1 -from "0 0 11 20"
$image3 copy $image2 -from "10 0 20 20" -to "10 0"
.m add -text "Hide" -accel "Ctrl-H" \
    -command "puts hide" -underline 0 -icon $image1
.m add -text "Show" -accel "Ctrl-S" \
    -command "puts show" -underline 0 -icon $image2
.m add -text "Toggle" -accel "Ctrl-T" \
    -command "puts toggle" -underline 0

proc Post { x y } {
    puts stderr "posting menu..."
    blt::ComboMenu::popup .m $x $y
}

bind all <ButtonPress-3> { Post %X %Y }


blt::table . \
    0,0 .cont -pady 10 \
    0,1 .quit -pady 10 \
    0,2 .cancel -pady 10 


