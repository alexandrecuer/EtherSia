#include "EtherSia.h"
#include "ICMPv6Packet.h"
#include "util.h"

PingClient::PingClient(EtherSia &ether) : Socket(ether)
{
    this->_identifier = random();
    this->_sequenceNumber = 0;
}

boolean PingClient::setRemoteAddress(const char *remoteAddress)
{
    return Socket::setRemoteAddress(remoteAddress, 0);
}

boolean PingClient::havePacket()
{
    ICMPv6Packet& packet = (ICMPv6Packet&)_ether.packet();

    if (!_ether.bufferContainsReceived()) {
        Serial.println("!bufferContainsReceived");
        return false;
    }

    if (packet.protocol() != IP6_PROTO_ICMP6) {
        // Wrong protocol
        Serial.println("Wrong protocol");
        return false;
    }

    if (!_ether.isOurAddress(packetDestination())) {
        // Wrong destination address
        Serial.println("Wrong destination address");
        return false;
    }

    if (!_remoteAddress.isZero() && packetSource() != _remoteAddress) {
        // Wrong source address
        Serial.println("Wrong source address");
        return false;
    }

    if (packet.type != ICMP6_TYPE_ECHO_REPLY) {
        // Wrong ICMPv6 type
        Serial.println("Wrong ICMPv6 type");
        return false;
    }

    if (ntohs(packet.echo.identifier) != this->_identifier) {
        // Wrong ICMPv6 Echo identifier
        Serial.println("Wrong ICMPv6 Echo identifier");
        return false;
    }

    if (ntohs(packet.echo.sequenceNumber) != (this->_sequenceNumber-1)) {
        // Sequence numbers didn't match
        Serial.println("Sequence numbers didn't match");
        return false;
    }

    // Everthing matched up
    return true;
}

void PingClient::send()
{
    uint8_t *payload = this->payload();
    uint8_t n;

    // Add some pseudo-random data to the payload
    for(n=0; n<8; n++) {
        payload[n] = random();
    }

    Socket::send(n, false /*isReply*/);
}

void PingClient::sendInternal(uint16_t length, boolean /*isReply*/)
{
    ICMPv6Packet& packet = (ICMPv6Packet&)_ether.packet();
    uint16_t totalLen = ICMP6_HEADER_LEN + ICMP6_ECHO_HEADER_LEN + length;
    
    packet.setProtocol(IP6_PROTO_ICMP6);
    packet.setPayloadLength(totalLen);
    
    packet.type = ICMP6_TYPE_ECHO;
    packet.code = 0;
    packet.echo.sequenceNumber = htons(this->_sequenceNumber++);
    packet.echo.identifier = htons(this->_identifier);
    packet.checksum = 0;
    packet.checksum = htons(packet.calculateChecksum());

    _ether.send();
}

uint8_t* PingClient::payload()
{
    ICMPv6Packet& packet = (ICMPv6Packet&)_ether.packet();
    return (uint8_t*)(packet.payload()) + ICMP6_HEADER_LEN + ICMP6_ECHO_HEADER_LEN;
}

uint16_t PingClient::payloadLength()
{
    ICMPv6Packet& packet = (ICMPv6Packet&)_ether.packet();
    return packet.payloadLength() - ICMP6_HEADER_LEN - ICMP6_ECHO_HEADER_LEN;
}
