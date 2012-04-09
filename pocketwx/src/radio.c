/*
 * This file is part of PinkOS
 * Copyright (c) 2010, Joby Taffey <jrt@hodgepig.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <organization> nor the
 * names of its contributors may be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "display.h"
#include "radio.h"
#include "cc1110-ext.h"
#include "ioCCxx10_bitdef.h"
#include <cc1110.h>

// #define DEBUG 1

#define PKTBUF_MAX 11   /* Three extra bytes appended to packet */

/* Rx packet buffer */
static __data volatile u8 pktbuf[PKTBUF_MAX];
static volatile u8 pktbuf_index = 0;

/* Flags */
static volatile bool errflag = false;
static volatile bool receiving = false;
static volatile bool pkt_complete = false;

/* External access to buffer */
__data volatile const u8 *radio_getbuf(void)
{
    return pktbuf;
}

/* Wait for MARCSTATE change */
static bool wait_rfstate(u8 state)
{
    u8 count = 0xFF;
    while(MARCSTATE != state && count != 0)
    {
        sleepMillis(1);
        count--;
    }

    if (count == 0)
    {
        return false;
    }
    return true;
}

u8 reverseBits(u8 b) {
    /*
     * The data bytes come over the air from the ISS least significant bit first.  Fix them as we go. From
     * http://www.ocf.berkeley.edu/~wwu/cgi-bin/yabb/YaBB.cgi?board=riddles_cs;action=display;num=1103355188
     */
    b = ((b & 0b11110000) >>4 ) | ((b & 0b00001111) << 4);
    b = ((b & 0b11001100) >>2 ) | ((b & 0b00110011) << 2);
    b = ((b & 0b10101010) >>1 ) | ((b & 0b01010101) << 1);

    return(b);
}

void radio_init(void) {
    /* Enter idle */
    RFST = RFST_SIDLE;
    wait_rfstate(MARC_STATE_IDLE);

    /*
     * Remember: Min BW = 2*(Peak Deviation + Bit Rate)
     *
     * Remember also that the radio transmission is composed of
     * - four preamble bytes in a 1010... pattern
     * - two sync bytes
     * - eight data bytes
     *
     * RF settings based on sniffing packets from a CC1021 in a Davis Weather
     * console, figuring out what those meant, and plugging those values into
     * TI Signal Studio. Also converted to a packet based format that is not
     * supported on the CC1021.  Only registers differing from the default are
     * initialized.
     */

    SYNC1 = 0xcb;       // Davis got the bit order swapped for the sync bytes.
    SYNC0 = 0x89;       // They show up over the air from the ISS LSB first.
    PKTLEN = 0x08;      // packet length - 8 bytes
    PKTCTRL1 = 0xc4;    // Packet automation control 1
                        // - Packet quality threshold = 24 (6*4)
                        // - Append RSSI and PQI
                        // - No address check
    PKTCTRL0 = 0x00;    // packet automation control 0
                        // Disable CRC check
                        // Fixed packet length mode
    FSCTRL1 = 0x06;     // frequency synthesizer control
    MDMCFG4 = 0xC9;     // modem configuration
    MDMCFG3 = 0x75;     // modem configuration
    MDMCFG2 = 0x11;     // modem configuration
                        // GFSK modulation, not 2-FSK!
                        // Match 15 out of 16 bits in sync word (0xd391)
    MDMCFG0 = 0xE5;     // modem configuration
                        // This sets channel spacing - don't really care.
    DEVIATN = 0x13;     // modem deviation setting
    MCSM0 = 0x18;       // main radio control state machine configuration
                        //  - Autocal when going from IDLE to RX
    FOCCFG = 0x17;      // frequency offset compensation configuration
    FSCAL3 = 0xE9;      // frequency synthesizer calibration
    FSCAL2 = 0x2A;      // frequency synthesizer calibration
    FSCAL1 = 0x00;      // frequency synthesizer calibration
    FSCAL0 = 0x1F;      // frequency synthesizer calibration
    TEST2 = 0x81;       // various test settings
    TEST1 = 0x35;       // various test settings
    TEST0 = 0x09;       // various test settings
    PA_TABLE0 = 0x8E;   // pa power setting 0

    /* Enable interrupts as per Section 10.5.1 of the manual */

    /* TODO Implement RFIF_IRQ_TIMEOUT at some point */
    RFIM = RFIF_IRQ_DONE | RFIF_IRQ_RXOVF | RFIF_IRQ_SFD;

    RFIF = 0;           // Clear interrupt flags
    RFTXRXIE = 1;       // Enable RF Tx / RX done interrupt enable (IEN0.0)
    IEN2 |= IEN2_RFIE;  // Enable RF general interrupts
    EA = 1;             // Enable global interrupts
}

/* Set the radio frequency in Hz */
u32 setFrequency(u32 freq) {
    /* TODO Put FREQ_REF in the makefile */
    u32 setting = (u32) (freq * (65536.0f/FREQ_REF) );

    RFST = RFST_SIDLE;                /* We will autocal coming out of idle */
    wait_rfstate(MARC_STATE_IDLE);
    RFIF = 0;   /* Clear our interrupt flags to be on the safe side */

    /* The frequency setting is in units of FREQ_REF/(2^16) Hz */
    /* Be sure FREQ_REF is set correctly for your IM-ME in the Makefile *** */

    FREQ2 = (setting >> 16) & 0xff;
    FREQ1 = (setting >> 8) & 0xff;
    FREQ0 = setting & 0xff;
    RFST = RFST_SRX;    /* We will autocal coming out of idle */

    /* Initialize flags while we wait */
    errflag = false;
    pkt_complete = false;

    wait_rfstate(MARC_STATE_RX);

    return freq;
}

/*
 * This is the interrupt vector for RFTXRX_VECTOR.  It is raised for us when Rx
 * data is ready in the RFD register.  See Section 13.3 of the datasheet.
 *
 * The format of the packet is this, totaling 11 bytes
 * - eight bytes from the ISS
 * - two bytes appended by the CC1110 for RSSI and LQI
 * - one byte appended in the code for the FREQEST offset error
 */
void rftxrx_isr(void) __interrupt RFTXRX_VECTOR
{
    /*
     * 13.3.1.1 says to clear the IRQ before reading RFD, or else this can screw
     * up the automatic append of the status bytes to the end of the packet.
     */

    RFIF &= ~(RFIF_IRQ_DONE);

    if (MARCSTATE == MARC_STATE_RX) {
        /* Fetch next byte. Remember we are using fixed length packets. */
        if (pktbuf_index < PKTBUF_MAX - 1)
            pktbuf[pktbuf_index] = RFD;

        /* Bytes from the ISS are in wrong bit order.  Sigh */
        /* We've post incremented, so be sure to back up by one */
        if (pktbuf_index < PKTBUF_MAX - 3)
            pktbuf[pktbuf_index] = reverseBits(pktbuf[pktbuf_index]);

        pktbuf_index++;

        /* Finished. Read the frequency error.  Must be idle to do this. */
        if (pktbuf_index == PKTBUF_MAX - 1) {
            RFST = RFST_SIDLE;
            wait_rfstate(MARC_STATE_IDLE);
            pkt_complete = true;
            pktbuf[PKTBUF_MAX - 3] ^= 0x80;     /* Normalize RSSI value */
            pktbuf[PKTBUF_MAX - 2] &= 0x7f;     /* Clear CRC flag bit */
            pktbuf[PKTBUF_MAX - 1] = FREQEST;
            packet_rx_callback(radio_getbuf());
        }
    } else
        errflag = true;
}

/*
 * This is the interrupt vector for RF_VECTOR (#16)
 * All other general interrupts flags are in the RFIF register.
 * At some point we'll also want to work with IRQ_TIMEOUT
 */
void rf_isr(void) __interrupt RF_VECTOR
{
    /* Clear flags */
    S1CON &= ~(S1CON_RFIF_1 + S1CON_RFIF_0);

    /* The RFIF_IRQ_DONE flag will be handled by rftxrx_isr */

    /* Start of frame delimiter */
    if (RFIF & RFIF_IRQ_SFD)
    {
        RFIF &= ~RFIF_IRQ_SFD;
        pktbuf_index = 0;
    }

    /* Errors TODO Handle timeout */
    /* if ((RFIF & RFIF_IRQ_RXOVF) || (RFIF & RFIF_IRQ_TIMEOUT)) */
    if (RFIF & RFIF_IRQ_RXOVF)
    {
        RFIF &= ~RFIF_IRQ_RXOVF;
        RFST = RFST_SIDLE;
        wait_rfstate(MARC_STATE_IDLE);
        pktbuf_index = 0;
        errflag = true;
    }
}
