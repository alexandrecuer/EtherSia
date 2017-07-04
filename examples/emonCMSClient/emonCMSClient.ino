/**
 * EmonCMS TCP Client example
 * to be used on a local/private network
 * tested with arduino Uno/Nano and emonCMS sofware from SD release "emonSD-07Nov16"
 * http://files.openenergymonitor.org/emonSD-07Nov16.zip
 * https://github.com/openenergymonitor/emonpi/wiki/emonSD-pre-built-SD-card-Download-&-Change-Log
 * Alexandre CUER 14/03/2017
 */

#include <Arduino.h>
#include "ApplicationMonitor.h"
 
Watchdog::CApplicationMonitor ApplicationMonitor;

// number of iterations completed. 
int g_nIterations = 0;     

#include <EtherSia.h>
#include <dynaBus.h>

const uint8_t ONE_WIRE_PIN = 2;
dynaBus ds(ONE_WIRE_PIN);

static unsigned long lastConnect = 0xffff0000;
unsigned long interval=30000L;
boolean remoteHostDiscover = false;
uint16_t nbtimeout=0;

char emonapikey[] = "your_32_bits_API_key";
uint16_t emonport=80;
const char *emonip="fe80::ba27:ebff:fe84:c0a1";//OFFICE
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
      Serial.println(F("Failed to get a global addr"));
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

    //search for OneWire devices
    ds.begin();
    ds.find();
    Serial.print(ds.nb());Serial.println(F(" 1wire dev."));
    /**
     * single wire 64-bit ROM : first byte is family
     * bytes 1 to 6 represent the unique serial code
     * byte 7 contain the CRC
     * * * * * * * * * * * 
     * in our specific case, we don't use byte 1 to generate the MAC and we use a fixed param 0x6e
     * this permits to have a local unicast MAC 
     */
    if(ds.nb()) macAddress.fromBytes(0x6e,ds[2],ds[3],ds[4],ds[5],ds[6]);
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

    //Neighbor Unreachability Detection - we reset
    if(!remoteHostDiscover){
      Serial.println(F("N.U.D"));
      while(1);
    }
    
    ether.receivePacket();
    
    uint16_t rcvdBytes=tcp.payloadLength();
    
    if(tcp.havePacket()){
      Serial.print(F("["));Serial.print(rcvdBytes);Serial.print(F(" bytes received]"));
      uint8_t *buf=tcp.payload();
      buf[rcvdBytes]='\0';
      Serial.print(F("[Srv said:"));
      Serial.print((char*)(&(buf[0])));
      Serial.println(F("]"));
    }

    if(tcp.reset()){
      while(1);
    }

    if(tcp.life())tcp.printState();
    
    if(tcp.aborted())Serial.println(F("conn.aborted"));
    
    if(tcp.closing())Serial.println(F("closing"));

    if(tcp.timedout()){Serial.println(F("timed out"));nbtimeout++;}
    
    /** neighbour discovery is successfull
     *  we are not connected and it is time to !
     */
    if (remoteHostDiscover && !tcp.connected() && (long)(millis() - lastConnect) >= interval) { 
      lastConnect=millis();
      Serial.println();Serial.println();Serial.print(F("Connecting srv from lport : "));
      tcp.connect();
      Serial.println(tcp.localPort());
      Serial.println(lastConnect);
    }
    
    /**if we are connected, we have 2 cases : 
     * 1 - the connection has just been synacked and we haven't sent any request to the server in that connection
     * 2 - we have to resend the request because of a timeout
     */
     if(tcp.synacked() || tcp.rexmit()){
        // 1 - DATA PREPARATION
        tcp.print("GET /emoncms/input/post.json?");
        tcp.print("node=");
        if(ds.nb())for (byte i=0; i<7; i++){
          tcp.print(ds[i],HEX);
          if(i<6)tcp.print("_");
        }
        else tcp.print("No1wireBus");
        tcp.print("&json={");
        for(byte i=0;i<ds.nb();i++){
          switch (ds[i*8]){
          case 0x28 :
            tcp.print("T28_");
            tcp.print(i);
            tcp.print(":");
            tcp.print(ds.get28temperature(i));
            break;
          case 0x26 :
            float celsius2438 = ds.get26temperature(i);
            float vdd = ds.get26voltage(i,"vdd");
            float vad = ds.get26voltage(i,"vad");
            float rh = (vad/vdd - 0.16)/0.0062;
            float truerh = rh/(1.0546-0.00216*celsius2438);
            tcp.print("T26_");
            tcp.print(i);
            tcp.print(":");
            tcp.print(celsius2438);
            tcp.print(",RH26_");
            tcp.print(i);
            tcp.print(":");
            tcp.print(truerh);
          }
          tcp.print(",");
        }
        tcp.print("LPort:");
        tcp.print(tcp.localPort());
        tcp.print(",");
        tcp.print("nbTimeOut:");tcp.print(nbtimeout);tcp.print(",");
        tcp.print("RcvPkts:");tcp.print(tcp.numrcvd());tcp.print(",");
        tcp.print("Dropped:");tcp.print(tcp.numdropped());tcp.print(",");
        tcp.print("ReSynAck:");tcp.print(tcp.numrexmitSynAck());tcp.print(",");
        tcp.print("ReData:");tcp.print(tcp.numrexmitData());tcp.print(",");
        tcp.print("ReFinAck:");tcp.print(tcp.numrexmitFinAck());tcp.print(",");

        tcp.print("_millis:");
        tcp.print(lastConnect);
        tcp.print("}&apikey=");
        tcp.println(emonapikey);
        
        // 2 - DATA POST
        tcp.send();
        Serial.print(F("full outgoing buffer size : "));Serial.print(ether.packet().length());Serial.println(F(" bytes"));
    }

}
