#include <ArduinoJson.h>
#include <easyMesh.h>
#include <limits.h>
#include <string> 
#include "ESP8266WiFi.h"

#define   MESH_PREFIX     "meshNet"
#define   MESH_PASSWORD   "meshPassword"
#define   MESH_PORT       5555
#define   SERVER_SSID     "serverssid"
#define   RSSI_THRESHOLD  50 
#define   SERVER_ID       -1
#define   MAX_SIZE        512
#define   DISCOVERY_REQ   0
#define   DATA            1
#define   SYNCINTERVAL    10000000 //1800000000

easyMesh mesh;
char msgString[MAX_SIZE];
uint32_t nextHopId = 0;
uint32_t lastSyncTime = 0;
int update = 0;
int lastPId[30];
uint32_t lastCId[30];
uint32_t myId;

int alreadySent(int id, uint32_t from){
  int i;
  for(i=0; i<30; i++)
    if(lastPId[i] == id && lastCId[i] == from)
      return 1;
  return 0;
}
int lastInserted = 0;
void addSentMessage(int id, uint32_t from){
  lastInserted=(lastInserted+1)%30;
  lastPId[lastInserted]=id;
  lastCId[lastInserted]=from;
}

void propagateDiscovery(JsonObject& m){
  char msg[256];
  sprintf(msg, "{\"from\": %"PRIu32", \"update_number\": %d, \"sender_id\": %"PRIu32", \"type\": 0}", (uint32_t) m["from"], m["update_number"], myId);
  String p(msg);
  mesh.sendBroadcast(p);
  return;
}

int dataNumber = 0;
void propagateData(String& msg_str, uint32_t from, int id ){
  /*If the route is expired, nextHopId is set to -1*/
  if((mesh.getNodeTime()-lastSyncTime)>=SYNCINTERVAL)
    nextHopId = 0;
  String toSend = msg_str+String(++dataNumber)+" "+String(nextHopId);
  if(nextHopId != 0){
    mesh.sendSingle(nextHopId, msg_str);
  } 
  else
    mesh.sendBroadcast(msg_str);
  addSentMessage(id, from);
  return;
}

void receivedCallback( uint32_t from, String &msg_str ){
  StaticJsonBuffer<512> jsonBuffer;
  JsonObject& msg = jsonBuffer.parseObject(msg_str);
  int type = msg["type"];
  switch(type){  
    case(DISCOVERY_REQ):{
        if(msg["update_number"] > update){
  
          update = msg["update_number"];
          if(update == INT_MAX)
            /*Prevent overflow*/
            update = 0;
          nextHopId = from;
          propagateDiscovery(msg);
          lastSyncTime = mesh.getNodeTime();
        }
    }break;
    case(DATA):{
      if(!alreadySent(msg["id"], msg["from"])){
          propagateData(msg_str, msg["from"], msg["id"]);
      }
    }break;
    default:{}break;
  }
}

void newConnectionCallback( bool adopt ){}

void setup(){
  mesh.init( MESH_PREFIX, MESH_PASSWORD, MESH_PORT );
  myId = mesh.getChipId();
  mesh.setReceiveCallback(&receivedCallback);
  mesh.setDebugMsgTypes( ERROR); //| MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE );
  mesh.setNewConnectionCallback( &newConnectionCallback );
  Serial.begin(115200);
}

void loop() {
  mesh.update();
}
