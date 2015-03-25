#IRock 2015
#Author Flavien Peyre (peyre.flavien@gmail.com)
#Description : Receive data from mbed by serial port and send to the server via MQTT
#Param: broker: ip of mosquitto serv, ser:Serial port on receptor is connect


import sys
import paho.mqtt.client as mqtt
import serial
import struct
import time

#For mqtt
broker="52.17.62.127"
port=1883
mqttc = mqtt.Client()
#connect to broker
mqttc.connect(broker,port,60)
ser = serial.Serial('/dev/ttyACM0',9600)
while True:
    data = ser.readline()
    print data
    listPars=data.split("/")

    # On recoit une weather
    if int(listPars[0])==1:
        # send timestamp_winddir_humi_temp_windspd_rain_bat_pres
        mqttc.publish("IRock/Weather/"+listPars[1],time.time()+'_'+str(listPars[2])+'_'+str(listPars[3])+'_'+str(listPars[4])+'_'+str(listPars[5])+'_'+str(listPars[6])+'_'+str(listPars[7])+'_'+str(listPars[8]))
        
        
         

    # On recoit un cailloux de type F3
    elif int(listPars[0])==2:
        #send timestamp_direction_idStation_RSSI
        mqttc.publish("IRock/F3/"+listPars[1],time.time()+'_'+str(listPars[2])+'_'+str(listPars[3])+'_'+str(listPars[4]))
       
        

        

    #On recoit un cailloux de type F4
    elif int(listPars[0])==3:
        #send timestamp_x_y_z_idstation_RSSI
        mqttc.publish("IRock/F4/"+listPars[1],time.time()+'_'+listPars[2]+'_'+listPars[3]+'_'+listPars[4]+'_'+listPars[5]+'_'+listPars[6])       
      
    
    
