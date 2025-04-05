/*
	Smalltalkje, version 1
	Written by Abdul Nabi, code krafters, March 2021

	ESP32 WiFi Support Header
	
	This header defines the interface for WiFi connectivity functions specific to
	the ESP32 platform. It provides a bridge between the Smalltalk environment
	and the ESP32's WiFi capabilities, enabling Smalltalk applications to connect
	to wireless networks, scan for available networks, and configure connection
	parameters.
	
	The functions defined here are typically called by Smalltalk primitives to
	implement networking functionality, allowing Smalltalk applications to
	interact with WiFi networks and Internet services without having to deal
	with the low-level details of the ESP32 WiFi stack.
*/

/**
 * Initialize the WiFi subsystem
 * 
 * This function initializes the ESP32's WiFi stack and prepares it for use.
 * It must be called before any other WiFi operations can be performed.
 * The function configures the WiFi system in station mode (client mode)
 * and registers the necessary event handlers.
 */
void wifi_start(void);

/**
 * Connect to a WiFi network
 * 
 * Initiates a connection to the WiFi network using the SSID and password
 * that were previously set using wifi_set_ssid() and wifi_set_password().
 * The function works asynchronously - it returns immediately and the 
 * connection process continues in the background.
 * 
 * Connection status can be monitored through ESP32 event handlers that
 * are set up during wifi_start().
 */
void wifi_connect(void);

/**
 * Scan for available WiFi networks
 * 
 * Performs a scan for nearby WiFi networks and returns the results as
 * a Smalltalk object (typically an Array of Dictionaries). Each entry
 * contains information about a discovered network including SSID,
 * signal strength, channel, and security type.
 * 
 * @return A Smalltalk object containing the scan results
 */
object wifi_scan(void);

/**
 * Set the SSID for WiFi connection
 * 
 * Configures the SSID (network name) to use for subsequent calls to
 * wifi_connect(). This must be called before attempting to connect
 * to a WiFi network.
 * 
 * @param ssid The name of the WiFi network to connect to
 */
void wifi_set_ssid(char *ssid);

/**
 * Set the password for WiFi connection
 * 
 * Configures the password to use for subsequent calls to wifi_connect().
 * This must be called before attempting to connect to a password-protected
 * WiFi network.
 * 
 * @param password The password for the WiFi network
 */
void wifi_set_password(char *password);
