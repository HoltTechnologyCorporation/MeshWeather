#include <ArduinoJson.h>
#include <easyMesh.h>
#include <limits.h>
#include "ESP8266WiFi.h"

#define   MESH_PREFIX     "meshNet"
#define   MESH_PASSWORD   "meshPassword"
#define   MESH_PORT       5555
#define   SERVER_SSID     "serverssid"
#define   RSSI_THRESHOLD  50 
#define   SERVER_ID       -1
#define   SERVER_IP       
#define   SYNCINTERVAL    10000000 //1800000000
#define   SERVER_PORT     
#define   MAX_SIZE        512
#define   DISCOVERY_REQ   0
#define   DATA            1

int tempPin = 2;
easyMesh mesh;
uint32_t lastSyncTime = 0; //Used to guarantee the route consistency
char msgString[MAX_SIZE];
uint32_t delayTime = 5000; //16000000; //1000000;
uint32_t totTime = 0;
uint32_t nextHopId = 0;
int update = 0;
int lastPId[30];
uint32_t lastCId[30];
short packetSendNumber = 0;
int lastInserted = 0;

int alreadySent(int id, uint32_t from){
  int i;
  for(i=0; i<30; i++){
    if(lastPId[i] == id && lastCId[i] == from)
      return 1;
  }
  return 0;
}

void addSentMessage(int id, uint32_t from){
  lastInserted=(lastInserted+1)%30;
  lastPId[lastInserted]=id;
  lastCId[lastInserted]=from;
}

void propagateDiscovery(JsonObject& m){
  char msg[256];
  sprintf(msg, "{\"from\": %du, \"update_number\": %d, \"sender_id\": %du, \"type\": 0}", (uint32_t) m["from"], m["update_number"], mesh.getChipId());
  String p(msg);
  mesh.sendBroadcast(p);
  return;
}

void propagateData(String& msg_str, uint32_t from, int id ){
  if(nextHopId != 0)
    mesh.sendSingle(nextHopId, msg_str);
  else
    mesh.sendBroadcast(msg_str);
  addSentMessage(id, from);
  return;
}

void receivedCallback( uint32_t from, String &msg_str ){
  if(from == 1291812) /* IN DEMO MODE, IGNORE MESSAGE FROM SERVER TO SIMULATE DISTANCE*/
    return;
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& msg = jsonBuffer.parseObject(msg_str);
  int type = msg["type"];
  switch(type){  
    case(DISCOVERY_REQ):{
        if(msg["update_number"] > update){
          update = msg["update_number"];
          if(update == INT_MAX)
            /*Prevent overflow*/
            update = 0;
          nextHopId = msg["sender_id"];
          propagateDiscovery(msg);
          lastSyncTime = mesh.getNodeTime();
        }
    }break;
    case(DATA):{
      if(!alreadySent(msg["id"], msg["from"]))
        propagateData(msg_str, msg["from"], msg["id"]);
    }break;
    default:{}break;
  }
}

float readTemp(){ 
  float millivolts = analogRead(tempPin)/1023.0*5000;
  float celsius = millivolts/10; 
  return celsius; /* return 23.57; */ 
}

void newConnectionCallback( bool adopt ){}

void setup(){
  mesh.init( MESH_PREFIX, MESH_PASSWORD, MESH_PORT );
  mesh.setReceiveCallback(&receivedCallback);
  mesh.setDebugMsgTypes(ERROR); //| MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE );
  mesh.setNewConnectionCallback( &newConnectionCallback );
  Serial.begin(115200);
}

void loop(){
  mesh.update();
  /*Send the temperature data to the next hop or broadcasts it*/
  char msg[256];
  /*Prevent overflow*/
  sprintf(msg, "{\"from\": %d, \"id\": %d, \"temp\": 26.4, \"buttonDown\": %d, \"type\": 1}", mesh.getChipId(), packetSendNumber++, digitalRead(tempPin));
  String p(msg);
 
  totTime = (mesh.getNodeTime()-lastSyncTime)+delayTime*1000;
  if(totTime>SYNCINTERVAL)
    nextHopId = 0;
  if(nextHopId == 0)
    mesh.sendBroadcast(p);
  else
    mesh.sendSingle(nextHopId, p);
  addSentMessage(packetSendNumber-1, mesh.getChipId()); 
  //delay((long) delayTime);
  delay(delayTime);
}


