*
* Little Smalltalk, version 3 - Oled Display support
* Written by Abdul Nabi, code crafters, January 2021
*
*  Oled class for drawing to a SSD1306 OLED display
*
*  Usually create a global display object with an instance
*
Class Oled Object font
Class Point Object x y
Class Rectangle Object origin extent
Class Circle Object center radius
*
Methods Oled 'all'
	new
		^super new initialize
|
	initialize
		<153>.
|
	clear
		<154>.
|
	transfer
		<155>.
|
	string: aString x: x y: y
		<156 aString x y>.
		self transfer.
|
	string: aString at: aPoint
		self string: aString x: aPoint x y: aPoint y.
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
	displayOn: anOled
		anOled rectangle: self isFilled: false.
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
	displayOn: anOled
		anOled circle: self isFilled: false.
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