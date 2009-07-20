
set s1 [image create picture -width 25 -height 25]
$s1 blank 0x00000000
$s1 draw circle 12 12 5 -shadow 0 -linewidth 1 \
	-fill 0x90FF0000 -antialias yes 

set bg [image create picture -width 600 -height 600]
$bg blank white


set n 500000
puts stderr [time {
    blt::vector create points($n)
    
    points expr { round(random(points) * 512) + 30 }
}]
puts stderr [time {
    foreach {x y} [points print] {
	$bg copy $s1 -to "$x $y" -blend yes
	#$bg draw circle $x $y 30 -shadow 0 -linewidth 2 \
	    -fill 0x90FF0000 -antialias no
    }
}]
label .l -image $bg
pack .l

