*
* m5StickC.st
* 
* Little Smalltalk, version 3 - M5StickC, Oled Display support
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
		display <- Oled new.
        EventHandlerBlocks <- Dictionary new.
|
    setupButtonHandlers
        "self when: 'BigButtonClicked' do: [display clear; string: 'BigButtonClicked' x: 16 y: 16]."
        "self when: 'BigButtonClicked' do: [(1 to: 10) do: [:i | display string: 'Big Click ', i asString x: 16 y: 16. m5 delayTaskFor: 200]]."
        self when: 'BigButtonClicked' do: [ display clear; string: 'BigButtonClicked' x: 16 y: 16. [display clear] runAfter: 2000 ].
        self when: 'BigButtonHeld' do: [display clear; string: 'BigButtonHeld' x: 16 y: 16].
        self when: 'LittleButtonClicked' do: [display clear; string: 'LittleButtonClicked' x: 16 y: 16].
        self when: 'LittleButtonHeld' do: [display clear; string: 'LittleButtonHeld' x: 16 y: 16].
|
	setupWifiHandlers
	    self when: 'WifiConnected' do: [ display clear; string: 'Wifi Connected' x: 16 y: 16. [display clear] runAfter: 2000 ].
|
    displayLoop
		(1 to: 10) do: [:x | self afterDelay: 1000 do: [display clear; string: 'Big Click ', x asString x: 16 y: 16]].
|
    backgroundLoop
		10 timesRepeat: [  self afterDelay: 500 do: [self ledOn].
			self afterDelay: 500 do: [self ledOff] ].
|
    display: aString atY: yPos delay: milliseconds
        display clear; string: aString x: 16 y: yPos.
        self delayTaskFor: milliseconds.
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
	wifiOn
		<170 0>
|
	wifiSSID: ssid password: password
		<170 1 ssid password>
|
	nvsAt: key put: object
		^ <182 1 key object>
|
	nvsStringAtKey: key
		^ <182 2 key String>
|
	nvsRemoveKey: key
		^ <182 3 key>
|
	freeHeap
		^ <170 100>
|
	display
		^ display
|
    delayTaskFor: milliseconds
        <152 nil milliseconds>
|
    afterDelay: milliseconds do: aBlock 
        <152 aBlock milliseconds>
|
    blinkLED
        self ledOn. [self ledOff] runAfter: 1000
|
	blinkAndRun: aBlock
	    self ledOn. [self ledOff] runAfter: 1000
|
	blinkTimes: numberOfBlinks
		self blinkTimes: numberOfBlinks count: 0 period: 250
|
	blinkTimes: numberOfBlinks period: milliseconds
		self blinkTimes: numberOfBlinks count: 0 period: milliseconds
|
	blinkTimes: numberOfBlinks count: c period: milliseconds | i |
		i <- c + 1. 
		i < numberOfBlinks 
		ifTrue: [ 
			[ self ledOn. 
				[self ledOff. 
					self blinkTimes: numberOfBlinks count: i period: milliseconds] 
						runAfter: milliseconds ] runAfter: 10 ]
|
    when: anEventString do: aBlock
        EventHandlerBlocks at: anEventString put: aBlock
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
