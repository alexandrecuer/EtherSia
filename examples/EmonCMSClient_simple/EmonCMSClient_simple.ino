/**
 * EmonCMS TCP Client example
 * to be used on a local/private network
 * tested with arduino Uno/Nano and emonCMS sofware from SD release "emonSD-07Nov16"
 * http://files.openenergymonitor.org/emonSD-07Nov16.zip
 * https://github.com/openenergymonitor/emonpi/wiki/emonSD-pre-built-SD-card-Download-&-Change-Log
 * Alexandre CUER June 2017
 * Application monitor is from Megunolink - https://github.com/Megunolink/ArduinoCrashMonitor
 */

#include <Arduino.h>
#include "ApplicationMonitor.h"
 
Watchdog::CApplicationMonitor ApplicationMonitor;

// number of iterations completed. 
int g_nIterations = 0;     

#include <EtherSia.h>

static unsigned long lastConnect = 0xffff0000;
unsigned long interval=30000L;
boolean remoteHostDiscover = false;
uint16_t nbtimeout=0;

char emonapikey[] = "Your_32_bits_API_key";
uint16_t emonport=80;
const char *emonip="fe80::ba27:ebff:fede:64b3";//HOME
//const char *emonip="fe80::ba27:ebff:fe84:c0a1";//OFFICE
//you can find the ipv6 address of your local emoncms server via the shell command ifconfig eth0

/** Ethernet Interface */
EtherSia_ENC28J60 ether;

/** Define TCP socket to send messages from */
TCPClient tcp(ether);
MACAddress macAddress("6e:e7:2f:4c:64:92");

/** interface initilization */
void interfaceConfig(){
    Serial.println(F("[EtherSia EMONclient]"));
    macAddress.println();
    if (ether.begin(macAddress) == false) {
      Serial.println(F("Failed to get a global address"));
    }
    Serial.print(F("Our link-local address : "));
    ether.linkLocalAddress().println(); 
}

/** remoteHost configuration */
void configRemoteHost(){
    if (remoteHostDiscover=tcp.setRemoteAddress(emonip, emonport)) {
       Serial.print(F("Srv IPv6 addr : "));
       tcp.remoteAddress().println();
    }
    else {
      Serial.println(F("err. conn. srv"));
    }
}

void setup() {
    
    // Setup serial port
    Serial.begin(115200);
    
    //we disable autoconfiguration as we only want to use link-local addressing
    ether.disableAutoconfiguration();

    delay(5000);
    // Start Ethernet on the device - global and local link
    interfaceConfig();

    // Remote server configuration
    configRemoteHost();

    ApplicationMonitor.Dump(Serial);
    ApplicationMonitor.EnableWatchdog(Watchdog::CApplicationMonitor::Timeout_8s);
}

void loop()
{

    ApplicationMonitor.IAmAlive();
    ApplicationMonitor.SetData(g_nIterations++);

    //Neighbor Unreachability Detection - we reset the arduino
    if(!remoteHostDiscover){
      Serial.println(F("N.U.D"));
      while(1);
    }
    
    ether.receivePacket();
    
    uint16_t rcvdBytes=tcp.payloadLength();
    
    if(tcp.havePacket()){
      Serial.print(F("["));Serial.print(rcvdBytes);Serial.print(F(" bytes rcv]"));
      uint8_t *buf=tcp.payload();
      buf[rcvdBytes]='\0';
      Serial.print(F("[Srv said :"));
      Serial.print((char*)(&(buf[0])));
      Serial.println(F("]"));
    }

    if(tcp.reset()){
      while(1);
    }

    if(tcp.life())tcp.printState();
    
    if(tcp.aborted())Serial.println(F("conn.aborted"));
    
    if(tcp.closing()){Serial.println(F("closing"));}

    if(tcp.timedout()){Serial.println(F("timed out"));nbtimeout++;}
    
    /** neighbour discovery is successfull
     *  we are not connected and it is time to !
     */
    if (remoteHostDiscover && !tcp.connected() && (long)(millis() - lastConnect) >= interval) { 
      lastConnect=millis();
      Serial.println();Serial.println();Serial.print(F("Conn. from lport : "));
      tcp.connect();
      Serial.println(tcp.localPort());
      Serial.println(lastConnect);
    }
    
    /**if we are connected, we have 2 cases : 
     * 1 - the connection has just been synacked and we haven't sent any request to the server in that connection
     * 2 - we have to resend the request because of a periodic timeout
     */
     if(tcp.synacked() || tcp.rexmit()){
        // 1 - DATA PREPARATION
        tcp.print("GET /emoncms/input/post.json?");
        tcp.print("node=");
        tcp.print("No1");
        tcp.print("&json={");
        tcp.print("LPort:");
        tcp.print(tcp.localPort());
        tcp.print("_millis:");
        tcp.print(lastConnect);
        tcp.print("}&apikey=");
        tcp.println(emonapikey);
        
        // 2 - DATA POST
        tcp.send();
        
    }

}
