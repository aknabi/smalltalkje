*
* Little Smalltalk, version 3 - Oled Display support
* Written by Abdul Nabi, code crafters, January 2021
*
*  M5StickC class representing the device...
*
*  Usually create a global m5 object with an instance
*
Class M5StickC Object display
Class Color Object red green blue
Class Font Object fontIndex
*
Methods M5StickC 'all'
	new
		^super new initialize
|
	initialize
		display <- Oled new.
|
    initializeLED
        <159 10 2>
|
    ledOn
        <160 10 0>
|
    ledOff
        <160 10 1>
|
	display
		^ display
|
    delayTaskFor: milliseconds
        <152 milliseconds>
|
    blinkLED
        self ledOff; ledOn; delayTaskFor: 1000; ledOff.
|
    when: anEventSymbol do: aBlock | eventNum process |
        eventNum <- self eventNumberFor: anEventSymbol.
        eventNum notNil
            ifTrue: [ 
                "Create the process to run when clicked. Wrap that in a block to reset the process when done"
                "buttonHandler <- self wrapHandlerBlock: aBlock event: eventNum."
                "buttonHandler <- aBlock newProcess."
                <181 1 eventNum aBlock> ]
|
    event: aSymbol
        [aSymbol = #BigButtonClicked]
            ifTrue: ['event big button click' print].
        ^self asString print
|
    wrapHandlerBlock: aBlock event: eventNum
        ^[ aBlock fork. buttonHandler <- self wrapHandlerBlock: aBlock event: eventNum. <181 1 eventNum buttonHandler> ] newProcess
|
    eventNumberFor: anEventSymbol
        (Switch new key: anEventSymbol)
            ifMatch: #BigButtonClicked do: [^1];
            ifMatch: #BigButtonLongPressed do: [^2];
            ifMatch: #LittleButtonClicked do: [^3];
            ifMatch: #LittleButtonLongPressed do: [^4];
            else: [^nil]
]
Methods String 'execute'
    evalWith: arg
        <20 self>
]
Methods Color 'all'
	new
		^super new black
|
	initialize
		display <- Oled new.
|
	red: redValue green: greenValue blue: blueValue
		red <- redValue.
        green <- greenValue.
        blue <- blueValue.
|
	black
		^ self red: 0 green: 0 blue: 0
|
	navy
		^ self red: 0 green: 0 blue: 128
|
	darkGreen
		^ self red: 0 green: 128 blue: 0
|
	darkCyan
		^ self red: 0 green: 128 blue: 128
|
	maroon
		^ self red: 128 green: 0 blue: 0
|
	purple
		^ self red: 128 green: 0 blue: 128
|
	olive
		^ self red: 128 green: 128 blue: 0
|
	lightGray
		^ self red: 192 green: 192 blue: 192
|
	gray
		^ self red: 128 green: 128 blue: 128
|
	darkGray
		^ self red: 64 green: 64 blue: 64
|
	blue
		^ self red: 0 green: 0 blue: 255
|
	green
		^ self red: 0 green: 255 blue: 0
|
	cyan
		^ self red: 0 green: 255 blue: 255
|
	red
		^ self red: 255 green: 0 blue: 0
|
	magenta
		^ self red: 255 green: 0 blue: 255
|
	yellow
		^ self red: 252 green: 255 blue: 0
|
	white
		^ self red: 255 green: 255 blue: 255
|
	orange
		^ self red: 255 green: 164 blue: 255
|
	puke
		^ self red: 172 green: 252 blue: 44
|
	pink
		^ self red: 255 green: 192 blue: 202
]
Methods Font 'all'
	new
		^super new default
|
	default
		fontIndex = 0.
|
	tiny
		fontIndex = 1.
|
	dejavu24
		fontIndex = 2.
|
	ubuntu16
		fontIndex = 3.
|
	comic24
		fontIndex = 4.
|
	minya24
		fontIndex = 5.
|
	tooney32
		fontIndex = 6.
|
	small
		fontIndex = 7.
|
	defautSmall
		fontIndex = 8.
|
	sevenSeg
		fontIndex = 9.
|
	user
		fontIndex = 10.
|
	customEmbedded
		fontIndex = 11.
]
