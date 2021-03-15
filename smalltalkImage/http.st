*
* http.st
* 
* Smalltalkje, version 1 - HTTP Support
* Written by Abdul Nabi, code crafters, March 2021
*
Class HttpRequest Object url method contentType body
Class HttpResponse Object statusCode contentLength content
Class Weather Object city temp feelsLike minTemp maxTemp wind desc
*
Methods HttpRequest 'all'
	getUrl: urlString
		url <- urlString.
		method <- 0
|
	postUrl: urlString contentType: typeString data: postData
		"If contentType is nil, then type will be assumed JSON"
		url <- urlString.
		method <- 1.
		contentType <- typeString.
		body <- postData
|
	send
		"Send the request and wait for the HttpResponse which will be returned"
		^ <183 0 self>
|
	sendWhenDone: aBlock
		"Do the request on a seperate thread and call aBlock when done with the HttpResponse"
		^ <183 1 self aBlock>
|
	demoGet
		"After this is set up 'H sendWhenDone: B' will run a get and save response in global R"
		url <- 'http://httpbin.org/get'.
		method <- 0.
		B <- [:r | R <- r. '=== HTTP GET DEMO DONE ===' print ].
		H <- self
|
	getWeather: urlEncodedCity
		(url <- self getWeatherUrl) isNil ifTrue: [^ nil].
		method <- 0.
		B <- [:r | R <- r. '=== HTTP POST DEMO DONE content size: ', r contentLength print ].
		H <- self
|
	getWeatherUrl | appId |
		appId <- gizmo nvsStringAtKey: 'weatherAppId'.
		appId isNil ifTrue: [^ nil].
		^ 'http://api.openweathermap.org/data/2.5/weather?units=metric&q=', city, '&appid=', appId
|
	demoPost
		"After this is set up 'H sendWhenDone: B' will run a get and save response in global R"
		url <- 'http://httpbin.org/post'.
		method <- 1.
		contentType <- nil.
		body <- '{ "firstName" : "Abdul", "lastName" : "Nabi" }'.
		B <- [:r | R <- r. '=== HTTP POST DEMO DONE ===' print ].
		H <- self
]
Methods HttpResponse 'all'
	statusCode
		^ statusCode
|
	contentLength
		^ contentLength
|
	content
		^ content
]
Methods Weather 'all'
	city
		^ city
|
	city: aString
		city <- aString
|
	temp
		^ temp
|
	feelsLike
		^ feelsLike
|
	minTemp
		^ minTemp
|
	maxTemp
		^ maxTemp
|
	wind
		^ wind
|
	desc
		^ desc
|
	getRequest
		^ HttpRequest new getUrl: 'http://thenabi.com/smalltalkje/weather/current.php?secret=stjerules!&city=', city
|
	update | req a |
		req <- self getRequest.
		req sendWhenDone: [:r | (r isNil or: [(r statusCode = 200) not]) ifFalse: [
			a <- r content asWords: $,.
			self loadArray: a ] ]
|
	updateSync | req r a |
		req <- self getRequest.
		r <- req send.
		(r isNil or: [(r statusCode = 200) not]) ifFalse: [
			a <- r content asWords: $,.
			self loadArray: a ]
|
	loadArray: anArray
		temp <- anArray at: 1. 
		feelsLike <- anArray at: 2.
		minTemp <- anArray at: 3.
		maxTemp <- anArray at: 4.
		wind <- anArray at: 5.
		desc <- anArray at: 6
]
