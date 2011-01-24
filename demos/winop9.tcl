#!../src/bltwish

package require BLT
source scripts/demo.tcl

#    -fontfile /usr/share/fonts/100dpi/helvR18.pcf.gz 
set img [image create picture -width 500 -height 500]  
$img draw rectangle 100 100 111 111 -color blue 
$img draw text "This is a test\nzxcVAbnm<//gqy" 100 100 \
    -rotate 0 \
    -kerning 1 \
    -font { "arial" 24 bold } \
    -color 0x7FFFFFFF

label .l -image $img
blt::table . \
   0,0 .l


