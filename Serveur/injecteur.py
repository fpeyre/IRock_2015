#IRock 2015
#Author Flavien Peyre (peyre.flavien@gmail.com)
#Description : simulator of data send by IRock/ Relay
#Param: broker: ip of mosquitto serv

import sys
import paho.mqtt.client as mqtt
import serial
import struct
import time
import random

broker="localhost"
port=1883
mqttc = mqtt.Client()
#connect to broker
mqttc.connect(broker,port,60)

while True:
    carte=random.randint(1,3)

    if carte==1:
        mqttc.publish("IRock/Weather/"+ str(random.randint(1,10)),str(time.time())+'_'+str(random.randint(1,10))+'_'+str(random.randint(1,10))+'_'+str(random.randint(1,10))+'_'+str(random.randint(1,10))+'_'+str(random.randint(1,10))+'_'+str(random.randint(1,10))+'_'+str(random.randint(1,10)))
    elif carte==2:
        mqttc.publish("IRock/F3/"+str(random.randint(1,10)),str(time.time())+'_'+str(random.randint(1,10))+'_'+str(random.randint(1,3))+'_'+str(random.randint(1,10)))
    elif carte==3:
        mqttc.publish("IRock/F4/"+str(random.randint(1,10)),str(time.time())+'_'+str(random.randint(1,10))+'_'+str(random.randint(1,10))+'_'+str(random.randint(1,10))+'_'+str(random.randint(1,3))+'_'+str(random.randint(1,10)))

    time.sleep(60)
