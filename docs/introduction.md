# **Smalltalkje - A Little Smalltalk for Embedded Devices**

## **Overview of Smalltalkje**

---

## **Goals of Smalltalkje**

In a nutshell I've been playing around with ESP32 based devices for about 2 years... the ESP32 is a cheap, small processor that integrates a dual-core 32-bit CPU, 520KB RAM, 4MB of flash, WiFi, Bluetooth and a variety of peripheral into a single module which are sold for around $5 (or Euro... I live in Europe, but still have my American habits).

Fun devices can be found for under $10 like the [M5StickC](https://m5stack-store.myshopify.com/products/stick-c)... there are also 100s of other various devices including buttons, watches, cameras based on the ESP32... all super inexpensive!

One result of this is that long term the development environment of Smalltalkje will run on a Raspberry Pi Zero... so a full development system would be a Pi Zero and a M5StickC... for a cost of $20... and hopefully can be given away to kids all over the world to spark their genius and creativity regardless of income or background.

However development on Arduinos or the ESP32 SDK relies on C++ and a lot of "overhead... even as an experienced programmer one runs into a lot of frustration with not only developing in a lower level language, but also the quirks of various library versions, tools, configurations, etc... you find yourself quickly 2 or 3 levels away from the fun of your original project searching on the Internet for a solution... 

And if you're working with your kids (or they're on their own tinkering) it's frustrating enough that they'll lose interest in something they may have enjoyed.

I was introduced to programming at an early age in the 70's with the benefit of being in Palo Alto... so we had access to HP, Xerox PARC and Stanford. We got to focus on programming and logic which lit a fire for many of us that continues today.

The goal of Smalltalkje is to bring that fun to programming for little fun gadgets... there are plenty of tools, classes and resources for learning on traditional PCs/tablets/phones, but there's something about having a little button or gadget controlling motors, legos and other projects that drive creativity.

Smalltalkje follows in the footsteps of the creators of Smalltalk and Little Smalltalk to enable the young and young at heart to learn about software, hardware and development.

---

## **About Smalltalkje**

Smalltalkje is based on an early version of Little Smalltalk. All the Smalltalks available in the public domain are not targeting embedded systems, which have some unique constraints (10x-1000x less memory, 10x-100x less CPU power, needs to run from ROM and RAM, RTOS integration). Based on this (and not wanting to start from scratch) I decided to use Little Smalltalk v3 as a base. For more information on why and the details of the implementation see [Smalltalkje implementation](www.example.com)

### What is Smalltalk?

From Wikipedia:
>  *Smalltalk was created as the language underpinning the "new world" of computing exemplified by "human–computer symbiosis". It was designed and created in part for educational use, specifically for constructionist learning, at the Learning Research Group (LRG) of Xerox PARC by Alan Kay, Dan Ingalls, Adele Goldberg, Ted Kaehler, Diana Merry, Scott Wallace, and others during the 1970s*

> *Smalltalk took second place for "most loved programming language" in the Stack Overflow Developer Survey in 2017*

https://en.wikipedia.org/wiki/Smalltalk


Smalltalk, besides being the "Grandfather" of modern object oriented languages, is also one of the... if not the most expressive languages, allowing for prose like programming.

In addition it's well known to have created rich, ahead-of-its time systems on the limited hardware of the 70's and 80's... that makes it ideal for very constrained, Arduino-like devices and it's natural syntanx makes it ideal for kids and adults who want to dive into programming (which was a main goal of the Smalltalk team).

### What is Little Smalltalk

Again from Wikipedia:

>Little Smalltalk is a non-standard dialect of the Smalltalk programming language invented by Timothy Budd. It was originally described in the book: "A Little Smalltalk", Timothy Budd, Addison-Wesley, 1987, ISBN 0-201-10698-1.

https://en.wikipedia.org/wiki/Little_Smalltalk

)Note: The Little Smalltalk Wikipedia page is very light on content. More information can be found on the [Resources](https://www.example.com) page)

### What is Smalltalkje and why it's different

As mentioned above Smalltalkje is based on a Little Smalltalk and runs on ESP32 based devices... DO WE NEED THIS SECTION OR WILL ALL BE COVERED VIA OTHERS?

### Why is it called "Smalltalkje"

Smalltalkje is based on a Little Smalltalk... I live in the Netherlands... in Dutch the ending "je" indicates the diminutive... so for example if you take Cat (Kat) and add "je" you get Katje, which is Dutch for kitten... so that's why it's Smalltalkje (oh and also for "Smalltalk ***j***ust ***e***mbedded). 

---

## **Navigating the documentation**

---

## **Navigating the project and code**

---

~~~c
void wifi_set_ssid(char *ssid)
{
    strcpy(wifi_ssid, ssid);
}
~~~

~~~smalltalk
value: x value: y
	^ (self checkArgumentCount: 2)
		ifTrue: [ context at: argLoc put: x.
				  context at: argLoc + 1 put: y.
				  context returnToBlock: bytePointer ]
~~~
