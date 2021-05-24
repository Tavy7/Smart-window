/*
   Rares Cristea, 12.03.2021
   Example of a REST endpoint with routing
   using Mathieu Stefani's example, 07 février 2016
*/

#include <algorithm>
#include <stdlib.h> 
#include <pistache/net.h>
#include <pistache/http.h>
#include <pistache/peer.h>
#include <pistache/http_headers.h>
#include <pistache/cookie.h>
#include <pistache/router.h>
#include <pistache/endpoint.h>
#include <pistache/common.h>
#include <ctime>    
#include <mqtt/client.h>

#include <signal.h>

using namespace std;
using namespace Pistache;

#define windowStatVal "windowStatus"
#define blindStatVal "blindsStatus"

// General advice: pay atetntion to the namespaces that you use in various contexts. Could prevent headaches.

// This is just a helper function to preety-print the Cookies that one of the enpoints shall receive.
void printCookies(const Http::Request& req) {
    auto cookies = req.cookies();
    std::cout << "Cookies: [" << std::endl;
    const std::string indent(4, ' ');
    for (const auto& c: cookies) {
        std::cout << indent << c.name << " = " << c.value << std::endl;
    }
    std::cout << "]" << std::endl;
}

// Definition of the WindowEnpoint class 
class WindowEndpoint {
public:
    struct UserPreferences{
        int roomTemperature;
      int lightLevel;
    };

    explicit WindowEndpoint(Address addr)
        : httpEndpoint(std::make_shared<Http::Endpoint>(addr)) { }

    // Initialization of the server. Additional options can be provided here
    void init(size_t thr = 2) {
        auto opts = Http::Endpoint::options()
            .threads(static_cast<int>(thr));
        httpEndpoint->init(opts);
        // Server routes are loaded up
        setupRoutes();
    }

    // Server is started threaded.  
    void start() {
        httpEndpoint->setHandler(router.handler());
        httpEndpoint->serveThreaded();
        AutomaticFeature();
    }

    // When signaled server shuts down
    void stop(){
        httpEndpoint->shutdown();
    }

private:

    class Window {
    // Defining the class of the window. It should model the entire configuration of the window
    private:
        UserPreferences userPreferences;
        // Defining and instantiating settings.
        struct boolSetting{
            std::string name;
            // Defines how open is the window/blinds. 0 = closed, 100 = max open
            int openPercentage;
        };

        // Windows status
        boolSetting windowStatus;
        // Blinds status
        boolSetting blindsStatus;
        bool automaticFeatureEnabled;

    public:
        explicit Window(){ 
            // Set default values
            setUserPreferences(20, 100);
            set(windowStatVal, 0);
            set(blindStatVal, 10);    
            automaticFeatureEnabled = true;
        }

        bool isAutomaticEnabled(){
            return automaticFeatureEnabled;
        }

        void setAutomaticFeature(bool value){
            automaticFeatureEnabled = value;
        }

        void setUserPreferences(int temp, int lightLevel)
        {
            userPreferences.roomTemperature = temp;
            userPreferences.lightLevel = lightLevel;
        }

        UserPreferences getUserPreferences(){
            return userPreferences;
        }

        // Setting the value for one of the settings. Hardcoded for the windowStatusing option
        int set(std::string name, int percentage){

            if (percentage < 0 || percentage > 100){
                percentage = 0;
            }

            if(name == windowStatVal){
                windowStatus.openPercentage = percentage;
                return 1;
            }

            if(name == blindStatVal){ 
                blindsStatus.openPercentage = percentage;
                return 1;
            }

            return 0;
        }

        // Getter
        std::string get(std::string name){
            if (name == windowStatVal){
                return std::to_string(windowStatus.openPercentage);
            }
            
            if (name == blindStatVal){
                return std::to_string(blindsStatus.openPercentage);
            }

            return "";
        }
    };

    // Create the lock which prevents concurrent editing of the same variable
    using Lock = std::mutex;
    using Guard = std::lock_guard<Lock>;
    Lock WindowLock;

    // Instance of the Window model
    Window window;

    // Defining the httpEndpoint and a router.
    std::shared_ptr<Http::Endpoint> httpEndpoint;
    Rest::Router router;

    void setupRoutes() {
        using namespace Rest;
        // Defining various endpoints
        // Generally say that when http://localhost:9080/ready is called, the handleReady function should be called. 
        Routes::Get(router, "/ready", Routes::bind(&WindowEndpoint::handleReady, this));
        Routes::Get(router, "/auth", Routes::bind(&WindowEndpoint::doAuth, this));
        // Value is window/blinds openPercentage
        Routes::Post(router, "/settings/:settingName/:value", Routes::bind(&WindowEndpoint::setSetting, this));
        Routes::Get(router, "/settings/:settingName/", Routes::bind(&WindowEndpoint::getSetting, this));

        Routes::Post(router, "/settings/user/:val1/:val2", Routes::bind(&WindowEndpoint::setUserSettings, this));
        Routes::Get(router, "/settings/user", Routes::bind(&WindowEndpoint::getUserSettings, this));
        
        // Auto does not have get route as its status can be seen on /ready route
        Routes::Post(router, "/settings/auto/:val", Routes::bind(&WindowEndpoint::setAuto, this));
    }
    
    void setAuto(const Rest::Request& request, Http::ResponseWriter response){
        Guard guard(WindowLock);
        bool val = 0;
        if (request.hasParam(":val")){
            auto Value = request.param(":val");
            val = Value.as<bool>();
        }

        if (val == true){
            AutomaticFeature();
        }

        window.setAutomaticFeature(val);
    }

    void handleReady(const Rest::Request& request, Http::ResponseWriter response) {
        std::string windows = "Window " + window.get(windowStatVal);
        std::string blinds = "Blinds " + window.get(blindStatVal);
        
        UserPreferences userPref = window.getUserPreferences();
        std::string userPrefsText = "\nUser prefs: temp=" + std::to_string(userPref.roomTemperature)
        + " lightLvl=" + std::to_string(userPref.lightLevel) + "\n";

        std::string autoFeature = "Auto disabled";
        if (window.isAutomaticEnabled()){
            autoFeature = "Auto enabled";
        }

        std::string textResponse = windows + "\n" + blinds + userPrefsText + autoFeature;
        response.send(Http::Code::Ok, textResponse + "\n");
    }

    void setUserSettings(const Rest::Request& request, Http::ResponseWriter response){
        // This is a guard that prevents editing the same value by two concurent threads. 
        Guard guard(WindowLock);

        int val1 = 0;
        if (request.hasParam(":val1")) {
            auto Value = request.param(":val1");
            val1 = Value.as<int>();
        }

        if (val1 > 100){
            val1 = 0;
        }


        int val2 = 0;
        if (request.hasParam(":val2")) {
            auto Value = request.param(":val2");
            val2 = Value.as<int>();
        }

        if (val2 > 100){
            val2 = 0;
        }

        // Sending some confirmation or error response.
        if (val1 == 0 || val2 == 0) {
            response.send(Http::Code::Not_Found,  
            "Not found and or '" + std::to_string(val1) +
            "or " + std::to_string(val2) + "' was not a valid value\n");
        }
        else {
            // Success
            window.setUserPreferences(val1, val2);}
            
        response.send(Http::Code::Ok, "User preferences were set to " 
        + std::to_string(val1) + " " + 
        std::to_string(val2) +"\n");
    }

    void getUserSettings(const Rest::Request&, Http::ResponseWriter response){
        // This is a guard that prevents editing the same value by two concurent threads. 
        Guard guard(WindowLock);

      
        // In this response I also add a couple of headers, describing the server that sent this response, and the way the content is formatted.
        using namespace Http;
        response.headers()
                    .add<Header::Server>("pistache/0.1")
                    .add<Header::ContentType>(MIME(Text, Plain));
                        
        UserPreferences userPref = window.getUserPreferences();
        std::string message = std::to_string(userPref.roomTemperature)
            + " " + std::to_string(userPref.lightLevel) + "\n";
        response.send(Http::Code::Ok, message);
    }

    void doAuth(const Rest::Request& request, Http::ResponseWriter response) {
        // Function that prints cookies
        printCookies(request);
        // In the response object, it adds a cookie regarding the communications language.
        response.cookies()
            .add(Http::Cookie("lang", "en-US"));
        // Send the response
        response.send(Http::Code::Ok);
    }

    // Endpoint to configure one of the Window's settings.
    void setSetting(const Rest::Request& request, Http::ResponseWriter response){
        // You don't know what the parameter content that you receive is, but you should
        // try to cast it to some data structure. Here, I cast the settingName to string.
        auto settingName = request.param(":settingName").as<std::string>();

        // This is a guard that prevents editing the same value by two concurent threads. 
        Guard guard(WindowLock);

        int val;
        // If request has value for open-percentage
        if (request.hasParam(":value")) {
            auto Value = request.param(":value");
            val = Value.as<int>();
        }

        // Setting the Window's setting to value
        int setResponse = window.set(settingName, val);

        // Sending some confirmation or error response.
        if (setResponse == 1) {
            response.send(Http::Code::Ok, settingName + " was set to " + std::to_string(val) + "\n");
        }
        else {
            response.send(Http::Code::Not_Found, settingName + " was not found and or '" + std::to_string(val) + "' was not a valid value\n");
        }
    }

    // Setting to get the settings value of one of the configurations of the Window
    void getSetting(const Rest::Request& request, Http::ResponseWriter response){
        auto settingName = request.param(":settingName").as<std::string>();
        Guard guard(WindowLock);

        string valueSetting = window.get(settingName);

        if (valueSetting != "") {

            // In this response I also add a couple of headers, describing the server that sent this response, and the way the content is formatted.
            using namespace Http;
            response.headers()
                        .add<Header::Server>("pistache/0.1")
                        .add<Header::ContentType>(MIME(Text, Plain));

            response.send(Http::Code::Ok, settingName + " is " + valueSetting);
        }
        else {
            response.send(Http::Code::Not_Found, settingName + " was not found");
        }
    }

    // Returns a number between 0-100
    int getLightIntensity(){
        return rand() % 101;
    }

    // Returns a number between 0-100 - this simulates a temperature sensor
    int getInsideTemperature(){
        return rand() % 101;
    }

    // Returns a number between 0-100 - this simulates a temperature sensor
    int getOutsideTemperature(){
        return rand() % 101;
    }

    int findPercentage(int inTemp, int outTemp, int prefTemp){
        bool cond1 = inTemp < prefTemp < outTemp;
        bool cond2 = outTemp < prefTemp < inTemp;

        // If opening window helps inTemp reach pref temp
        if (cond1 || cond2){
            unsigned int tempDifference = abs(inTemp - outTemp);
            // Open the window 15% for every grade in temp difference
            int percentage = tempDifference * 15;
            if (percentage > 100){
                percentage = 100;
            }

            return percentage;
        }
        
        // Opening the window won't help to reach pref temp
        return 0;
    }

    void AutomaticTempController(){ 
        int insideTemp = getInsideTemperature();
        int outsideTemp = getOutsideTemperature();
        
        int prefTemp = window.getUserPreferences().roomTemperature;
        int percentage = findPercentage(insideTemp, outsideTemp, prefTemp);

        window.set(windowStatVal, percentage);
    }
        
    void AutomaticLightController(){
        // Light intensity and blindStatVal are both a percentage about light
        int lightIntensity = getLightIntensity(); 
        int percentage = lightIntensity;

        int userLightIntPref = window.getUserPreferences().lightLevel;

        percentage = 100 - (userLightIntPref / lightIntensity) * 100;

        window.set(blindStatVal, percentage);   
    }

     void AutomaticFeature(){
            int one_minute = 0;
            while(window.isAutomaticEnabled()){
                AutomaticLightController();
                sleep(one_minute);
                AutomaticTempController();
                sleep(one_minute + 60 * 14);
                one_minute = 60;
            }
     }
};

void mqttExample() {
    const std::string address = "localhost";
    const std::string clientId = "window";

    // Create a client
    mqtt::client client(address, clientId);

    mqtt::connect_options options;
    options.set_keep_alive_interval(20);
    options.set_clean_session(true);

    try {
        // Connect to the client
        client.connect(options);

        // Create a message
        const std::string TOPIC = "window";
        const std::string PAYLOAD = "Hello World!";
        auto msg = mqtt::make_message(TOPIC, PAYLOAD);

        // Publish it to the server
        client.publish(msg);

        // Disconnect
        client.disconnect();
    }
    catch (const mqtt::exception& exc) {
        std::cerr << exc.what() << " [" << exc.get_reason_code() << "]" << std::endl;
    }
}

void httpExample(){

    // This code is needed for gracefull shutdown of the server when no longer needed.
    sigset_t signals;
    if (sigemptyset(&signals) != 0
            || sigaddset(&signals, SIGTERM) != 0
            || sigaddset(&signals, SIGINT) != 0
            || sigaddset(&signals, SIGHUP) != 0
            || pthread_sigmask(SIG_BLOCK, &signals, nullptr) != 0) {
        perror("install signal handler failed");
        return 1;
    }

    // Set a port on which your server to communicate
    Port port(9080);

    // Number of threads used by the server
    int thr = 2;

    if (argc >= 2) {
        port = static_cast<uint16_t>(std::stol(argv[1]));

        if (argc == 3)
            thr = std::stoi(argv[2]);
    }

    srand(thr);
    
    Address addr(Ipv4::any(), port);

    cout << "Cores = " << hardware_concurrency() << endl;
    cout << "Using " << thr << " threads" << endl;

    // Instance of the class that defines what the server can do.
    WindowEndpoint stats(addr);

    // Initialize and start the server
    stats.init(thr);
    stats.start();

    // Code that waits for the shutdown sinal for the server
    int signal = 0;
    int status = sigwait(&signals, &signal);
    if (status == 0)
    {
        std::cout << "received signal " << signal << std::endl;
    }
    else
    {
        std::cerr << "sigwait returns " << status << std::endl;
    }

    stats.stop();
}

int main(int argc, char *argv[]) {

	int program = fork();

	// Parent executes mqtt
	if (program > 0){
    	mqttExample();
	}
	else if (program == 0){
		httpExample();
	}
}