*
* Smalltalkje, version 1 - Display and graphics support
* Written by Abdul Nabi, code krafters, January 2021
*
*  Display class for drawing
*
*  Display is meant to be a singleton... ususally accessed via
*  the device (e.g. gizmo display)
*
Class Display Object font
Class Point Object x y
Class Rectangle Object origin extent
Class Circle Object center radius
Class Color Object red green blue
Class Font Object value
*
Methods Display 'all'
	new
		^super new initialize
|
	initialize
		<153 0>.
|
	clear
		<153 2>.
|
	transfer
		<153 3>.
|
	backlightOn: aBoolean
		<153 1 aBoolean>
|
	string: aString x: x y: y
		<156 0 aString x y>.
		self transfer.
|
	string: aString at: aPoint
		self string: aString x: aPoint x y: aPoint y.
|
	setFont: aFont | fontIndex |
		fontIndex <- aFont asInteger.
		<156 20 fontIndex>
|
	set7Seg: l width: w outline: o
		<156 21 l w o>
|
	rectangleX: x y: y width: w height: h isFilled: aBoolean
		<157 x y w h aBoolean>.
		self transfer.
|
	rectangleAt: origin extent: extent isFilled: aBoolean
		self rectangleX: origin x 
			y: origin y 
			width: extent x 
			height: extent y 
			isFilled: aBoolean.
|
	rectangle: aRectangle isFilled: aBoolean
		self rectangleAt: aRectangle origin
			extent: aRectangle extent
			isFilled: aBoolean.
|
	circleX: x y: y radius: r isFilled: aBoolean
		<158 x y r aBoolean>.
		self transfer.
|
	circleAt: aPoint radius: r isFilled: aBoolean
		self circleX: aPoint x y: aPoint y radius: r isFilled: aBoolean.
|
	circle: aCircle isFilled: aBoolean
		self circleAt: aCircle center radius: aCircle radius isFilled: aBoolean.
]
Methods Rectangle 'all'
	origin: aPoint
		origin <- aPoint.
|
	extent: aPoint
		extent <- aPoint.
|
	origin: originPoint extent: extentPoint
		origin <- originPoint.
		extent <- extentPoint.
|
	origin
		^ origin
|
	extent
		^extent
|
	corner
		^ (origin x + extent x) @ (origin y + extent y)
|
	displayOn: aDisplay
		aDisplay rectangle: self isFilled: false.
]
Methods Circle 'all'
	center: aPoint
		center <- aPoint.
|
	radius: anInteger
		radius <- anInteger.
|
	center: aPoint radius: anInteger
		center <- aPoint.
		radius <- anInteger.
|
	center
		^center
|
	radius
		^radius
|
	displayOn: aDisplay
		aDisplay circle: self isFilled: false.
]
Methods Point 'all'
	x: xInteger
		x <- xInteger.
|
	y: yInteger
		y <- yInteger.
|
	x: xInteger y: yInteger
		self x: xInteger; y: yInteger.
|
	x
		^x
|
	y
		^y
]
Methods Integer 'all'
	@ y
		^ Point new x: self y: y
]
Methods Font 'all'
	new
		^super new default
|
	default
		value <- 0.
|
	tiny
		value <- 1.
|
	dejavu24
		value <- 2.
|
	ubuntu16
		value <- 3.
|
	comic24
		value <- 4.
|
	minya24
		value <- 5.
|
	tooney32
		value <- 6.
|
	small
		value <- 7.
|
	defautSmall
		value <- 8.
|
	sevenSeg
		value <- 9.
|
	user
		value <- 10.
|
	customEmbedded
		value <- 11.
|
	asInteger
		^ value
]
Methods Color 'all'
	new
		^super new black
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