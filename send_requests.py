import os

# Hint: run file with following command
# python -i send_requests.py 
# use cmd(function()) to run requests

port = 9091
adress = "http://localhost:" + str(port) + "/"

def cmd(text):
    os.system(text)

def trueOrFalse():
    value = int(input("Please input 1 or 0\t"))
    if value == 0 or value == 1:
        return str(value)

    return "0"

def ready():
    return "curl " + adress + "/ready"

def setUserPreferences():
    # "/user/:val1/:val2"
    roomTemp = str(int(input("Room temp = ")))
    blindsValue = str(int(input("Blinds value = ")))

    userPref = "/user/" + roomTemp + "/" + blindsValue
    return "curl -XPOST " + adress + userPref

def getUserPreferences():
    return "curl " + adress + "/user"

def windowStatus():
    return "curl " + adress + "/windowStatus"

def setWindowStatus():
    windowLvl = str(int(input("Windows level = ")))
    return "curl -XPOST" + adress + "/windowStatus/" + windowLvl

def blindsStatus():
    return "curl " + adress + "/blindsStatus"

def setBlindsStatus():
    blindsLvl = str(int(input("Windows level = ")))
    return "curl -XPOST" + adress + "/blindsStatus/" + blindsLvl()

def autoWindow():
    return "curl -XPOST " + adress + "/auto/windowStatus/" + trueOrFalse()

def autoBlinds():
    return "curl -XPOST " + adress + "/auto/blindsStatus/" + trueOrFalse()

def setAlarm():
    seconds = str(int(input("Enter amount of seconds = ")))
    percentage = str(int(input("Enter blinds percentage = ")))
    return "curl -XPOST " + adress + "/alarm/" + seconds + "/" + percentage

def toggleAlarm():
    return "curl -XPOST " + adress + "/alarm/isOn/" + trueOrFalse()


