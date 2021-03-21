*
* gizmo.st
* 
* Smalltalkje, version 1 - Gizmo support
* Written by Abdul Nabi, code crafters, January 2021
*
*  Gizmo class representing the device...
*
*  Usually create a global gizmo object with an instance
*
Class Gizmo Object display
Class HttpRequest Object url method contentType body
Class HttpResponse Object statusCode contentLength content
Class GizmoTerm Object y input i
*
Methods Gizmo 'all'
	new
		display <- Display new.
        EventHandlerBlocks <- Dictionary new.
|
    setupButtonHandlers
		self deviceName = 'T-WRBD' ifTrue: [
			DisplayUpdateBlock <- [ 
				display backlightOn: true. 
				self updateDisplayTime; updateDisplayWeather. 
				[display backlightOn: false] runAfter: 5000 ].
        	self when: 'BigButtonClicked' do: DisplayUpdateBlock ].
		self deviceName = 'M5StickC' ifTrue: [
			DisplayUpdateBlock <- [ display clear; backlightOn: true. self updateDisplayTime; updateDisplayWeather. [display clear; backlightOn: false] runAfter: 2000 ].
        	self when: 'BigButtonClicked' do: DisplayUpdateBlock.
        	self when: 'BigButtonHeld' do: [display clear; string: 'BigButtonHeld' x: 16 y: 16].
        	self when: 'LittleButtonClicked' do: [display clear; string: 'LittleButtonClicked' x: 16 y: 16].
        	self when: 'LittleButtonHeld' do: [display clear; string: 'LittleButtonHeld' x: 16 y: 16] ]
|
	setupKeyboardHandlers
		self deviceName = 'M5StickC' ifTrue: [
			KeyboardSemaphore <- Semaphore new.
			gizmoTerm <- GizmoTerm new initialize.
			self when: 'KeyboardChar' do: [:c |  KeyboardChar <- c. gizmoTerm addCharInt: c ].
			"[ [true] whileTrue: [KeyboardSemaphore wait. KeyboardChar printAscii ] ] fork."
			gizmo runKBTask ]
|
	setupWifiHandlers
	    self when: 'WifiConnected' do: [ 
			display clear; string: 'Wifi Connected' x: 16 y: 16.
			[ display clear. self setupDate; setupWeather. DisplayUpdateBlock value] runAfter: 100 ]
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
	wifiOn
		<170 0>
|
	wifiConnect
		<170 2>
|
	updateDisplayWeather
		weather updateSync.
		display string: 'Temp:' x: 16 y: 40; string: weather temp x: 60 y: 40; string: weather desc x: 16 y: 54.
|
	setupWeather
		city <- self nvsStringAtKey: 'weatherCity'.
		city isNil ifTrue: [city <- 'Amsterdam'].
		weather <- Weather new city: city.
		"weatherUpdateBlock <- [self weatherUpdateLoop]."
		"weatherUpdateBlock value"
|
	weatherUpdateLoop | s |
		self updateDisplayWeather.
		weatherUpdateBlock runAfter: 10000
|
	wifiAutoconnect | ssids savedSSID savedPass found |
		found <- false.
		savedSSID <- self nvsStringAtKey: 'ssid'.
		savedSSID notNil ifTrue: [
			ssids <- self wifiScan.
			(found <- ssids includes: savedSSID)
				ifTrue: [
					savedPass <- gizmo nvsStringAtKey: 'password'.
					self wifiSSID: savedSSID password: savedPass.
					self wifiConnect ]
				ifFalse: ['Saved ssid not found' print] ].
		^ found
|
	wifiScan
		^ <170 3>
|
	wifiSSID: ssid password: password
		<170 1 ssid password>
]
*
Methods Gizmo 'NVS'
	nvsAt: key put: object
		^ <182 1 key object>
|
	nvsStringAtKey: key
		^ <182 2 key String>
|
	nvsRemoveKey: key
		^ <182 3 key>
]
*
Methods Gizmo 'configuration'
	skipObjectDataOnBoot: aBoolean
		aBoolean
			ifTrue: [self nvsAt: '_skipODP' put: 100]
			ifFalse: [self nvsRemoveKey: '_skipODP']
]
*
Methods Gizmo 'gpio-i2c'
	i2cByteAt: i2cAddress
		^ <170 20 i2cAddress>
|
	i2cInterruptAt: i2cAddress
		^ <170 21 i2cAddress>
|
	gpioAt: pin put: anInteger
		<160 pin anInteger>
|
    initializeLED
        <159 10 2>
|
    ledOn
        self gpioAt: 10 put: 0
|
    ledOff
        self gpioAt: 10 put: 1
]
*
Methods Gizmo 'keyboard'
	runKBTask
		^ <170 22>
|
	getCardKB | c |
		c <- 0.
		"Need the = test as noKB could be nil first time"
		(noKB = true)
			ifTrue: [ [ noKb <- false ] runAfter: 1000 ]
			ifFalse: [ c <- self getCardKBChar ].
		^ c
|
	getCardKBChar | c |
		c <- self i2cByteAt: 95.
		"If an error then the keyboard is likely disconnected so don't read for a while"
		c isError 
			ifTrue: [ noKB <- true. c <- 0 ] 
			ifFalse: [
				c > 127
					ifTrue: [c <- 0]
					ifFalse: [ c = 13 ifTrue: [ c <- 10 ] ] ].
		^ c
|
	freeHeap
		^ <170 100>
|
	deviceName
		^ <200 0>
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
*
Methods String 'execute'
    evalWith: arg
        <20 self>
]
*
Methods GizmoTerm 'all'
	initialize
		i <- 1.
		y <- 4.
		input <- ByteArray new: 64.
|
	newLine | res |
		y <- y + 12.
		res <- input deepCopy asString value printString. 
		gizmo display string: res x: 4 y: y.
		y <- y + 12.
		i <- 1.
		input <- ByteArray new: 64.
		y > 70 ifTrue: [[gizmo display clear. y <- 4] runAfter: 1000]
|
	addCharInt: c
		gizmo display backlightOn: true.
		(c = 8 and: [i > 1]) 
			ifTrue: [
				input at: i put: 0.
				i <- i - 1. 
				input at: i put: 13 ]
			ifFalse: [
				(c = 13 or: [c = 10])
					ifTrue: [ scheduler critical: [self newLine]. ^ self ]
					ifFalse: [
						input at: i put: c.
						i <- i + 1.
						input at: i put: 0 ] ].
		gizmo display string: input deepCopy asString x: 4 y: y
]