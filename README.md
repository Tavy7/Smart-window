

# Smart window 
 C++ application using Pistache.

# Prerequisites
Build tested on Ubuntu Server. Pistache doesn't support Windows, but you can use something like WSL or a virtual machine with Linux.
You will need to have a C++ compiler. I used g++ that came preinstalled. Check using g++ -v
You will need to install the Pistache library. On Ubuntu, you can install a pre-built binary as described here.
 
# MQTT
For MQTT protocol support, you can use the Eclipse Paho client library. The Paho C++ library can be installed from source (will require you to also build and install the Paho C library).
MQTT also requires a server (a message broker) to be running in order to have where to send the messages. For this, you can use Eclipse Mosquitto. On Ubuntu, it can be installed from a PPA:

- sudo apt-add-repository ppa:mosquitto-dev/mosquitto-ppa 
- sudo apt update 
- sudo apt install mosquitto
 
# Building
Using Make
You can build the SmartWindow executable by running make.

# Manually
A step by step series of examples that tell you how to get a development env running
You should open the terminal, navigate into the root folder of this repository, and run:

- g++ SmartWindow_example.cpp -o SmartWindow -lpistache -lcrypto -lssl -lpthread

This will compile the project using g++, into an executable called SmartWindow using the libraries pistache, crypto, ssl, pthread. You only really want pistache, but the last three are dependencies of the former. Note that in this compilation process, the order of the libraries is important.

# Running
In a separate terminal, start the Mosquitto server:
- mosquitto -v

The -v argument starts it in verbose mode, meaning it will print out all incoming and outgoing messages.

# To start the server run
./SmartWindow

Your server should display the number of cores being used and no errors.

To test, open up another terminal, and type:
curl http://localhost:9080/ready

When ready is called, the following information will be displayed:
- the percentage of the window (how % is open)
- the percentage of the blinds (how % are open)
- the percentage of temperature and light set by the user
- the value of auto for temp and for light
- the status for led functionality
- the status for safeclean functionality 
- the status for alarm functionality
- if the alarm feature is enabled, is an alarm active?
- the status of blinds schedule


curl -XPOST http://localhost:9080/blindsStatus/:value
Sets the open percentage of the blinds to the given value

curl http://localhost:9080/blindsStatus/
Get the current open percentage of the blinds

curl -XPOST http://localhost:9080/windowStatus/:value
Sets the open percentage of the window to the given value

curl http://localhost:9080/windowStatus/
Get the current open percentage of the blinds

curl -XPOST http://localhost:9080/user/:val1/:val2/val3/val4/val5
Sets user preferences: temperature and light in the room and after that a percent for blinds when outside is: rainy, cloudy, sunny


curl http://localhost:9080/user/
Get all values set above

curl -XPOST http://localhost:9080/auto/windowStatus
Set auto for windowStatus to 0(off) or 1(on) - this will shut down user preferences and will use the base algorithm

curl -XPOST http://localhost:9080/auto/blindsStatus
Set auto for blindsStatus to 0(off) or 1(on) - this will shut down user preferences and will use the base algorithm

curl -XPOST http://localhost:9080/windowTimer/:val/:val/:val
Set the window to open at a specific time: 0(off)/1(on), hour and minute

curl -XPOST http://localhost:9080/schedule/isOn/:val
Start or stop the schedule

curl -XPOST http://localhost:9080/schedule/:val/:val
How many seconds until the alarm will start and at what percent would you like the blinds to be open

curl -XPOST http://localhost:9080/led/power/:val
Start/stop led functionality -

curl -XPOST http://localhost:9080/led/color/:val
Set the color for the led

curl -XPOST http://localhost:9080/safeclean/:val
on/off safeclean functionality that will clean the window when is dirty

# Built With
Pistache - Web server
