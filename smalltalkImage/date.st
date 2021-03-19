*
* Date.st
* 
* Smalltalkje, version 1 - Date and Time Support
* Written by Abdul Nabi, code crafters, March 2021
Class Date Object epochSeconds
*
Methods Date 'all'
	new
		^super new now
|
	now
		epochSeconds <- <170 55>
|
    epochSeconds
        ^ epochSeconds
|
    epochSeconds: aNumber
        epochSeconds <- aNumber asFloat
|
    day: day month: month year: year
        "Set new values for the receiver day, month, year"
        epochSeconds <- <170 58 epochSeconds day month year>
|
    hour: hour minutes: minutes seconds: seconds
        "Set new values for the receiver day, month, year"
        epochSeconds <- <170 59 epochSeconds hour minutes seconds>
|
    seconds
        ^ <170 56 epochSeconds 1>
|
    minutes
        ^ <170 56 epochSeconds 2>
|
    hour
        ^ <170 56 epochSeconds 3>
|
    day
        ^ <170 56 epochSeconds 4>
|
    month
        ^ <170 56 epochSeconds 5>
|
    year
        ^ <170 56 epochSeconds 6> + 1900
|
    asString: format
        ^ <170 57 epochSeconds format>
|
    printString
        ^ self asString
|
    asString
        ^ self asString: '%FT%TZ'
|
	initSNTP
		^ <170 50>
|
	getSNTPTime
		^ <170 51>
|
	getESP32Time
		^ <170 52>
|
	getTimeString: format
		"Return time as string based on linux strftime() format string"
		"see linux.die.net/man/3/strftime"
		^ <170 53 format>
|
	setTimeZone: tzString
		"Set the timezone based on POSIX TZ string and save in flash"
		"see www.gnu.org/software/libc/manual/html_node/TZ-Variable.html"
		^ <170 54 tzString>
]