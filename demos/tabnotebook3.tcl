#!../src/bltwish

package require BLT
source scripts/demo.tcl
#blt::bltdebug 100

image create picture label1 -file checkon.png
image create picture label2 -file ./images/mini-book2.gif
image create picture testImage -file ./images/txtrflag.gif

blt::tabset .ts \
    -scrollcommand { .s set } \
    -scrollincrement 1 

label .ts.l -image testImage

set attributes {
    "Graph \#1" pink	
    "Graph \#2" lightblue	
    "Graph \#3" orange
    "Graph \#5" yellow	
    "Barchart \#2" green
}

foreach { label color } $attributes {
    .ts insert end $label \
	-selectbackground ${color}3  \
	-background ${color}3 \
	-activebackground ${color}2
}

.ts insert end -selectbackground salmon2 -background salmon3 \
    -selectbackground salmon3 -activebackground salmon2 -window .ts.l

set tabLabels { 
    Aarhus Aaron Ababa aback abaft abandon abandoned abandoning
    abandonment abandons abase abased abasement abasements abases
    abash abashed abashes abashing abasing abate abated abatement
    abatements abater abates abating Abba abbe abbey abbeys abbot
    abbots Abbott abbreviate abbreviated abbreviates abbreviating
    abbreviation abbreviations Abby abdomen abdomens abdominal
    abduct abducted abduction abductions abductor abductors abducts
    Abe abed Abel Abelian Abelson Aberdeen Abernathy aberrant
    aberration aberrations abet abets abetted abetter abetting
    abeyance abhor abhorred abhorrent abhorrer abhorring abhors
    abide abided abides abiding Abidjan Abigail Abilene abilities
    ability abject abjection abjections abjectly abjectness abjure
    abjured abjures abjuring ablate ablated ablates ablating
    ablation ablative ablaze able abler ablest ably Abner abnormal
    abnormalities abnormality abnormally Abo aboard abode abodes
    abolish abolished abolisher abolishers abolishes abolishing
    abolishment abolishments abolition abolitionist abolitionists
    abominable abominate aboriginal aborigine aborigines abort
    aborted aborting abortion abortions abortive abortively aborts
    Abos abound abounded abounding abounds about above aboveboard
    aboveground abovementioned abrade abraded abrades abrading
    Abraham Abram Abrams Abramson abrasion abrasions abrasive
    abreaction abreactions abreast abridge abridged abridges
    abridging abridgment abroad abrogate abrogated abrogates
    abrogating abrupt abruptly abruptness abscess abscessed
    abscesses abscissa abscissas abscond absconded absconding
    absconds absence absences absent absented absentee
    absenteeism absentees absentia absenting absently absentminded
    absents absinthe absolute absolutely absoluteness absolutes
    absolution absolve absolved absolves absolving absorb
    absorbed absorbency absorbent absorber absorbing absorbs
    absorption absorptions absorptive abstain abstained abstainer
    abstaining abstains abstention abstentions abstinence
    abstract abstracted abstracting abstraction abstractionism
    abstractionist abstractions abstractly abstractness
    abstractor abstractors abstracts abstruse abstruseness
    absurd absurdities absurdity absurdly Abu abundance abundant
    abundantly abuse abused abuses abusing abusive abut abutment
    abuts abutted abutter abutters abutting abysmal abysmally
    abyss abysses Abyssinia Abyssinian Abyssinians acacia
    academia academic academically academics academies academy
    Acadia Acapulco accede acceded accedes accelerate accelerated
    accelerates accelerating acceleration accelerations
    accelerator accelerators accelerometer accelerometers accent
    accented accenting accents accentual accentuate accentuated
    accentuates accentuating accentuation accept acceptability
    acceptable acceptably acceptance acceptances accepted
    accepter accepters accepting acceptor acceptors accepts
    access accessed accesses accessibility accessible accessibly
    accessing accession accessions accessories accessors
    accessory accident accidental accidentally accidently
    accidents acclaim acclaimed acclaiming acclaims acclamation
    acclimate acclimated acclimates acclimating acclimatization
    acclimatized accolade accolades accommodate accommodated
    accommodates accommodating accommodation accommodations
    accompanied accompanies accompaniment accompaniments
    accompanist accompanists accompany accompanying accomplice
    accomplices accomplish accomplished accomplisher accomplishers
    accomplishes accomplishing accomplishment accomplishments
    accord accordance accorded accorder accorders according
    accordingly accordion accordions accords accost accosted
    accosting accosts account accountability accountable accountably
    accountancy accountant accountants accounted accounting
    accounts Accra accredit accreditation accreditations
    accredited accretion accretions accrue accrued accrues
    accruing acculturate acculturated acculturates acculturating
    acculturation accumulate accumulated accumulates accumulating
    accumulation accumulations accumulator accumulators
    accuracies accuracy accurate accurately accurateness accursed
    accusal accusation accusations accusative accuse accused
    accuser accuses accusing accusingly accustom accustomed
    accustoming accustoms ace aces acetate acetone acetylene
    Achaean Achaeans ache ached aches achievable achieve achieved
    achievement achievements achiever achievers achieves achieving
    Achilles aching acid acidic acidities acidity acidly acids
    acidulous Ackerman Ackley acknowledge acknowledgeable
    acknowledged acknowledgement acknowledgements acknowledger
    acknowledgers acknowledges acknowledging acknowledgment
    acknowledgments acme acne acolyte acolytes acorn acorns
    acoustic acoustical acoustically acoustician acoustics
    acquaint acquaintance acquaintances acquainted acquainting
    acquaints acquiesce acquiesced acquiescence acquiescent
    acquiesces acquiescing acquirable acquire acquired acquires
    acquiring acquisition acquisitions
}

foreach label $tabLabels {
    .ts insert end $label -image label1 
}

blt::tk::scrollbar .s -command { .ts view } -orient horizontal
blt::combobutton .side -text "-side" -menu .side.m
blt::combomenu .side.m 
.side.m add -type radiobutton -text "Top" -variable side -value "top" \
    -command { .ts configure -side $side -rotate 0 }
.side.m add -type radiobutton -text "Bottom" -variable side -value "bottom" \
    -command { .ts configure -side $side -rotate 0 }
.side.m add -type radiobutton -text "Left" -variable side -value "left" \
    -command { .ts configure -side $side -rotate 90 }
.side.m add -type radiobutton -text "Right" -variable side -value "right" \
    -command { .ts configure -side $side -rotate 270 }

blt::combobutton .txside -text "-textside" -menu .txside.m 
blt::combomenu .txside.m 
.txside.m add -type radiobutton -text "Right" -variable textside \
    -value "right" -command { .ts configure -textside $textside }
.txside.m add -type radiobutton -text "Left" -variable textside -value "left" \
    -command { .ts configure -textside $textside }
.txside.m add -type radiobutton -text "Top" -variable textside -value "top" \
    -command { .ts configure -textside $textside }
.txside.m add -type radiobutton -text "Bottom" -variable textside \
    -value "bottom" -command { .ts configure -textside $textside }

blt::combobutton .slant -text "-slant" -menu .slant.m 
blt::combomenu .slant.m 
.slant.m add -type radiobutton -text "None" -variable slant \
    -value "none" -command { .ts configure -slant $slant }
.slant.m add -type radiobutton -text "Left" -variable slant -value "left" \
    -command { .ts configure -slant $slant }
.slant.m add -type radiobutton -text "Right" -variable slant \
    -value "right" -command { .ts configure -slant $slant }
.slant.m add -type radiobutton -text "Both" -variable slant -value "both" \
    -command { .ts configure -slant $slant }

blt::combobutton .rotate -text "-rotate" -menu .rotate.m 
blt::combomenu .rotate.m 
.rotate.m add -type radiobutton -text "0" -variable rotate \
    -value "0.0" -command { .ts configure -rotate $rotate }
.rotate.m add -type radiobutton -text "90" -variable rotate \
    -value "90.0" -command { .ts configure -rotate $rotate }
.rotate.m add -type radiobutton -text "180" -variable rotate \
    -value "180.0" -command { .ts configure -rotate $rotate }
.rotate.m add -type radiobutton -text "270" -variable rotate \
    -value "270.0" -command { .ts configure -rotate $rotate }
.rotate.m add -type radiobutton -text "30" -variable rotate \
    -value "30.0" -command { .ts configure -rotate $rotate }

blt::combobutton .tiers -text "-tiers" -menu .tiers.m 
blt::combomenu .tiers.m 
.tiers.m add -type radiobutton -text "1" -variable tiers \
    -value "1" -command { .ts configure -tiers $tiers }
.tiers.m add -type radiobutton -text "2" -variable tiers -value "2" \
    -command { .ts configure -tiers $tiers }
.tiers.m add -type radiobutton -text "3" -variable tiers \
    -value "3" -command { .ts configure -tiers $tiers }
.tiers.m add -type radiobutton -text "4" -variable tiers \
    -value "4" -command { .ts configure -tiers $tiers }
.tiers.m add -type radiobutton -text "5" -variable tiers \
    -value "5" -command { .ts configure -tiers $tiers }
.tiers.m add -type radiobutton -text "10" -variable tiers \
    -value "10" -command { .ts configure -tiers $tiers }

blt::combobutton .tabwidth -text "-tabwidth" -menu .tabwidth.m 
blt::combomenu .tabwidth.m 
.tabwidth.m add -type radiobutton -text "Variable" -variable tabwidth \
    -value "variable" -command { .ts configure -tabwidth $tabwidth }
.tabwidth.m add -type radiobutton -text "Same" -variable tabwidth \
    -value "same" -command { .ts configure -tabwidth $tabwidth }
.tabwidth.m add -type radiobutton -text ".5 inch" -variable tabwidth \
    -value "0.5i" -command { .ts configure -tabwidth $tabwidth }
.tabwidth.m add -type radiobutton -text "1 inch" -variable tabwidth \
    -value "1i" -command { .ts configure -tabwidth $tabwidth }
.tabwidth.m add -type radiobutton -text "2 inch" -variable tabwidth \
    -value "2i" -command { .ts configure -tabwidth $tabwidth }

blt::combobutton .justify -text "-justify" -menu .justify.m 
blt::combomenu .justify.m 
.justify.m add -type radiobutton -text "Left" -variable justify \
    -value "left" -command { .ts configure -justify $justify }
.justify.m add -type radiobutton -text "Center" -variable justify \
    -value "center" -command { .ts configure -justify $justify }
.justify.m add -type radiobutton -text "Right" -variable justify \
    -value "right" -command { .ts configure -justify $justify }

blt::combobutton .tearoff -text "-tearoff" -menu .tearoff.m 
blt::combomenu .tearoff.m 
.tearoff.m add -type radiobutton -text "No" -variable tearoff \
    -value "0" -command { .ts configure -tearoff $tearoff }
.tearoff.m add -type radiobutton -text "Yes" -variable tearoff \
    -value "1" -command { .ts configure -tearoff $tearoff }

blt::combobutton .highlightthickness -text "-highlightthickness" \
    -menu .highlightthickness.m 
blt::combomenu .highlightthickness.m 
.highlightthickness.m add -type radiobutton -text "0" \
    -variable highlightthickness -value "0" \
    -command { .ts configure -highlightthickness $highlightthickness }
.highlightthickness.m add -type radiobutton -text "1" \
    -variable highlightthickness -value "1" \
    -command { .ts configure -highlightthickness $highlightthickness }
.highlightthickness.m add -type radiobutton -text "2" \
    -variable highlightthickness -value "2" \
    -command { .ts configure -highlightthickness $highlightthickness }
.highlightthickness.m add -type radiobutton -text "3" \
    -variable highlightthickness -value "3" \
    -command { .ts configure -highlightthickness $highlightthickness }
.highlightthickness.m add -type radiobutton -text "10" \
    -variable highlightthickness -value "10" \
    -command { .ts configure -highlightthickness $highlightthickness }

blt::combobutton .outerpad -text "-outerpad" \
    -menu .outerpad.m 
blt::combomenu .outerpad.m 
.outerpad.m add -type radiobutton -text "0" \
    -variable outerpad -value "0" \
    -command { .ts configure -outerpad $outerpad }
.outerpad.m add -type radiobutton -text "1" \
    -variable outerpad -value "1" \
    -command { .ts configure -outerpad $outerpad }
.outerpad.m add -type radiobutton -text "2" \
    -variable outerpad -value "2" \
    -command { .ts configure -outerpad $outerpad }
.outerpad.m add -type radiobutton -text "3" \
    -variable outerpad -value "3" \
    -command { .ts configure -outerpad $outerpad }
.outerpad.m add -type radiobutton -text "10" \
    -variable outerpad -value "10" \
    -command { .ts configure -outerpad $outerpad }

blt::combobutton .borderwidth -text "-borderwidth" \
    -menu .borderwidth.m 
blt::combomenu .borderwidth.m 
.borderwidth.m add -type radiobutton -text "0" \
    -variable borderwidth -value "0" \
    -command { .ts configure -borderwidth $borderwidth }
.borderwidth.m add -type radiobutton -text "1" \
    -variable borderwidth -value "1" \
    -command { .ts configure -borderwidth $borderwidth }
.borderwidth.m add -type radiobutton -text "2" \
    -variable borderwidth -value "2" \
    -command { .ts configure -borderwidth $borderwidth }
.borderwidth.m add -type radiobutton -text "3" \
    -variable borderwidth -value "3" \
    -command { .ts configure -borderwidth $borderwidth }
.borderwidth.m add -type radiobutton -text "10" \
    -variable borderwidth -value "10" \
    -command { .ts configure -borderwidth $borderwidth }

blt::combobutton .gap -text "-gap" \
    -menu .gap.m 
blt::combomenu .gap.m 
.gap.m add -type radiobutton -text "0" \
    -variable gap -value "0" \
    -command { .ts configure -gap $gap }
.gap.m add -type radiobutton -text "1" \
    -variable gap -value "1" \
    -command { .ts configure -gap $gap }
.gap.m add -type radiobutton -text "2" \
    -variable gap -value "2" \
    -command { .ts configure -gap $gap }
.gap.m add -type radiobutton -text "3" \
    -variable gap -value "3" \
    -command { .ts configure -gap $gap }
.gap.m add -type radiobutton -text "10" \
    -variable gap -value "10" \
    -command { .ts configure -gap $gap }

blt::combobutton .relief -text "-relief" \
    -menu .relief.m 
blt::combomenu .relief.m 
.relief.m add -type radiobutton -text "flat" \
    -variable relief -value "flat" \
    -command { .ts configure -relief $relief }
.relief.m add -type radiobutton -text "sunken" \
    -variable relief -value "sunken" \
    -command { .ts configure -relief $relief }
.relief.m add -type radiobutton -text "raised" \
    -variable relief -value "raised" \
    -command { .ts configure -relief $relief }
.relief.m add -type radiobutton -text "groove" \
    -variable relief -value "groove" \
    -command { .ts configure -relief $relief }
.relief.m add -type radiobutton -text "ridge" \
    -variable relief -value "ridge" \
    -command { .ts configure -relief $relief }
.relief.m add -type radiobutton -text "solid" \
    -variable relief -value "solid" \
    -command { .ts configure -relief $relief }

blt::table . \
    .ts 0,0 -fill both -rspan 14 \
    .borderwidth 0,1 -anchor w \
    .gap 1,1 -anchor w \
    .highlightthickness 2,1 -anchor w \
    .justify 3,1 -anchor w \
    .outerpad 4,1 -anchor w \
    .relief 5,1 -anchor w \
    .rotate 6,1 -anchor w \
    .side 7,1 -anchor w \
    .slant 8,1 -anchor w \
    .tabwidth 9,1 -anchor w \
    .tearoff 10,1 -anchor w \
    .tiers 11,1 -anchor w \
    .txside 12,1 -anchor w \
    .s 14,0 -fill x \

foreach option { 
    borderwidth gap highlightthickness justify outerpad relief rotate 
    side slant tabwidth tearoff tiers textside
} {
    set $option [.ts cget -$option]
}

puts stderr angle=[.ts cget -rotate]

blt::table configure . r* c1 -resize none
blt::table configure . r13 -resize expand
focus .ts

.ts focus 0

if 1 {
set filecount 0
foreach file { graph1 graph2 graph3 graph5 barchart2 } {
    namespace eval $file {
	if { [string match graph* $file] } {
	    set graph [blt::graph .ts.$file]
	} else {
	    set graph [blt::barchart .ts.$file]
	}
	source scripts/$file.tcl
	.ts tab configure $filecount -window $graph -fill both 
	incr filecount
    }
}
}


.ts select 0
.ts activate 0
.ts focus 0
after 5000 {
    .ts tab configure 0 -state disabled
}
