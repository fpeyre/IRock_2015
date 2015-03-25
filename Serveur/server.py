#IRock 2015
#Author Flavien Peyre (peyre.flavien@gmail.com)
#scriptto execute on the serv to send data to InfluxDB/ Excel file. Condition are ready to implement localisation programm

#!/usr/bin/env python
# -*- coding: utf-8 -*-
import sys
import paho.mqtt.client as mqtt
import struct
import xlrd
from xlwt import Workbook
from xlutils.copy import copy
import json
import requests
import time

#function for write in a excel file
def writeExcel (value,excelFile,numberC,numberR):
    rb = xlrd.open_workbook(excelFile,formatting_info=True)
    r_sheet = rb.sheet_by_index(0) # read only copy to introspect the file
    wb = copy(rb) # a writable copy (I can't read values out of this, only write to it)
    w_sheet = wb.get_sheet(0) # the sheet to write to within the writable copy
     
    
    w_sheet.write(numberR+5, numberC, value)
    wb.save(excelFile)
    
#connection to thee local mosquitto
def on_connect(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))

    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    client.subscribe("IRock/Weather/#",0)
    client.subscribe("IRock/F3/#", 0)
    client.subscribe("IRock/F4/#", 0)

# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
    #Creation of a list of sensor (one list per type of board)
    
    
    msgPars=msg.topic.split("/")
    if msgPars[1]=="Weather":
        # Check if the sensor is inside the list. If not, we create a new Excel files 
        if msgPars[2] not in listWeather:
            dictComptWeather[str(msgPars[2])]=0
            listWeather.append(msgPars[2])
            book=Workbook()
            sheet=book.add_sheet('Resultat')
            sheet.write(0,0,'Carte')
            sheet.write(0,1,'Weather')
            sheet.write(1,0,'ID')
            sheet.write(1,1,msgPars[2])
          
            sheet.write(3,0,'Valeurs')
            sheet.write(5,0,'Timestamp (s)')
            sheet.write(5,1,'Direction Vent (degre)')
            sheet.write(5,2,'Vitesse Vent (mph)')
            sheet.write(5,3,'Humidite (pourcent)')
            sheet.write(5,4,'temperature (F)')
            sheet.write(5,5,'Pluviometrie')
            sheet.write(5,6,'Pression')
            sheet.write(5,7,'Batterie')

            book.save ('./Excel/Weather/'+str(msgPars[2])+'.xls')

        # Parsing of msg, write in InfluxDB and Excel
        val=dictComptWeather[str(msgPars[2])]
        dictComptWeather[str(msgPars[2])]=val+1
        listPars=msg.payload.split("_")
        excelFile='./Excel/Weather/'+str(msgPars[2])+'.xls'

        

        writeExcel (listPars[0],excelFile,0,val+1)
        writeExcel (listPars[1],excelFile,1,val+1)
        writeExcel (listPars[4],excelFile,2,val+1)
        writeExcel (listPars[2],excelFile,3,val+1)
        writeExcel (listPars[3],excelFile,4,val+1)
        writeExcel (listPars[5],excelFile,5,val+1)
        writeExcel (listPars[7],excelFile,6,val+1)
        writeExcel (listPars[6],excelFile,7,val+1)
        print type(listPars[6])
        ts=float(listPars[0])*1000
        print ts
        print type(ts)
        v = [{"name": "Weather", "columns": ["time","sequence_number","Direction du vent","Vitesse vent","humidite","Temperature","Pluviometrie","Pression","Batterie"],"points": [[ts, int(msgPars[2]),int(listPars[1]),int(listPars[4]),int(listPars[2]),int(listPars[3]),int(listPars[5]),int(listPars[7]),int(listPars[6])]]}]
        r = requests.post('http://localhost:8086/db/%s/series?u=root&p=root' % DATABASE, data=json.dumps(v))
        if r.status_code != 200:
            print r.status_code
            print 'Failed to add point to influxdb -- aborting.'
            sys.exit(1) 
        
        
        
            
        
        
    elif msgPars[1]=="F3":
         # Check if the sensor is inside the list. If not, we create a new Excel files and a database.We add a list for check the rssi
        if msgPars[2] not in listF3:
            dictComptF3[str(msgPars[2])]=0
            listF3.append(msgPars[2])
            book=Workbook()
            sheet=book.add_sheet('Resultat')

            sheet.write(0,0,'Carte')
            sheet.write(0,1,'F3')
            sheet.write(1,0,'ID')
            sheet.write(1,1,msgPars[2])           
            sheet.write(3,0,'Valeurs')
            sheet.write(5,0,'Timestamp (s)')
            sheet.write(5,1,'Magnetometre (degre)')
            sheet.write(3,3,'Position')
            sheet.write(5,3,'Timestamp')
            sheet.write(5,4,'Longitude')
            sheet.write(5,5,'Latitude')            

            book.save ('./Excel/F3/'+str(msgPars[2])+'.xls')

            dictionnaryRSSIF3S1[str(msgPars[2])]=0
            dictionnaryRSSIF3S2[str(msgPars[2])]=0
            dictionnaryRSSIF3S3[str(msgPars[2])]=0
            
        val=dictComptF3[str(msgPars[2])]
        dictComptF3[str(msgPars[2])]=val+1
        listPars=msg.payload.split("_")
        excelFile='./Excel/F3/'+str(msgPars[2])+'.xls'    
        writeExcel (listPars[0],excelFile,0,val+1)
        writeExcel (listPars[1],excelFile,1,val+1)
        ts=float(listPars[0])*1000
        print ts
        print type(ts)
        print type(listPars[1])
        v = [{"name": "F3", "columns": ["time","sequence_number","Magnetometre"],"points": [[ts,int(msgPars[2]),int(listPars[1])]]}]
        r = requests.post('http://localhost:8086/db/%s/series?u=root&p=root' % DATABASE, data=json.dumps(v))
        if r.status_code != 200:
            print r.status_code
            print 'Failed to add point to influxdb -- aborting.'
            sys.exit(1) 
        
        # now we put the rssi value give by relay inside a dictionnary               
        if listPars[3]==1:
            dictionnaryRSSIF3S1[str(msgPars[2])]=listPars[4]
        elif listPars[3]==2:
            dictionnaryRSSIF3S2[str(msgPars[2])]=listPars[4]
        elif listPars[3]==3:
            dictionnaryRSSIF3S3[str(msgPars[2])]=listPars[4]
        #if we have all rssi value (one per relay), we calcul the sensor position
        if (dictionnaryRSSIF3S1[str(msgPars[2])]!=0) and (dictionnaryRSSIF3S2[str(msgPars[2])]!=0) and (dictionnaryRSSIF3S3[str(msgPars[2])]!=0):
            #fonction calcul PH

            #write timestamp
            writeExcel (time.time(),excelFile,3,val+1)
            #write longitude
            longitude=0
            latitude=0
            #write latitude
            ts=float(listPars[0])*1000
            v = [{'name': 'F3Pos','columns': ['time','sequence_number','Longitude','Latitude'],\
                            'points': [[ts,int(msgPars[2]),int(longitude),int(latitude)]]}]
            r = requests.post('http://localhost:8086/db/%s/series?u=root&p=root' % DATABASE, data=json.dumps(v))
            if r.status_code != 200:
                print r.status_code
                print 'Failed to add point to influxdb -- aborting.'
                sys.exit(1) 
            
            dictionnaryRSSIF3S1[str(msgPars[2])]=0
            dictionnaryRSSIF3S2[str(msgPars[2])]=0
            dictionnaryRSSIF3S3[str(msgPars[2])]=0
            
                

                    
    elif msgPars[1]=="F4":
        # Check if the sensor is inside the list. If not, we create a new Excel files and a database.We add a list for check the rssi
        if msgPars[2] not in listF4:
            dictComptF4[str(msgPars[2])]=0
            listF4.append(msgPars[2])
            book=Workbook()
            sheet=book.add_sheet('Resultat')
            print('init feuille')
            sheet.write(0,0,'Carte')
            sheet.write(0,1,'F4')
            sheet.write(1,0,'ID')
            sheet.write(1,1,msgPars[2])
           
            sheet.write(3,0,'Valeurs')
            sheet.write(5,0,'Timestamp (s)')
            sheet.write(5,1,'Acc X')
            sheet.write(5,2,'Acc Y')
            sheet.write(5,3,'Acc Z')
            sheet.write(3,7,'Position')
            sheet.write(5,7,'Timestamp')
            sheet.write(5,8,'Longitude')
            sheet.write(5,9,'Latitude') 

            book.save ('./Excel/F4/'+msgPars[2]+'.xls')

            dictionnaryRSSIF4S1[str(msgPars[2])]=0
            dictionnaryRSSIF4S2[str(msgPars[2])]=0
            dictionnaryRSSIF4S3[str(msgPars[2])]=0

            
        val=dictComptF4[str(msgPars[2])]
        dictComptF4[str(msgPars[2])]=val+1
        print('add value ligne')
        print(val)
        
        listPars=msg.payload.split("_")
        excelFile='./Excel/F4/'+str(msgPars[2])+'.xls'    
        writeExcel (listPars[0],excelFile,0,val+1)
        writeExcel (listPars[1],excelFile,1,val+1)
        writeExcel (listPars[2],excelFile,2,val+1)
        writeExcel (listPars[3],excelFile,3,val+1)
        ts=float(listPars[0])*1000
        print ts
        print type(ts)
        v = [{"name": "F4","columns": ["time","sequence_number","X","Y", "Z"],"points": [[ts, int(msgPars[2]),int(listPars[1]),int(listPars[2]),int(listPars[3])]]}]
        r = requests.post('http://localhost:8086/db/%s/series?u=root&p=root' % DATABASE, data=json.dumps(v))
        if r.status_code != 200:
            print r.status_code
            print 'Failed to add point to influxdb -- aborting.'
            sys.exit(1)
        # now we put the rssi value give by relay inside a dictionnary               
        if listPars[3]==1:
            dictionnaryRSSIF4S1[str(msgPars[2])]=listPars[4]
        elif listPars[3]==2:
            dictionnaryRSSIF4S2[str(msgPars[2])]=listPars[4]
        elif listPars[3]==3:
            dictionnaryRSSIF4S3[str(msgPars[2])]=listPars[4]
        #if we have all rssi value (one per relay), we calcul the sensor position
        if (dictionnaryRSSIF4S1[str(msgPars[2])]!=0) and (dictionnaryRSSIF4S2[str(msgPars[2])]!=0) and (dictionnaryRSSIF4S3[str(msgPars[2])]!=0):
            #fonction calcul PH

            #write timestamp
            writeExcel (time.time(),excelFile,7,val+1)
           
            #write longitude
            longitude=0
            latitude=0
            #write latitude
            ts=float(listPars[0])*1000

            v = [{'name': 'F4Pos','columns': ['time','sequence_number','Longitude','Latitude'],\
                            'points': [[ts,msgPars[2],longitude,latitude]]}]
            r = requests.post('http://localhost:8086/db/%s/series?u=root&p=root' % DATABASE, data=json.dumps(v))
            if r.status_code != 200:
                print r.status_code
                print 'Failed to add point to influxdb -- aborting.'
                sys.exit(1)
                
            dictionnaryRSSIF4S1[str(msgPars[2])]=0
            dictionnaryRSSIF4S2[str(msgPars[2])]=0
            dictionnaryRSSIF4S3[str(msgPars[2])]=0








                                      
#For mqtt
broker="localhost"
port=1883
mqttc = mqtt.Client()
mqttc.on_connect=on_connect
mqttc.on_message= on_message

listWeather=[]
listF3=[]
listF4=[]
dictComptWeather={}
dictComptF3={}
dictComptF4={}
    
dictionnaryRSSIF3S1={}
dictionnaryRSSIF3S2={}
dictionnaryRSSIF3S3={}
dictionnaryRSSIF4S1={}
dictionnaryRSSIF4S2={}
dictionnaryRSSIF4S3={}

DATABASE='test'

mqttc.connect(broker,port,60)

mqttc.loop_forever()



