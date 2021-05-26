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
#include <chrono>
#include <thread>
#include <fstream>

// TODO LOGGINING

//#include <mqtt/client.h>

#include <signal.h>

using namespace std;
using namespace Pistache;
#define windowStatVal "windowStatus"
#define blindStatVal "blindsStatus"
#define weatherModeVal "weatherMode"

void logger(std::string message){
    message += "\n";
    cout << message;

    // poate putem face cumva sa nu mai deschid fisierul mereu can apelam logger
    ofstream file;
    // numele fisierului ar trb sa aiba un timestamp
    file.open("log.txt");
    file << message;
    file.close();
}

// Definition of the WindowEnpoint class
class WindowEndpoint
{
public:
    struct UserPreferences
    {
        int roomTemperature;
        int blindsLevel;
        std::unordered_map<std::string, int> weatherVals;
    };

    explicit WindowEndpoint(Address addr)
        : httpEndpoint(std::make_shared<Http::Endpoint>(addr)) {}

    // Initialization of the server. Additional options can be provided here
    void init(size_t thr = 2)
    {
        auto opts = Http::Endpoint::options()
                        .threads(static_cast<int>(thr));
        httpEndpoint->init(opts);
        // Server routes are loaded up
        setupRoutes();
    }

    // Server is started threaded.
    void start()
    {
        logger("HTTP Server started...");
        httpEndpoint->setHandler(router.handler());
        httpEndpoint->serveThreaded();
        //AutomaticFeature();
        thread t1(&WindowEndpoint::ThreadFunction, this);
        t1.detach();
        //printTest();
    }

    // When signaled server shuts down
    void stop()
    {
        httpEndpoint->shutdown();
    }

private:
    class Window
    {
        // Defining the class of the window. It should model the entire configuration of the window
    private:
        UserPreferences userPreferences;
        // Defining and instantiating settings.
        struct boolSetting
        {
            std::string name;
            // Defines how open is the window/blinds. 0 = closed, 100 = max open
            int openPercentage;
        };

        // Windows status
        boolSetting windowStatus;
        // Blinds status
        boolSetting blindsStatus;

        bool automaticLightEnabled;
        bool automaticTempEnabled;
        bool alarmEnabled;
        bool weatherModeEnabled;
        bool ledPower;
        std::string ledcolor;
        bool selfCleaning;
        int cleanPercentage;
        
        std::vector<pair<int, int>> alarmStartTime;
        std::vector<std::string> weatherStates{"rainy", "cloudy", "sunny"};
        std::vector<std::string> windowOpenTime;
        std::vector<std::string> windowCloseTime;

    public:
        explicit Window()
        {
            // Am putea face si cu json pt al treilea input...
            ifstream file;
            file.open("input.txt");

            vector<int> upf;
            std::string value;
            while (getline(file, value)){
                int val = stoi(value);
                upf.push_back(val);
                value = "";
            }

            // Set default values
            setUserPreferences(upf[0], upf[1], upf[2], upf[3], upf[4]);
            //setUserPreferences(20, 20, 40, 50, 60);
            set(windowStatVal, 0);
            set(blindStatVal, 10);
            automaticLightEnabled = false;
            automaticTempEnabled = false;
            alarmEnabled = false;
            ledPower = false;
        }

#pragma region
        void addWindowOpenTime(std::string time)
        {
            windowOpenTime.push_back(time);
        }

        void addWindowCloseTime(std::string time)
        {
            windowCloseTime.push_back(time);
        }

        std::vector<std::string> getWindowOpenTime()
        {
            return windowOpenTime;
        }

        std::vector<std::string> getWindowCloseTime()
        {
            return windowCloseTime;
        }

        std::vector<std::string> getWeatherStates()
        {
            return weatherStates;
        }

        void setLedPower(bool value){
            ledPower = value;
        }

        bool getLedPower(){
            return ledPower;
        }

        void setLedColor(std::string value){
            ledcolor = value;
        }

        std::string getLedColor(){
            return ledcolor;
        }

        void setSelfCleaning(bool value){
            selfCleaning = value;
        }
        
        bool getSelfCleaning(){
            return selfCleaning;
        }

        void setCleanPercentage(int value){
            cleanPercentage = value;
        }
        
        int getCleanPercentage(){
            return cleanPercentage;
        }

        bool isAutomaticLightEnabled()
        {
            return automaticLightEnabled;
        }

        bool isAutomaticTempEnabled()
        {
            return automaticTempEnabled;
        }

        bool isAlarmEnabled()
        {
            return alarmEnabled;
        }

        void setAlarmEnabled(bool value)
        {
            alarmEnabled = value;
        }

        bool isWeatherModeEnabled()
        {
            return weatherModeEnabled;
        }

        void setWeatherMode(bool value)
        {
            weatherModeEnabled = value;
        }

        std::vector<pair<int, int>> getAlarmStartTime()
        {
            return alarmStartTime;
        }

        int getAlarmVectorSize()
        {
            return alarmStartTime.size();
        }

        void setAutomaticLight(bool value)
        {
            automaticLightEnabled = value;
        }

        void setAutomaticTemp(bool value)
        {
            automaticTempEnabled = value;
        }

        void setAlarmStatus(bool value)
        {
            alarmEnabled = value;
        }
#pragma endregion

        void popAlarmList()
        {
            // Remove last element
            alarmStartTime.pop_back();
        }

        void addAlarmStartTime(pair<int, int> value)
        {
            alarmStartTime.push_back(value);
            std::sort(alarmStartTime.begin(), alarmStartTime.end());
            std::reverse(alarmStartTime.begin(), alarmStartTime.end());
        }

        void setUserPreferences(int temp, int blindsLevel, int rainyPref, int cloudyPref, int sunnyPref)
        {
            userPreferences.roomTemperature = temp;
            userPreferences.blindsLevel = blindsLevel;

            userPreferences.weatherVals["rainy"] = rainyPref;
            userPreferences.weatherVals["cloudy"] = cloudyPref;
            userPreferences.weatherVals["sunny"] = sunnyPref;
        }

        UserPreferences getUserPreferences()
        {
            return userPreferences;
        }

        // Setting the value for one of the settings. Hardcoded for the windowStatusing option
        int set(std::string name, int percentage)
        {

            if (percentage < 0 || percentage > 100)
            {
                percentage = 0;
            }

            if (name == windowStatVal)
            {
                windowStatus.openPercentage = percentage;
                return 1;
            }

            if (name == blindStatVal)
            {
                blindsStatus.openPercentage = percentage;
                return 1;
            }

            return 0;
        }

        // Getter
        std::string get(std::string name)
        {
            if (name == windowStatVal)
            {
                return std::to_string(windowStatus.openPercentage);
            }

            if (name == blindStatVal)
            {
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

    void setupRoutes()
    {
        using namespace Rest;
        // Defining various endpoints
        // Generally say that when http://localhost:9080/ready is called, the handleReady function should be called.
        Routes::Get(router, "/ready", Routes::bind(&WindowEndpoint::handleReady, this));
        Routes::Get(router, "/auth", Routes::bind(&WindowEndpoint::doAuth, this));

        Routes::Post(router, "/user/:val1/:val2/:val3/:val4/:val5", Routes::bind(&WindowEndpoint::setUserSettings, this));
        Routes::Get(router, "/user", Routes::bind(&WindowEndpoint::getUserSettings, this));
        // :action = open || close
        Routes::Post(router, "/windowTimer/:action/:hour/:minute", Routes::bind(&WindowEndpoint::setWindowTimer, this));

        // Value is window/blinds openPercentage
        Routes::Post(router, "/:settingName/:value", Routes::bind(&WindowEndpoint::setSetting, this));
        Routes::Get(router, "/:settingName", Routes::bind(&WindowEndpoint::getSetting, this));

        // Auto does not have get route as its status can be seen on /ready route
        Routes::Post(router, "/auto/:settingName/:val", Routes::bind(&WindowEndpoint::setAuto, this));

        Routes::Post(router, "/alarm/:val1/:val2", Routes::bind(&WindowEndpoint::setAlarm, this));
        Routes::Post(router, "/alarm/isOn/:val", Routes::bind(&WindowEndpoint::setAlarmStatus, this));

        Routes::Post(router, "/led/power/:val", Routes::bind(&WindowEndpoint::setLeds, this));
        Routes::Post(router, "/led/color/:val", Routes::bind(&WindowEndpoint::setLedsColor, this));
        Routes::Post(router, "/selfclean/:val", Routes::bind(&WindowEndpoint::setSelfClean, this));
    }

    // Returns time in format hh:mmAM
    std::string getCurrentTime()
    {
        #include <time.h>
        time_t rawtime;
        struct tm *timeinfo;

        time(&rawtime);
        timeinfo = localtime(&rawtime);

        std::string time = asctime(timeinfo);
        // hours:minute begins at pos 11, we only need first 5 pos
        return time.substr(11, 5);
    }

    void AutomaticFeature()
    {
        if (window.isAutomaticLightEnabled())
        {
            AutomaticLightController();
        }

        if (window.isAutomaticTempEnabled())
        {
            AutomaticTempController();
        }

        int alarmVectorSize = window.getAlarmVectorSize();
        if (window.isAlarmEnabled() && alarmVectorSize > 0)
        {
            int currTime = time(0);

            int alarmTime = window.getAlarmStartTime()[alarmVectorSize - 1].first;
            int alarmSettings = window.getAlarmStartTime()[alarmVectorSize - 1].second;

            if (currTime > alarmTime)
            {
                window.set(blindStatVal, alarmSettings);
                window.popAlarmList();
                int newVectorSize = window.getAlarmVectorSize();
                if (newVectorSize == 0)
                {
                    window.setAlarmStatus(0);
                }
            }
        }

        std::vector<std::string> wndwOpenTime = window.getWindowOpenTime();
        if (wndwOpenTime.size() > 0)
        {
            std::string currTime = getCurrentTime();
            for (auto openTime : wndwOpenTime)
            {
                if (currTime == openTime)
                {
                    // Open the window
                    int roomTemp = window.getUserPreferences().roomTemperature;
                    int temp1, temp2 = getOutsideTemperature(), getInsideTemperature();
                    int percentage = findPercentage(temp1, temp2, roomTemp);

                    window.set(windowStatVal, percentage);
                }
            }
        }

        std::vector<std::string> wndwCloseTime = window.getWindowCloseTime();
        if (wndwCloseTime.size() > 0)
        {
            std::string currTime = getCurrentTime();
            for (auto closeTime : wndwCloseTime)
            {
                if (currTime.compare(closeTime) == 0)
                {
                    // Close the window
                    window.set(windowStatVal, 0);
                }
            }
        }

        if (window.isWeatherModeEnabled())
        {
            std::string key = getWeather();
            auto weatherVals = window.getUserPreferences().weatherVals;

            if (!(weatherVals.find(key) == weatherVals.end()))
            {
                int percentage = weatherVals.at(key);
                window.set(blindStatVal, percentage);

            }
            std::cout << key << std::endl;
        }

        int cleanPercentage = getSensorCleanPergentage();
        if (cleanPercentage < 100){
            window.setCleanPercentage(cleanPercentage);
            // Start cleaning
            int cleanSpeed = 100 - cleanPercentage;

            // TODO sa legam cleaning de vreme sau altcv
            // ca nu stiu ce sa fac mai departe :))
        }
    }

    void ThreadFunction()
    {
        while (true)
        {
            AutomaticFeature();
            this_thread::sleep_for(chrono::milliseconds(1000));
        }
    }

    void setWindowTimer(const Rest::Request &request, Http::ResponseWriter response)
    {
        logger("setWindowTimer");
        Guard guard(WindowLock);

        int hour = 0;
        if (request.hasParam(":hour"))
        {
            auto value = request.param(":hour");
            hour = value.as<int>();
            if (hour > 24)
            {
                hour = 0;
            }
        }   

        int minute = 0;
        if (request.hasParam(":minute"))
        {
            auto value = request.param(":minute");
            minute = value.as<int>();
            if (minute > 60)
            {
                minute = 0;
            }
        }

        // Formatam textul hh:mm
        std::string inputTime = std::to_string(hour) + ":" + std::to_string(minute);

        std::string actionName;
        if (request.hasParam(":action"))
        {
            auto value = request.param(":action");
            actionName = value.as<std::string>();
        }

        std::string actionOpen = "open";
        std::string actionClose = "close";
        if (actionName == actionOpen)
        {
            window.addWindowOpenTime(inputTime);
            response.send(Http::Code::Ok, "Window will open at " + inputTime + "\n");
        }
        else if (actionName.compare(actionClose) == 0)
        {
            window.addWindowCloseTime(inputTime);
            response.send(Http::Code::Ok, "Window will close at " + inputTime + "\n");
        }
        else{
            response.send(Http::Code::Not_Found, "Wrong action\n");
        }
    }

    void setAlarmStatus(const Rest::Request &request, Http::ResponseWriter response)
    {
        Guard guard(WindowLock);

        bool val = 0;
        if (request.hasParam(":val"))
        {
            auto Value = request.param(":val");
            val = Value.as<bool>();
        }

        window.setAlarmEnabled(val);

        response.send(Http::Code::Ok, "Is alarm on? " + std::to_string(val) + "\n");
    }

    void setSelfClean(const Rest::Request &request, Http::ResponseWriter response){
        Guard guard(WindowLock);

        bool val = 0;
        if (request.hasParam(":val"))
        {
            auto Value = request.param(":val");
            val = Value.as<bool>();
        }

        window.setSelfCleaning(val);
        response.send(Http::Code::Ok, "Is selfcleaning on? " + std::to_string(val) + "\n");
    }

    void setLeds(const Rest::Request &request, Http::ResponseWriter response){
        Guard guard(WindowLock);

        bool val = 0;
        if (request.hasParam(":val"))
        {
            auto Value = request.param(":val");
            val = Value.as<bool>();
        }

        window.setLedPower(val);
        std::string color = window.getLedColor();
        if (color == ""){
            window.setLedColor("000000");
        }

        response.send(Http::Code::Ok, "Are Led on? " + std::to_string(val) + "\n");
    }

    void setLedsColor(const Rest::Request &request, Http::ResponseWriter response){
        Guard guard(WindowLock);

        std::string val = "";
        if (request.hasParam(":val"))
        {
            auto Value = request.param(":val");
            val = Value.as<std::string>();
        }

        //validate input wih regex
        if ( regex_match (val, regex("[a-fA-F0-9]{6}") ))
        {
            window.setLedPower(true);
            window.setLedColor(val);
            response.send(Http::Code::Ok, "Led are set to " + val + "\n");
            return;
        }

        response.send(Http::Code::Not_Found, "Bad input");
    }
    // void setWeather(const Rest::Request &request, Http::ResponseWriter response)
    // {
    //    auto settingName = request.param(":weatherType").as<std::string>();
    // }

    void setAuto(const Rest::Request &request, Http::ResponseWriter response)
    {
        auto settingName = request.param(":settingName").as<std::string>();

        Guard guard(WindowLock);
        bool val = 0;
        if (request.hasParam(":val"))
        {
            auto Value = request.param(":val");
            val = Value.as<bool>();
        }

        if (settingName == blindStatVal)
        {
            window.setAutomaticLight(val);
            response.send(Http::Code::Ok, " auto " + settingName + " was set to " + std::to_string(val) + "\n");
        }
        else if (settingName == windowStatVal)
        {
            window.setAutomaticTemp(val);
            response.send(Http::Code::Ok, " auto " + settingName + " was set to " + std::to_string(val) + "\n");
        }
        else if (settingName == weatherModeVal)
        {
            window.setWeatherMode(val);
            response.send(Http::Code::Ok, " auto " + settingName + " was set to " + std::to_string(val) + "\n");
        }
        else
        {
            response.send(Http::Code::Not_Found, settingName + " was not found and or '" + std::to_string(val) + "' was not a valid value\n");
        }
    }

    void setAlarm(const Rest::Request &request, Http::ResponseWriter response)
    {
        Guard guard(WindowLock);

        int val1 = 0;
        if (request.hasParam(":val1"))
        {
            auto Value = request.param(":val1");
            val1 = Value.as<int>();
        }

        int val2 = 0;
        if (request.hasParam(":val2"))
        {
            auto Value = request.param(":val2");
            val2 = Value.as<int>();
        }

        if (val1 > 0 && val2 > 0)
        {
            window.setAutomaticLight(0);
            window.setAutomaticTemp(0);

            int currentTime = time(0);
            pair<int, int> alarmValues;

            alarmValues.first = currentTime + val1;
            alarmValues.second = val2;

            window.addAlarmStartTime(alarmValues);
            window.setAlarmStatus(1);

            response.send(Http::Code::Ok, "alarm starts in " + std::to_string(val1) + " with blinds open at " + std::to_string(val2) + "\n");
        }
        else if (val1 == 0)
        {
            window.setAlarmStatus(0);
            response.send(Http::Code::Ok, "alarm disabled\n");
        }
        else
        {
            response.send(Http::Code::Not_Found, std::to_string(val1) + " or " + std::to_string(val2) + " was not a valid value\n");
        }
    }

    void handleReady(const Rest::Request &request, Http::ResponseWriter response)
    {
        
        logger("[GET] /ready");
        std::string windows = "Window " + window.get(windowStatVal);
        std::string blinds = "Blinds " + window.get(blindStatVal);

        UserPreferences userPref = window.getUserPreferences();
        std::string userPrefsText = "\nUser prefs: temp=" + std::to_string(userPref.roomTemperature) + ", lightLvl=" + std::to_string(userPref.blindsLevel) + 
        ", rainy open % =" + std::to_string(userPref.weatherVals["rainy"]) + 
        ", cloudy open % =" + std::to_string(userPref.weatherVals["cloudy"]) +
        ", sunny open % =" + std::to_string(userPref.weatherVals["sunny"]) +"\n";

        std::string autoFeatureLight = "Auto Light disabled";
        std::string autoFeatureTemp = "Auto Temp disabled";
        if (window.isAutomaticLightEnabled())
        {
            autoFeatureLight = "Auto for LIGHT is enabled";
        }

        if (window.isAutomaticTempEnabled())
        {
            autoFeatureTemp = "Auto for TEMP is enabled";
        }

        std::string ledStatus = "\nLeds are off\n";
        if (window.getLedPower()){
            ledStatus = "\nLeds are on, color " + window.getLedColor() + "\n";
        }

        std::string selfClean = "Self cleaning is off\n";
        if (window.getSelfCleaning()){
            selfClean = "Self cleaning is on " + std::to_string(window.getCleanPercentage()) + "\n";
        }

        std::string textResponse = windows + "\n" + blinds 
        + userPrefsText + autoFeatureLight + "\n" + autoFeatureTemp
        + ledStatus + selfClean;
        response.send(Http::Code::Ok, textResponse + "\n");
    }

    void setUserSettings(const Rest::Request &request, Http::ResponseWriter response)
    {
        // This is a guard that prevents editing the same value by two concurent threads.
        Guard guard(WindowLock);
        int val1 = 0, val2 = 0, val3 = 0, val4 = 0, val5 = 0;

        if (request.hasParam(":val1"))
        {
            auto Value = request.param(":val1");
            val1 = Value.as<int>();
        }

        if (request.hasParam(":val2"))
        {
            auto Value = request.param(":val2");
            val2 = Value.as<int>();
        }

        //weather percentage
        if (request.hasParam(":val3"))
        {
            auto Value = request.param(":val3");
            val3 = Value.as<int>();
        }

        if (request.hasParam(":val4"))
        {
            auto Value = request.param(":val4");
            val4 = Value.as<int>();
        }

        if (request.hasParam(":val5"))
        {
            auto Value = request.param(":val5");
            val5 = Value.as<int>();
        }

        if (val1 > 100)
            val1 = 0;

        if (val2 > 100)
            val2 = 0;

        //weather perc
        if (val3 > 100)
            val3 = 0;

        if (val3 > 100)
            val3 = 0;

        if (val3 > 100)
            val3 = 0;

        // Sending some confirmation or error response.
        if (val1 == 0 || val2 == 0 || val3 == 0 || val4 == 0 || val5 == 0)
        {
            response.send(Http::Code::Not_Found,
                          "Not found and or '" + std::to_string(val1) +
                              "or " + std::to_string(val2) + " was not a valid value\n");
        }
        else
        {
            // Success
            window.setUserPreferences(val1, val2, val3, val4, val5);
        }

        response.send(Http::Code::Ok, "User preferences were set to " + std::to_string(val1) + " " +
                                          std::to_string(val2) + " " +
                                          std::to_string(val3) + " " +
                                          std::to_string(val4) + " " +
                                          std::to_string(val5) + " " + "\n");
    }

    void getUserSettings(const Rest::Request &, Http::ResponseWriter response)
    {
        // This is a guard that prevents editing the same value by two concurent threads.
        Guard guard(WindowLock);

        // In this response I also add a couple of headers, describing the server that sent this response, and the way the content is formatted.
        using namespace Http;
        response.headers()
            .add<Header::Server>("pistache/0.1")
            .add<Header::ContentType>(MIME(Text, Plain));

        UserPreferences userPref = window.getUserPreferences();
        std::string message = std::to_string(userPref.roomTemperature) + " " + std::to_string(userPref.blindsLevel) + "\n";
        response.send(Http::Code::Ok, message);
    }

    void doAuth(const Rest::Request &request, Http::ResponseWriter response)
    {
        // In the response object, it adds a cookie regarding the communications language.
        response.cookies()
            .add(Http::Cookie("lang", "en-US"));
        // Send the response
        response.send(Http::Code::Ok);
    }

    // Endpoint to configure one of the Window's settings.
    void setSetting(const Rest::Request &request, Http::ResponseWriter response)
    {
        // You don't know what the parameter content that you receive is, but you should
        // try to cast it to some data structure. Here, I cast the settingName to string.
        auto settingName = request.param(":settingName").as<std::string>();

        // This is a guard that prevents editing the same value by two concurent threads.
        Guard guard(WindowLock);

        int val;
        // If request has value for open-percentage
        if (request.hasParam(":value"))
        {
            auto Value = request.param(":value");
            val = Value.as<int>();
        }

        // Setting the Window's setting to value
        int setResponse = window.set(settingName, val);

        // Sending some confirmation or error response.
        if (setResponse == 1)
        {
            response.send(Http::Code::Ok, settingName + " was set to " + std::to_string(val) + "\n");
        }
        else
        {
            response.send(Http::Code::Not_Found, settingName + " was not found and or '" + std::to_string(val) + "' was not a valid value\n");
        }
    }

    // Setting to get the settings value of one of the configurations of the Window
    void getSetting(const Rest::Request &request, Http::ResponseWriter response)
    {
        auto settingName = request.param(":settingName").as<std::string>();
        Guard guard(WindowLock);

        string valueSetting = window.get(settingName);

        if (valueSetting != "")
        {

            // In this response I also add a couple of headers, describing the server that sent this response, and the way the content is formatted.
            using namespace Http;
            response.headers()
                .add<Header::Server>("pistache/0.1")
                .add<Header::ContentType>(MIME(Text, Plain));

            response.send(Http::Code::Ok, settingName + " is " + valueSetting);
        }
        else
        {
            response.send(Http::Code::Not_Found, settingName + " was not found");
        }
    }

    // Returns a number between 0-100
    int getLightIntensity()
    {
        srand(time(0));
        return rand() % 101;
    }

    // Returns a number between 0-100 - this simulates a temperature sensor
    int getInsideTemperature()
    {
        int currTime = time(0);
        srand(currTime + 1);
        return rand() % 41;
    }

    // Returns a number between 0-100 - this simulates a temperature sensor
    int getOutsideTemperature()
    {
        int currTime = time(0);
        srand(currTime);
        return rand() % 41;
    }

    // Simulate clean sensor
    int getSensorCleanPergentage(){
        return rand() % 100;
    }

    std::string getWeather()
    {
        // window.
        std::vector<std::string> weather;
        weather = window.getWeatherStates();

        int currTime = time(0);
        srand(currTime);

        std::string currentWeather = weather[rand() % 3];
        return currentWeather;
    }

    int findPercentage(int inTemp, int outTemp, int prefTemp)
    {
        bool cond1 = inTemp < prefTemp < outTemp;
        bool cond2 = outTemp < prefTemp < inTemp;

        // If opening window helps inTemp reach pref temp
        if (cond1 || cond2)
        {
            unsigned int tempDifference = abs(inTemp - outTemp);
            // Open the window 15% for every grade in temp difference
            int percentage = tempDifference * 2.6;
            if (percentage > 100)
            {
                percentage = 100;
            }

            return percentage;
        }

        // Opening the window won't help to reach pref temp
        return 0;
    }

    void AutomaticTempController()
    {
        int insideTemp = getInsideTemperature();
        int outsideTemp = getOutsideTemperature();

        int prefTemp = window.getUserPreferences().roomTemperature;
        int percentage = findPercentage(insideTemp, outsideTemp, prefTemp);

        window.set(windowStatVal, percentage);
    }

    void AutomaticLightController()
    {
        // Light intensity and blindStatVal are both a percentage about light
        int lightIntensity = getLightIntensity();

        int userLightIntPref = 100 - window.getUserPreferences().blindsLevel;

        float percentage = 100 - (float(userLightIntPref) / float(lightIntensity)) * 100;

        window.set(blindStatVal, percentage);
    }
};

void mqttExample()
{
    const std::string address = "localhost";
    const std::string clientId = "window";

    // Create a client
    // mqtt::client client(address, clientId);

    // mqtt::connect_options options;
    // options.set_keep_alive_interval(20);
    // options.set_clean_session(true);

    // try {
    //     // Connect to the client
    //     client.connect(options);

    //     // Create a message
    //     const std::string TOPIC = "window";
    //     const std::string PAYLOAD = "Hello World!";
    //     auto msg = mqtt::make_message(TOPIC, PAYLOAD);

    //     // Publish it to the server
    //     client.publish(msg);

    //     // Disconnect
    //     client.disconnect();
    // }
    // catch (const mqtt::exception& exc) {
    //     std::cerr << exc.what() << " [" << exc.get_reason_code() << "]" << std::endl;
    // }
}

void httpExample(int argc, char *argv[])
{

    // This code is needed for gracefull shutdown of the server when no longer needed.
    sigset_t signals;
    if (sigemptyset(&signals) != 0 || sigaddset(&signals, SIGTERM) != 0 || sigaddset(&signals, SIGINT) != 0 || sigaddset(&signals, SIGHUP) != 0 || pthread_sigmask(SIG_BLOCK, &signals, nullptr) != 0)
    {
        perror("install signal handler failed");
        return;
    }

    // Set a port on which your server to communicate
    Port port(9080);

    // Number of threads used by the server
    int thr = 2;

    if (argc >= 2)
    {
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

int main(int argc, char *argv[])
{

    //int program = fork();
    httpExample(argc, argv);

    // // Parent executes mqtt
    // if (program > 0){
    // 	//mqttExample();
    // }
    // else if (program == 0){
    // 	httpExample(argc, argv);
    // }
}