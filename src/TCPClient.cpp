#include "EtherSia.h"
#include "util.h"

#define PRINT(...) //Serial.print(__VA_ARGS__)
#define PRINTLN(...) //Serial.println(__VA_ARGS__)

TCPClient::TCPClient(EtherSia &ether) : Socket(ether)
{
    _state = TCP_STATE_DISCONNECTED;
    _nrexmitSynAck=0;
    _nrexmitData=0;
    _nrexmitFinAck=0;
    _nrcvd=0;
    _ndropped=0;
}

//ACTIVE OPEN
void TCPClient::connect()
{
    IPv6Packet& packet = _ether.packet();
    struct tcp_header *tcpHeader = TCP_HEADER_PTR;

    // Initialise our sequence number to a random number
    _localSeqNum = random();

    _remoteSeqNum = 0;

    // Use a new local port number for new connections
    _localPort++;

    //_unAckLen is the number of unacknowledged sent bytes
    //TCP length of our SYN is 1 byte
    _unAckLen=1;

    //RTT related parameters
    _timer=_rto=UIP_RTO;
    //UIP uses 0 as initial value of _sa and 16 for _sv
    //we have tested 42/16 or 84/32 or 84/48 without the use of the periodic timer
    _sa=0;
    _sv=16;
    _nrtx=0;

    tcpHeader->flags = TCP_FLAG_SYN;

    _state = TCP_STATE_WAIT_SYN_ACK;
    _periodic = _inactivity = millis();

    send((uint16_t)0, false);

}

boolean TCPClient::havePacket()
{
    IPv6Packet& packet = _ether.packet();
    struct tcp_header *tcpHeader = TCP_HEADER_PTR;
    static uint32_t tmp32;

    //if _appliFlags has a non null value, we reset the _inactivity periodic timer
    if (_appliFlags) {
      _inactivity = millis();
    }
	
    _appliFlags = 0;
	
    /** _appliFlags hasn't changed for 10 minutes
     * we inform the application
     */
     if((long)(millis()-_inactivity) >= 600000){
         _appliFlags |= UIP_RESET;
     }

    //in disconnect mode we do not accept incoming packets, as we only realize active open
    if (_state == TCP_STATE_DISCONNECTED) goto drop;
    if (_state & TCP_STATE_TIMEDOUT) {_state = TCP_STATE_DISCONNECTED;goto drop;}

    /** we check if the periodic timer has fired
     * the periodic timer is set to fire on PERIODIC_TIME_OUT
     */
    if((long)(millis()-_periodic) >= PERIODIC_TIME_OUT){
      //we reset the timer
      _periodic = millis();
      //if we have outstanding datas, we decrease the retransmission timer
      if(_unAckLen>0){
          _timer--;
          PRINT(F("[sa:"));PRINT(_sa);
          PRINT(F("-sv:"));PRINT(_sv);
          PRINT(F("-rto:"));PRINT(_rto);
          PRINT(F("-timer:"));PRINT(_timer);
          PRINT(F("-nrtx:"));PRINT(_nrtx);PRINTLN(F("]"));
      }
    }

    if (!_ether.bufferContainsReceived()) {
        // No incoming packets to process
        goto check_for_retransmit;
    } else _nrcvd++;

    if (packet.destination() != _ether.linkLocalAddress() || (packet.source() != remoteAddress())) {
        // Wrong IP pair - packet is not for us or not from the good source
        // we drop the packet !!
        _ndropped++;
        goto check_for_retransmit;
    }

    if (packet.protocol() != IP6_PROTO_TCP) {
        // Wrong protocol
        // we drop the packet !!
        _ndropped++;
        goto check_for_retransmit;
    }

    if (ntohs(tcpHeader->sourcePort) != _remotePort) {
        //wrong incoming port
        // we drop the packet !!
        _ndropped++;
        goto check_for_retransmit;
    }

    if(_nrtx) {
        PRINT(F("[LPort:"));PRINT(_localPort);
        PRINT(F("-destPortRCV:"));PRINT(ntohs(tcpHeader->destinationPort));
        PRINT(F("-RSN:"));PRINT(_remoteSeqNum);
        PRINT(F("-LSN:"));PRINT(_localSeqNum);
        PRINT(F("-SNRCV:"));PRINT(ntohl(tcpHeader->sequenceNum));
        PRINT(F("-ACKSNRCV:"));PRINT(ntohl(tcpHeader->acknowledgementNum));
        PRINT(F("-flags:"));PRINT(tcpHeader->flags,BIN);
        PRINT(F("]"));
    }
	
    if (ntohs(tcpHeader->destinationPort) == _localPort) {
        //we've found a packet for us
        goto found;
    }

    /**
    at this stage, either the packet is a SYN for a new connection, either it is an old packet
    we can't accept a SYN for a new connection as we practise only ACTIVE OPEN, so we drop it
    in case of an old packet, we have to make a reset
    */
    if (tcpHeader->flags & TCP_FLAG_SYN) {
        _ndropped++;
        goto check_for_retransmit;
    }

    PRINTLN(F("[old pkt - send RST]"));
    reset :
        //we cannot send resets in response to resets and we must jump directly to drop (no jump to check_for_retransmit label please)
        if (tcpHeader->flags & TCP_FLAG_RST) goto drop;

        tcpHeader->flags = TCP_FLAG_RST | TCP_FLAG_ACK;
        
		//prepareReply() swaps ethernet source and destination mac and IP and fixes a fresh hopLimit
        _ether.prepareReply();
        packet.setProtocol(IP6_PROTO_TCP);
		
        //swap ports
		tcpHeader->sourcePort = tcpHeader->destinationPort;
		tcpHeader->destinationPort = _remotePort;

        //swap sequence numbers and add 1 to the sequence number we are acknowledging
        tmp32 = ntohl(tcpHeader->sequenceNum);
        tcpHeader->sequenceNum = tcpHeader->acknowledgementNum;
        tcpHeader->acknowledgementNum = htonl(tmp32+1);

        /**we don't send any data nor options in a reset
        just an IP payload consisting in a TCP header of 20 bytes
        */
        tcpHeader->dataOffset = 5 << 4;
        packet.setPayloadLength((tcpHeader->dataOffset & 0xF0)>>2);
		tcpHeader->window = htons(TCP_WINDOW_SIZE);
		tcpHeader->urgentPointer = 0;
        tcpHeader->checksum = 0;
        tcpHeader->checksum = htons(packet.calculateChecksum());
		_ether.send();
        goto drop;

    found :
        if (tcpHeader->flags & TCP_FLAG_RST) {
            _state=TCP_STATE_DISCONNECTED;
            _appliFlags=UIP_ABORT;
            PRINTLN(F("[RST rcv]"));
            goto drop;
        }

        /** in case the sequence number of the incoming packet is not what we're expecting
        we send an ACK with the correct numbers inside
        */
        if (!(_state == TCP_STATE_WAIT_SYN_ACK && (tcpHeader->flags & (TCP_FLAG_SYN|TCP_FLAG_ACK)))){
            if ((payloadLength()>0) || (tcpHeader->flags & TCP_FLAG_FIN)){
                 if (ntohl(tcpHeader->sequenceNum) != _remoteSeqNum) {
                    PRINTLN(F("[Unexpected incoming SN]"));
                    goto tcp_send_ack;
                }
            }
        }

        /**ACKING TEST
         is the packet acknowledging sent datas ?
        */
        if ((tcpHeader->flags & TCP_FLAG_ACK) && (_unAckLen >0)){
            if (ntohl(tcpHeader->acknowledgementNum) == (_localSeqNum + _unAckLen)){
                _localSeqNum += _unAckLen;
                /**This Ack coming in response to a sent packet constitutes a measurement we can use to update the RTT estimation
                RTT : round trip time
                The following code comes from the Van Jacobson's article on congestion avoidance
                */
                if (_nrtx == 0) {
                    signed char m;
                    m = _rto - _timer;
                    m = m - (_sa >> 3);
                    _sa += m;
                    if (m < 0) m = -m;
                    m = m - (_sv >> 2);
                    _sv += m;
                    _rto = (_sa >> 3) + _sv;
                }
                _timer = _rto;
                _unAckLen=0;
                _appliFlags=UIP_ACKDATA;
                PRINT(F("["));
                if(_nrtx == 0) PRINT(F("UPDATED"));
                PRINT(F("-sa:"));PRINT(_sa);
                PRINT(F("-sv:"));PRINT(_sv);
                PRINT(F("-rto:"));PRINT(_rto);
                PRINTLN(F("]"));
            }
        }

        /**
        ********* at this stage we will do different things according to connexion state
        */
        if ((_state & TCP_STATE_MASK) == TCP_STATE_WAIT_SYN_ACK){
            if ((_appliFlags & UIP_ACKDATA) && (tcpHeader->flags & (TCP_FLAG_SYN|TCP_FLAG_ACK))){
                //FIXME : we should check the offset and parse the MSS Options if present
                _state = TCP_STATE_CONNECTED;
                _remoteSeqNum = ntohl(tcpHeader->sequenceNum) + 1;
                //in UIP it is UIP_CONNECTED|UIP_NEWDATA ??
                _appliFlags = UIP_CONNECTED;
                goto tcp_send_ack;
            }
            //connexion has failed
            _appliFlags = UIP_ABORT;
            _state = TCP_STATE_DISCONNECTED;
            PRINTLN(F("[incorrect SYN - send RST]"));
            goto reset;
        }

        if ((_state & TCP_STATE_MASK) == TCP_STATE_CONNECTED){

            /** DESIGN CHOICE : ONLY PASSIVE CLOSING
               we receive a FIN
               we can't accept a FIN with unacknowledged sent datas
               Otherwise the sequence numbers will be screwed up
            */
            if(tcpHeader->flags & TCP_FLAG_FIN) {
                if(_unAckLen>0) goto check_for_retransmit;
                _remoteSeqNum += (payloadLength()+1);
                _appliFlags |= UIP_CLOSE;
                if (payloadLength()>0)_appliFlags |= UIP_NEWDATA;
                _unAckLen=1;
                _state=TCP_STATE_LAST_ACK;
                _nrtx=0;
            tcp_send_finack:
                tcpHeader->flags = TCP_FLAG_FIN | TCP_FLAG_ACK;
                goto tcp_send_nodata_reply;
            }

            // Got data!
            if (payloadLength()>0){
                _appliFlags |= UIP_NEWDATA;
                _remoteSeqNum += payloadLength();
                goto tcp_send_ack;
            }

            goto check_for_retransmit;
        }

        // Last Ack
        if ((_state & TCP_STATE_MASK) == TCP_STATE_LAST_ACK){
            if (_appliFlags & UIP_ACKDATA) {
                _state = TCP_STATE_DISCONNECTED;
                _appliFlags = UIP_CLOSE;
                goto drop;
            }
            goto check_for_retransmit;
        }

    tcp_send_ack :
        tcpHeader->flags = TCP_FLAG_ACK;
    tcp_send_nodata_reply :
        //prepareReply() swaps ethernet source and destination mac and IP and fixes a fresh hopLimit
        _ether.prepareReply();
        tcpHeader->dataOffset = 5 << 4;
    tcp_send_nodata :
        packet.setPayloadLength((tcpHeader->dataOffset & 0xF0)>>2);
        fillTCPHeader();
	    _ether.send();

    goto drop;

    //this is the check for retransmit algorithm !!
    check_for_retransmit :
        if (_unAckLen>0) {
		        if (_timer == 0){
                //first we update the timer with an exponential backoff
                _timer = UIP_RTO << (_nrtx > 4 ? 4 : _nrtx);
                //if we've reached the maximum retransmit number, we send a RST
                if (_nrtx==UIP_MAXRTX || ((_state==TCP_STATE_WAIT_SYN_ACK) && _nrtx==UIP_MAXSYNRTX)){

                    // network congestion / we inform the application and send a reset
                    _state|=TCP_STATE_TIMEDOUT;
                    tcpHeader->flags = TCP_FLAG_RST | TCP_FLAG_ACK;
                    tcpHeader->dataOffset=5<<4;
                    goto tcp_resend;
                }
                //if not we update ntrx and do the retransmission
                _nrtx++;
                switch (_state & TCP_STATE_MASK){
                case TCP_STATE_WAIT_SYN_ACK :
                    _nrexmitSynAck++;
                    tcpHeader->flags = TCP_FLAG_SYN;
                    //we include a full line of options with our SYN, in order to send our MSS to the server
                    tcpHeader->dataOffset=6<<4;
                    tcp_resend :
                        _ether.prepareSend();
                        packet.setDestination(_remoteAddress);
                        packet.setEtherDestination(_remoteMac);
                        goto tcp_send_nodata;
                    break;
                case TCP_STATE_CONNECTED :
                    _nrexmitData++;
                    tcpHeader->flags = TCP_FLAG_ACK;
                    //we set the appropriate flag in order to inform the ino application that the "data" segment has to be resent
                    _appliFlags |= UIP_REXMIT;
                    goto drop;
                    break;
                case TCP_STATE_LAST_ACK :
                    _nrexmitFinAck++;
                    tcpHeader->flags = TCP_FLAG_FIN | TCP_FLAG_ACK;
                    tcpHeader->dataOffset=5<<4;
                    goto tcp_resend;
                    break;
                }
            }
        }

    drop :
        /** Here we can return if datas were received or not
         * havePacket() sends only "blank" packets with empty payload - no side effect on TCP payload
         */
        if(_appliFlags & UIP_NEWDATA) return true;
        else return false;
}

void TCPClient::fillTCPHeader()
{
    IPv6Packet& packet = _ether.packet();
    struct tcp_header *tcpHeader = TCP_HEADER_PTR;

    tcpHeader->destinationPort = htons(_remotePort);
    tcpHeader->sourcePort = htons(_localPort);

    tcpHeader->sequenceNum = htonl(_localSeqNum);
    tcpHeader->acknowledgementNum = htonl(_remoteSeqNum);

    packet.setProtocol(IP6_PROTO_TCP);
    tcpHeader->window = htons(TCP_WINDOW_SIZE);

    if ((tcpHeader->dataOffset & 0xF0)>0x50){
        tcpHeader->mssOptionKind = 2;
        tcpHeader->mssOptionLen = 4;
        tcpHeader->mssOptionValue = tcpHeader->window;
    }
    tcpHeader->urgentPointer = 0;
    tcpHeader->checksum = 0;
    tcpHeader->checksum = htons(packet.calculateChecksum());
    PRINT(F("[Send"));
    if ((tcpHeader->flags & 0x3f) & TCP_FLAG_SYN)PRINT(F("-SYN"));
    if ((tcpHeader->flags & 0x3f) & TCP_FLAG_FIN)PRINT(F("-FIN"));
    if ((tcpHeader->flags & 0x3f) & TCP_FLAG_ACK)PRINT(F("-ACK"));
    if ((tcpHeader->flags & 0x3f) & TCP_FLAG_RST)PRINT(F("-RST"));
    if ((tcpHeader->flags & 0x3f) & TCP_FLAG_PSH)PRINT(F("-PSH"));
    PRINT(F("-sizeIPpayload:"));PRINT(packet.payloadLength());
    PRINTLN(F("]"));

}

void TCPClient::sendInternal(uint16_t length, boolean /*isReply*/)
{
    IPv6Packet& packet = _ether.packet();
    struct tcp_header *tcpHeader = TCP_HEADER_PTR;
    //When data are sent, TCPHeader must contain 6 lines of 32 bits
    tcpHeader->dataOffset=6<<4;
    packet.setPayloadLength(((tcpHeader->dataOffset & 0xF0)>>2) + length);
    fillTCPHeader();
	_ether.send();

    if (!(_appliFlags & UIP_REXMIT)){
        _unAckLen+=length;
        _timer=_rto;
		//added on 01/07/2017
		_nrtx=0;
    }

}

uint8_t* TCPClient::payload()
{
    IPv6Packet& packet = _ether.packet();
    return packet.payload() + TCP_RECEIVE_HEADER_LEN;
}

uint16_t TCPClient::payloadLength()
{
    IPv6Packet& packet = _ether.packet();
    return packet.payloadLength() - TCP_RECEIVE_HEADER_LEN;
}

uint8_t* TCPClient::transmitPayload()
{
    IPv6Packet& packet = _ether.packet();
    return packet.payload() + TCP_TRANSMIT_HEADER_LEN;
}

void TCPClient::printState(Print &p) const
{
    p.print('[');p.print('x');
    printPaddedHex(_state, p);
    p.print(']');
}
