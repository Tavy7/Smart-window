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
//#include <mqtt/client.h>

#include <signal.h>

using namespace std;
using namespace Pistache;
#define windowStatVal "windowStatus"
#define blindStatVal "blindsStatus"
#define morningModeVal "morningModeStatus"

// Definition of the WindowEnpoint class
class WindowEndpoint
{
public:
    struct UserPreferences
    {
        int roomTemperature;
        int blindsLevel;
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

        std::vector<int> alarmStartTime;

    public:
        explicit Window()
        {
            // Set default values
            setUserPreferences(20, 20);
            set(windowStatVal, 0);
            set(blindStatVal, 10);
            //automaticFeatureEnabled = true;
            automaticLightEnabled = false;
            automaticTempEnabled = false;
            alarmEnabled = false;
        }

        // bool isAutomaticEnabled()
        // {
        //     return automaticFeatureEnabled;
        // }

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

        std::vector<int> getAlarmStartTime()
        {
            return alarmStartTime;
        }
        
        int getAlarmVectorSize(){
            return alarmStartTime.size();
        }

        // void setAutomaticFeature(bool value)
        // {
        //     automaticFeatureEnabled = value;
        // }

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

        void popAlarmList(){
            // Get last element
            int element = alarmStartTime.back();
            // Remove last element
            alarmStartTime.pop_back();

            int size = alarmStartTime.size(); 
            if (size > 0){
                // If list has atleast one more element
                // I substract time already spent from the last one
                alarmStartTime[size - 1] -= element;
            }
        }

        void setAlarmStartTime(int value)
        {
            alarmStartTime.push_back(value);
            // TODO Sort vector after adding a new value
        }

        void setUserPreferences(int temp, int blindsLevel)
        {
            userPreferences.roomTemperature = temp;
            userPreferences.blindsLevel = blindsLevel;
        }

        // void setMorningMode(bool value)
        // {
        //     if (value == 1) { // daca este activat morningMode
        //         this->setAutomaticLight(0); // atunci opresc automatic light
        //         this->setAutomaticTemp(0); // opresc automatic temp
        //         this->windowStatus.openPercentage = 0; // inchid geamul
        //         this->blindsStatus.openPercentage = 0; // inchid jaluzelele
        //     } else {
        //         this->setAutomaticLight(1); // daca morningMode e inchis, atunci pornesc automatic light
        //         this->setAutomaticTemp(1) // daca morningMode e inchis, atunci pornesc automatic temp
        //     }
        //     morningModeEnabled = value;
        //     //activationtime = 1;
        //     //alarmtime = 4
        // }

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

        Routes::Post(router, "/user/:val1/:val2", Routes::bind(&WindowEndpoint::setUserSettings, this));
        Routes::Get(router, "/user", Routes::bind(&WindowEndpoint::getUserSettings, this));

        // Value is window/blinds openPercentage
        Routes::Post(router, "/:settingName/:value", Routes::bind(&WindowEndpoint::setSetting, this));
        Routes::Get(router, "/:settingName/", Routes::bind(&WindowEndpoint::getSetting, this));

        // Auto does not have get route as its status can be seen on /ready route
        Routes::Post(router, "/settings/auto/:settingName/:val", Routes::bind(&WindowEndpoint::setAuto, this));

        Routes::Post(router, "/alarm/:val/", Routes::bind(&WindowEndpoint::setAlarm, this));
        //Routes::Post(router, "/")
    }

    void AutomaticFeature()
    {
        //std ::cout << "update test" << endl;
        if (window.isAutomaticLightEnabled())
        {
            AutomaticLightController();
        }

        if (window.isAutomaticTempEnabled())
        {
            AutomaticTempController();
        }

        if (window.isAlarmEnabled())
        {
            int currTime = time(0);
            std ::cout << currTime << std ::endl;
            int alarmVectorSize = window.getAlarmVectorSize();
            int alarmTime = window.getAlarmStartTime()[alarmVectorSize - 1];

            if (currTime > alarmTime)
            {
                window.set(blindStatVal, 100);
                window.setAlarmStatus(0);
                window.popAlarmList();
            }
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

        // response.send(Http::Code::Ok, settingName + " was set to " + std::to_string(val) + "\n");

        // if (val == true)
        // {
        //     AutomaticFeature();
        // }
        //AutomaticLightController();

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
        else
        {
            response.send(Http::Code::Not_Found, settingName + " was not found and or '" + std::to_string(val) + "' was not a valid value\n");
        }
        // else if (settingName == morningModeVal)
        // {
        //     //window.setMorningMode(val);
        //     response.send(Http::Code::Ok, " auto " + settingName + " was set to " + std::to_string(val) + "\n");
        // }
    }

    void setAlarm(const Rest::Request &request, Http::ResponseWriter response)
    {

        Guard guard(WindowLock);

        int val = 0;
        if (request.hasParam(":val"))
        {
            auto Value = request.param(":val");
            val = Value.as<int>();
        }

        if (val > 0)
        {
            // dc se leaga de astea cand setezi alarma??
            // window.setAutomaticLight(0);
            // window.setAutomaticTemp(0);
            // window.set(windowStatVal, 0);
            // window.set(blindStatVal, 100);

            int currentTime = time(0);
            window.setAlarmStartTime(currentTime + val);
            window.setAlarmStatus(1);

            response.send(Http::Code::Ok, "alarm starts in " + std::to_string(val) + "\n");
        }
        else if (val == 0)
        {
            window.setAlarmStatus(0);
            response.send(Http::Code::Ok, "alarm disabled\n");
        }
        else
        {
            response.send(Http::Code::Not_Found, std::to_string(val) + " was not a valid value\n");
        }
    }

    void handleReady(const Rest::Request &request, Http::ResponseWriter response)
    {
        std::string windows = "Window " + window.get(windowStatVal);
        std::string blinds = "Blinds " + window.get(blindStatVal);

        UserPreferences userPref = window.getUserPreferences();
        std::string userPrefsText = "\nUser prefs: temp=" + std::to_string(userPref.roomTemperature) 
        + ", lightLvl=" + std::to_string(userPref.blindsLevel) + ", alarm blinds open % ="
        + std::to_string(100 - userPref.blindsLevel) + "\n";

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

        std::string textResponse = windows + "\n" + blinds + userPrefsText + autoFeatureLight + "\n" + autoFeatureTemp;
        response.send(Http::Code::Ok, textResponse + "\n");
    }

    void setUserSettings(const Rest::Request &request, Http::ResponseWriter response)
    {
        // This is a guard that prevents editing the same value by two concurent threads.
        Guard guard(WindowLock);
        std::cout << "test" << std::endl;
        int val1 = 0;
        if (request.hasParam(":val1"))
        {
            auto Value = request.param(":val1");
            val1 = Value.as<int>();
        }

        if (val1 > 100)
        {
            val1 = 0;
        }

        int val2 = 0;
        if (request.hasParam(":val2"))
        {
            auto Value = request.param(":val2");
            val2 = Value.as<int>();
        }

        if (val2 > 100)
        {
            val2 = 0;
        }

        // Sending some confirmation or error response.
        if (val1 == 0 || val2 == 0)
        {
            response.send(Http::Code::Not_Found,
                          "Not found and or '" + std::to_string(val1) +
                              "or " + std::to_string(val2) + " was not a valid value\n");
        }
        else
        {
            // Success
            window.setUserPreferences(val1, val2);
        }

        response.send(Http::Code::Ok, "User preferences were set to " + std::to_string(val1) + " " +
                                          std::to_string(val2) + "\n");
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

        //std ::cout << "test setsett";
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

    int findPercentage(int inTemp, int outTemp, int prefTemp)
    {
        bool cond1 = inTemp < prefTemp < outTemp;
        bool cond2 = outTemp < prefTemp < inTemp;

        // std :: cout << inTemp << std :: endl;
        // std :: cout << outTemp << std :: endl;
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
    Port port(9091);

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
