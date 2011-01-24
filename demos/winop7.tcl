#!../src/bltwish

package require BLT

set imgfile ~/rc_logo_sm.gif

set im1 [image create picture -width 50 -height 50]
$im1 blank 0x00000000
set x 6
set y 30
$im1 draw text $x $y \
    -text "048" \
    -font "arial 14 bold" \
    -color 0xFF000000
$im1 blur $im1 2
set im2 [image create picture]
$im2 select $im1 0x01000000 0xFFFFFFFF
$im1 and 0xFF000000 -mask $im2
set im [image create picture -file $imgfile -width 50 -height 50 -filter sinc]
$im blend $im $im1
incr x -1
incr y -1 
$im draw text $x $y \
    -text "048" \
    -font "arial 14 bold" \
  -color white
label .show -image $im
pack .show 
