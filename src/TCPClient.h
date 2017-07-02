/**
 * Header file for the TCPClient class
 * @file TCPClient.h
 */

#ifndef TCPClient_H
#define TCPClient_H

#include <stdint.h>
#include "IPv6Packet.h"
#include "Socket.h"
#include "tcp.h"

//the periodic time out expressed in ms
#define PERIODIC_TIME_OUT 250

/**
 * Class for open a TCP connection to a client
 *
 */
class TCPClient : public Socket {

public:

    /**
     * Construct a TCP client, with a listening port defined
     *
     * @param ether The Ethernet interface to attach the client to
     */
    TCPClient(EtherSia &ether);

    /**
     * Check if a TCP data packet is available for this client
     *
     * This method also has a side effect of responding to other stages
     * of the TCP sequence (the SYN and FIN packets).
     *
     * havePacket() always fixes the flags (control bits) when preparing a packet for sending
     * @return true if datas are received from the server
     */
    boolean havePacket();

    /**
     * Get a pointer to the TCP payload of the last received packet
     *
     * @note Please call havePacket() first, before calling this method.
     * @return A pointer to the payload
     */
    virtual uint8_t* payload();

    /**
     * Get the length (in bytes) of the last received TCP packet payload
     *
     * @note Please call havePacket() first, before calling this method.
     * @return A pointer to the payload
     */
    virtual uint16_t payloadLength();

    /**
     * Get a pointer to the next TCP packet payload to be sent
     *
     * @return A pointer to the transmit payload buffer
     */
    virtual uint8_t* transmitPayload();

    /**
     * Open a new connection to the remote host.
     */
    void connect();

    /**
     * Check if client has successfully connected to the server
     *
     * @return True if the client is connected
     */
    boolean connected(){return ((_state & TCP_STATE_MASK) == TCP_STATE_CONNECTED);}

    /**
     * check if connection has been synacked
     */
    boolean synacked(){return (_appliFlags & UIP_CONNECTED);}

    /**
     * check if a segment containing datas has to be resent
     */
    boolean rexmit(){return (_appliFlags & UIP_REXMIT);}

    /**
     * Check if the connection has timedout
     */
    boolean timedout(){return (_state & TCP_STATE_TIMEDOUT);}
	
    /**
     * Check if there is activity for application information
     */
    boolean life(){return !(_appliFlags == 0);}
	
    /**
     * check if the connection has been aborted
     */
    boolean aborted(){return (_appliFlags & UIP_ABORT);}

    /**
     * check if the connection has been closed/is closing in a nice way
     */
    boolean closing(){return (_appliFlags & UIP_CLOSE);}

    /**
     * check if the arduino needs to reset
     */
    boolean reset(){return (_appliFlags & UIP_RESET);}

    /**
     * Print the TCP state to a stream
     */
    void printState(Print &print=Serial) const;

    /**
     * FOR STATISTICS PURPOSE ONLY
     * return the number of retransmitted packets
     */
    uint16_t numrexmitSynAck(){return _nrexmitSynAck;}
    uint16_t numrexmitData(){return _nrexmitData;}
    uint16_t numrexmitFinAck(){return _nrexmitFinAck;}
    uint16_t numrcvd(){return _nrcvd;}
    uint16_t numdropped(){return _ndropped;}

protected:

    /** the TCP states*/
    enum {
        TCP_STATE_DISCONNECTED = 0x00,//0000
        TCP_STATE_WAIT_SYN_ACK = 0x01,//0001
        TCP_STATE_CONNECTED = 0x03,//   0011
        TCP_STATE_LAST_ACK = 0x08,//    1000
        TCP_STATE_MASK = 0x0f,//        1111
        TCP_STATE_TIMEDOUT = 0x10,//   10000
    };

    /** enumeration of application activity flags*/
    enum {
        UIP_ACKDATA = 1,//         1
        UIP_NEWDATA = 2,//        10
        UIP_REXMIT = 8,//       1000
        UIP_CLOSE = 16,//      10000
        UIP_ABORT = 32,//     100000
        UIP_CONNECTED = 64,//1000000
        UIP_RESET = 128,//  10000000
    };
	
	/** enumeration of timer parameters*/
    enum {
        UIP_RTO = 3,//the initial retransmission timer - stay at a low value
        UIP_MAXRTX = 8,//the max number of times a non SYN segment should be sent before connection abortion
        UIP_MAXSYNRTX = 5,//the max number of times a SYN segment should be sent before connection abortion
    };
	
    /**
     * sets all numbers (ports, sequence) in TCP header 
     * sets window size, checksum, urgent pointer and options if any
     * @note dataOffset should have been set before any call to this methodIn havePacket()It 
     * @note fixes the protocol in the IP header - does not fix payloadLength in the IP header
     */
	//basicSend();
    void fillTCPHeader(); 

    /**
     * Protocol specific function that is called by send(), sendReply() etc.
     *
     * @param length The length (in bytes) of the data to send
     * @param isReply Set to true if this packet is a reply to an incoming packet
     */
    virtual void sendInternal(uint16_t length, boolean isReply);

    uint32_t _remoteSeqNum;
    uint32_t _localSeqNum;

    uint8_t _state;
    uint8_t _appliFlags;
    uint16_t _unAckLen;

    //the 2 periodic timers
    uint32_t _inactivity;
    uint32_t _periodic;

    //the retransmission timer
    uint8_t _timer;
    //the retransmit time out
    uint8_t _rto;
    uint8_t _sa;
    uint8_t _sv;

    // the number of retransmissions for the last segment sent...
    uint8_t _nrtx;

    //for stats purposes
    //incoming packets on the interface
    uint16_t _nrcvd;
    uint16_t _ndropped;
    //counts the number of retransmitted packets
    uint16_t _nrexmitSynAck;
    uint16_t _nrexmitData;
    uint16_t _nrexmitFinAck;

};


#endif
