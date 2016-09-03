/**
 * Header file for direct Ethernet frame access to the W5100 controller
 * @file w5100.h
 */

/*
 * Copyright (c) 2013, WIZnet Co., Ltd.
 * Copyright (c) 2016, Nicholas Humfrey
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef ETHERSIA_W5100_H
#define ETHERSIA_W5100_H

#include <SPI.h>

#include "EtherSia.h"

/**
 * Send and receive Ethernet frames directly using a Wiznet W5100 controller.
 */
class EtherSia_W5100 : public EtherSia {

public:
    /**
     * Constructor that uses the default hardware SPI pins
     * @param cs the Arduino Chip Select / Slave Select pin (default 10)
     */
    EtherSia_W5100(int8_t cs=SS);

    /**
     * Initialise the Ethernet controller
     * Must be called before sending or receiving Ethernet frames
     *
     * @param address the local MAC address for the Ethernet interface
     * @return Returns true if setting up the Ethernet interface was successful
     */
    boolean begin(const MACAddress &address);

    /**
     * Shut down the Ethernet controlled
     */
    void end();
  
    /**
     * Send an Ethernet frame
     * @param data a pointer to the data to send
     * @param datalen the length of the data in the packet
     * @return the number of bytes transmitted
     */
    uint16_t sendFrame(const uint8_t *data, uint16_t datalen);

    /**
     * Read an Ethernet frame
     * @param buffer a pointer to a buffer to write the packet to
     * @param bufsize the available space in the buffer
     * @return the length of the received packet
     *         or 0 if no packet was received
     */
    uint16_t readFrame(uint8_t *buffer, uint16_t bufsize);

private:
    const uint16_t TxBufferAddress = 0x4000;  /* Internal Tx buffer address of the iinchip */
    const uint16_t RxBufferAddress = 0x6000;  /* Internal Rx buffer address of the iinchip */

    /**
     * Default function to select chip.
     * @note This function help not to access wrong address. If you do not describe this function or register any functions,
     * null function is called.
     */
    inline void wizchip_cs_select()
    {
        digitalWrite(_cs, LOW);
    }

    /**
     * Default function to deselect chip.
     * @note This function help not to access wrong address. If you do not describe this function or register any functions,
     * null function is called.
     */
    inline void wizchip_cs_deselect()
    {
        digitalWrite(_cs, HIGH);
    }

    /**
     * Reset WIZCHIP by softly.
     */
    void wizchip_sw_reset(void);

    /**
     * Reads a 1 byte value from a register.
     * @param AddrSel Register address
     * @return The value of register
     */
    uint8_t wizchip_read(uint16_t AddrSel);

    /**
     * Reads a 2 byte value from a register.
     * @param AddrSel Register address
     * @return The value of register
     */
    uint16_t wizchip_read_word(uint16_t AddrSel);

    /**
     * Writes a 1 byte value to a register.
     * @param AddrSel Register address
     * @param wb Write data
     * @return void
     */
    void wizchip_write(uint16_t AddrSel, uint8_t wb);

    /**
     * Writes a 2 byte value to a register.
     * @param AddrSel Register address
     * @param wb Write data
     * @return void
     */
    void wizchip_write_word(uint16_t AddrSel, uint16_t word);

    /**
     * It reads sequence data from registers.
     * @param AddrSel Register address
     * @param pBuf Pointer buffer to read data
     * @param len Data length
     */
    void wizchip_read_buf(uint16_t AddrSel, uint8_t* pBuf, uint16_t len);

    /**
     * It writes sequence data to registers.
     * @param AddrSel Register address
     * @param pBuf Pointer buffer to write data
     * @param len Data length
     */
    void wizchip_write_buf(uint16_t AddrSel, const uint8_t* pBuf, uint16_t len);

    /**
     * It copies data to internal TX memory
     *
     * @details This function reads the Tx write pointer register and after that,
     * it copies the <i>wizdata(pointer buffer)</i> of the length of <i>len(variable)</i> bytes to internal TX memory
     * and updates the Tx write pointer register.
     * This function is being called by send() and sendto() function also.
     *
     * @param wizdata Pointer buffer to write data
     * @param len Data length
     * @sa recv_data()
     */
    void wizchip_send_data(const uint8_t *wizdata, uint16_t len);

    /**
     * It copies data to your buffer from internal RX memory
     *
     * @details This function read the Rx read pointer register and after that,
     * it copies the received data from internal RX memory
     * to <i>wizdata(pointer variable)</i> of the length of <i>len(variable)</i> bytes.
     * This function is being called by recv() also.
     *
     * @param wizdata Pointer buffer to read data
     * @param len Data length
     * @sa wiz_send_data()
     */
    void wizchip_recv_data(uint8_t *wizdata, uint16_t len);

    /**
     * Get @ref Sn_TX_FSR register
     * @return uint16_t. Value of @ref Sn_TX_FSR.
     */
    uint16_t getSn_TX_FSR();

    /**
     * Get @ref Sn_RX_RSR register
     * @return uint16_t. Value of @ref Sn_RX_RSR.
     */
    uint16_t getSn_RX_RSR();


    /** Common registers */
    enum {
        MR = 0x0000,    // Mode Register address (R/W)
        GAR = 0x0001,   // Gateway IP Register address(R/W)
        SUBR = 0x0005,  // Subnet mask Register address(R/W)
        SHAR = 0x0009,  // Source MAC Register address(R/W)
        SIPR = 0x000F,  // Source IP Register address(R/W)
        IR = 0x0015,    // Interrupt Register(R/W)
        IMR = 0x0016,   // Socket Interrupt Mask Register(R/W)
        RTR = 0x0017,   // Timeout register address( 1 is 100us )(R/W)
        RCR = 0x0019,   // Retry count register(R/W)
        RMSR = 0x001A,  // Receive Memory Size
        TMSR = 0x001B,  // Transmit Memory Size
    };

    /** Socket 0 registers */
    enum {
        Sn_MR = 0x0400,     ///< Socket Mode register(R/W)
        Sn_CR = 0x0401,     ///< Socket command register (R/W)
        Sn_IR = 0x0402,     ///< Socket interrupt register (R)
        Sn_SR = 0x0403,     ///< Socket status register (R)
        Sn_PORT = 0x0404,   ///< source port register (R/W)
        Sn_DHAR = 0x0406,   ///< Peer MAC register address (R/W)
        Sn_DIPR = 0x040C,   ///< Peer IP register address (R/W)
        Sn_DPORT = 0x0410,  ///< Peer port register address (R/W)
        Sn_MSSR = 0x0412,   ///< Maximum Segment Size(Sn_MSSR0) register address (R/W)
        Sn_PROTO = 0x0414,  ///< IP Protocol(PROTO) Register (R/W)
        Sn_TOS = 0x0415,    ///< IP Type of Service(TOS) Register (R/W)
        Sn_TTL = 0x0416,    ///< IP Time to live(TTL) Register (R/W)
        Sn_TX_FSR = 0x0420, ///< Transmit free memory size register (R)
        Sn_TX_RD = 0x0422,  ///< Transmit memory read pointer register address (R)
        Sn_TX_WR = 0x0424,  ///< Transmit memory write pointer register address (R/W)
        Sn_RX_RSR = 0x0426, ///< Received data size register (R)
        Sn_RX_RD = 0x0428,  ///< Read point of Receive memory (R/W)
        Sn_RX_WR = 0x042A,  ///< Write point of Receive memory (R)
    };

    /** Mode register values */
    enum {
        MR_RST = 0x80,    ///< Reset
        MR_PB = 0x10,     ///< Ping block
        MR_AI = 0x02,     ///< Address Auto-Increment in Indirect Bus Interface
        MR_IND = 0x01,    ///< Indirect Bus Interface mode
    };

    /** Sn_MR register values */
    enum {
        Sn_MR_CLOSE = 0x00,  ///< Unused socket
        Sn_MR_TCP = 0x01,    ///< TCP
        Sn_MR_UDP = 0x02,    ///< UDP
        Sn_MR_IPRAW = 0x03,  ///< IP LAYER RAW SOCK
        Sn_MR_MACRAW = 0x04, ///< MAC LAYER RAW SOCK
        Sn_MR_ND = 0x20,     ///< No Delayed Ack(TCP) flag
        Sn_MR_MF = 0x40,     ///< Use MAC filter
        Sn_MR_MULTI = 0x80,  ///< support multicating
    };

    /** Sn_CR register values */
    enum {
        Sn_CR_OPEN = 0x01,      ///< Initialise or open socket
        Sn_CR_CLOSE = 0x10,     ///< Close socket
        Sn_CR_SEND = 0x20,      ///< Update TX buffer pointer and send data
        Sn_CR_SEND_MAC = 0x21,  ///< Send data with MAC address, so without ARP process
        Sn_CR_SEND_KEEP = 0x22, ///< Send keep alive message
        Sn_CR_RECV = 0x40,      ///< Update RX buffer pointer and receive data
    };

    /** Sn_SR register values */
    enum {
        SOCK_CLOSED = 0x00,      ///< Closed
        SOCK_INIT = 0x13,        ///< Initiate state
        SOCK_LISTEN = 0x14,      ///< Listen state
        SOCK_SYNSENT = 0x15,     ///< Connection state
        SOCK_SYNRECV = 0x16,     ///< Connection state
        SOCK_ESTABLISHED = 0x17, ///< Success to connect
        SOCK_FIN_WAIT = 0x18,    ///< Closing state
        SOCK_CLOSING = 0x1A,     ///< Closing state
        SOCK_TIME_WAIT = 0x1B,   ///< Closing state
        SOCK_CLOSE_WAIT = 0x1C,  ///< Closing state
        SOCK_LAST_ACK = 0x1D,    ///< Closing state
        SOCK_UDP = 0x22,         ///< UDP socket
        SOCK_IPRAW = 0x32,       ///< IP raw mode socket
        SOCK_MACRAW = 0x42,      ///< MAC raw mode socket
    };

    /** Sn_IR register values */
    enum {
        Sn_IR_CON = 0x01,      ///< CON Interrupt
        Sn_IR_DISCON = 0x02,   ///< DISCON Interrupt
        Sn_IR_RECV = 0x04,     ///< RECV Interrupt
        Sn_IR_TIMEOUT = 0x08,  ///< TIMEOUT Interrupt
        Sn_IR_SENDOK = 0x10,   ///< SEND_OK Interrupt
    };

    /**
     * Set Mode Register
     * @param (uint8_t)mr The value to be set.
     * @sa getMR()
     */
    inline void setMR(uint8_t mode) {
        wizchip_write(MR, mode);
    }

    /**
     * Get @ref MR.
     * @return uint8_t. The value of Mode register.
     * @sa setMR()
     */
    inline uint8_t getMR() {
        return wizchip_read(MR);
    }

    /**
     * Set @ref SHAR.
     * @param (uint8_t*)shar Pointer variable to set local MAC address. It should be allocated 6 bytes.
     * @sa getSHAR()
     */
    inline void setSHAR(const uint8_t* macaddr)  {
        wizchip_write_buf(SHAR, macaddr, 6);
    }

    /**
     * Get @ref SHAR.
     * @param (uint8_t*)shar Pointer variable to get local MAC address. It should be allocated 6 bytes.
     * @sa setSHAR()
     */
    inline void getSHAR(uint8_t* macaddr) {
        wizchip_read_buf(SHAR, macaddr, 6);
    }

    /**
     * Set @ref Sn_TXMEM_SIZE register
     * @param (uint8_t)txmemsize Value to set \ref Sn_TXMEM_SIZE
     * @sa getSn_TXMEM_SIZE()
     */
    inline uint16_t getSn_TXMEM_SIZE() {
        return wizchip_read(TMSR) & 0x03;
    }

    /**
     * Get @ref Sn_RXMEM_SIZE register
     * @return uint8_t. Value of @ref Sn_RXMEM.
     * @sa setSn_RXMEM_SIZE()
     */
    inline uint16_t getSn_RXMEM_SIZE() {
        return wizchip_read(RMSR) & 0x03;
    }

    /**
     * Get the max RX buffer size of socket sn
     * @return uint16_t. Max buffer size
     */
    inline uint16_t getSn_RxMAX() {
        return (uint16_t)(1 << getSn_RXMEM_SIZE()) << 10;
    }

    /**
     * Get the max TX buffer size of socket sn
     * @return uint16_t. Max buffer size
     */
    inline uint16_t getSn_TxMAX() {
        return (uint16_t)(1 << getSn_TXMEM_SIZE()) << 10;
    }

    /**
     * Get the mask of socket sn RX buffer.
     * @return uint16_t. Mask value
     */
    inline uint16_t getSn_RxMASK() {
        return getSn_RxMAX() - 1;
    }

    /**
     * Get the mask of socket sn TX buffer
     * @return uint16_t. Mask value
     */
    inline uint16_t getSn_TxMASK() {
        return getSn_TxMAX() - 1;
    }

    /**
     * Set @ref Sn_TX_WR register
     * @param (uint16_t)txwr Value to set @ref Sn_TX_WR
     * @sa GetSn_TX_WR()
     */
    inline uint16_t getSn_TX_WR() {
        return wizchip_read_word(Sn_TX_WR);
    }

    /**
     * Set @ref Sn_TX_WR register
     * @param (uint16_t)txwr Value to set @ref Sn_TX_WR
     * @sa GetSn_TX_WR()
     */
    inline void setSn_TX_WR(uint16_t txwr) {
        wizchip_write_word(Sn_TX_WR, txwr);
    }

    /**
     * Get @ref Sn_RX_RD register
     * @regurn uint16_t. Value of @ref Sn_RX_RD.
     * @sa setSn_RX_RD()
     */
    inline uint16_t getSn_RX_RD() {
        return wizchip_read_word(Sn_RX_RD);
    }

    /**
     * Set @ref Sn_RX_RD register
     * @param (uint16_t)rxrd Value to set @ref Sn_RX_RD
     * @sa getSn_RX_RD()
     */
    inline void setSn_RX_RD(uint16_t rxrd) {
        wizchip_write_word(Sn_RX_RD, rxrd);
    }

    /**
     * Set @ref Sn_MR register
     * @param mr Value to set @ref Sn_MR
     * @sa getSn_MR()
     */
    inline void setSn_MR(uint8_t mr) {
        wizchip_write(Sn_MR, mr);
    }

    /**
     * Get @ref Sn_MR register
     * @return Value of @ref Sn_MR.
     * @sa setSn_MR()
     */
    inline uint8_t getSn_MR() {
        return wizchip_read(Sn_MR);
    }

    /**
     * Set @ref Sn_CR register, then wait for the command to execute
     * @param (uint8_t)cr Value to set @ref Sn_CR
     * @sa getSn_CR()
     */
    void setSn_CR(uint8_t cr);

    /**
     * Get @ref Sn_CR register
     * @return uint8_t. Value of @ref Sn_CR.
     * @sa setSn_CR()
     */
    inline uint8_t getSn_CR() {
        return wizchip_read(Sn_CR);
    }

    /**
     * Get @ref Sn_SR register
     * @return uint8_t. Value of @ref Sn_SR.
     */
    inline uint8_t getSn_SR() {
        return wizchip_read(Sn_SR);
    }

    /**
     * Get @ref Sn_IR register
     * @return uint8_t. Value of @ref Sn_IR.
     * @sa setSn_IR()
     */
    inline uint8_t getSn_IR() {
        return wizchip_read(Sn_IR);
    }

    /**
     * Set @ref Sn_IR register
     * @param (uint8_t)ir Value to set @ref Sn_IR
     * @sa getSn_IR()
     */
    inline void setSn_IR(uint8_t ir) {
        wizchip_write(Sn_IR, ir);
    }


    int8_t _cs;

};

#endif //_W5100_H_


