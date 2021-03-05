*
* http.st
* 
* Smalltalkje, version 1 - HTTP Support
* Written by Abdul Nabi, code crafters, March 2021
*
Class HttpRequest Object url method contentType body
Class HttpResponse Object statusCode contentLength content
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
