/*
 * Copyright 2012 DeKay
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#include <cc1110.h>
#include "ioCCxx10_bitdef.h"
#include "display.h"
#include "keys.h"
#include "stdio.h"
#include "radio.h"
#include "pocketwx.h"
#include "pm.h"

/* globals */
__xdata channel_info chan_table[NUM_CHANNELS];
u32 centerFreq;
u32 userFreq;
__bit sleepy;
__bit packetDone;
const __data u8 *pktbuf;
u8 ch;

/* CRC calculation from http://www.menie.org/georges/embedded/ */
u16 crc16_ccitt(const __data u8 *buf, u8 len)
{
    u16 crc = 0;
    while( len-- ) {
        int i;
        crc ^= *(char *)buf++ << 8;
        for( i = 0; i < 8; ++i ) {
            if( crc & 0x8000 )
                crc = (crc << 1) ^ 0x1021;
            else
                crc = crc << 1;
        }
    }
    return crc;
}

void printDebugHeader() {
    /* IM-ME display is 132W x 64H for a 22 x 8 character display */
    SSN = LOW;
    setCursor(0, 0);
    printf("Freq:          Chan:");
    setCursor(1, 0);
    printf("Data:");
    setCursor(3,0);
    printf("CRC:");
    setCursor(4,0);
    printf("LQI:");
    setCursor(5,0);
    printf("RSSI:    NOW:");
    setCursor(6,0);
    printf("OFFSET:");
    SSN = HIGH;
}

void printDebugFrequency(u32 freq, u8 ch) {
    SSN = LOW;
    setCursor(0, 30);
    printf("%lu", freq);
    setCursor(0, 120);
    printf("%2u", ch);
    SSN = HIGH;
}

void printDebugPacket() {
    u16 crc = crc16_ccitt(pktbuf, 6);
    SSN = LOW;
    setCursor(1, 30);
    printf("%02x %02x %02x %02x", pktbuf[0], pktbuf[1], pktbuf[2], pktbuf[3]);
    setCursor(2, 30);
    printf("%02x %02x %02x %02x", pktbuf[4], pktbuf[5], pktbuf[6], pktbuf[7]);
    setCursor(3, 24);
    printf("%04x", crc);
    setCursor(4, 24);
    printf("%3u ", pktbuf[9]);
    setCursor(5, 30);
    printf("%3u ", pktbuf[8]);
    setCursor(6, 42);
    printf("%3u ", pktbuf[10]);
    SSN= HIGH;
}

void poll_keyboard() {

	switch (getkey()) {
	case 'a':
	case 'A':
		userFreq -= STEP_1MHZ;
		break;
	case 's':
	case 'S':
		userFreq -= STEP_100KHZ;
		break;
	case 'd':
	case 'D':
		userFreq -= STEP_10KHZ;
		break;
	case 'f':
	case 'F':
		userFreq -= STEP_1KHZ;
		break;
	case 'h':
	case 'H':
		userFreq += STEP_1KHZ;
		break;
	case 'j':
	case 'J':
		userFreq += STEP_10KHZ;
		break;
	case 'k':
	case 'K':
		userFreq += STEP_100KHZ;
		break;
	case 'l':
	case 'L':
		userFreq += STEP_1MHZ;
		break;
	case ' ':
		/* pause */
		while (getkey() == (u8)' ');
		while (getkey() != (u8)' ')
			sleepMillis(200);
		break;
	case KPWR:
		sleepy = 1;
		break;
	default:
		break;
	}
}

void pollPacket() {
    if (packetDone) {
        packetDone = 0;
        printDebugPacket();
        /* First and ten, do it again! */
        centerFreq = setFrequency(centerFreq);
        printDebugFrequency(centerFreq, 0);
        chan_table[ch].ss = 0;
        chan_table[ch].max = 0;
    }
}

void main(void) {
	u16 i;
    pktbuf = radio_getbuf();
    ch = 0;

reset:
	centerFreq = DEFAULT_FREQ;
	userFreq = centerFreq;
	sleepy = 0;
    packetDone = 0;

	xtalClock();
	setIOPorts();
	configureSPI();
	LCDReset();
	radio_init();
    clear();
    printDebugHeader();
    setFrequency(centerFreq);
    printDebugFrequency(centerFreq, ch);

	while (1) {
		poll_keyboard();
        pollPacket();

        /* Show current RSSI */
        SSN = LOW;
        setCursor(5, 78);
        printf("%3u", (RSSI ^ 0x80));
        SSN = HIGH;

        /* TODO Mod this when more than one channel */
		if (userFreq != centerFreq) {
			centerFreq = setFrequency(userFreq);
            chan_table[ch].ss = 0;
            chan_table[ch].max = 0;
            printDebugFrequency(centerFreq, ch);
        }

		/* Go to sleep (more or less a shutdown) if power button pressed */
		if (sleepy) {
			clear();
			sleepMillis(1000);
			SSN = LOW;
			LCDPowerSave();
			SSN = HIGH;

			while (1) {
				sleep();

				/* Power button depressed long enough to wake? */
				sleepy = 0;
				for (i = 0; i < DEBOUNCE_COUNT; i++) {
					sleepMillis(DEBOUNCE_PERIOD);
					if (keyscan() != KPWR) sleepy = 1;
				}
				if (!sleepy) break;
			}

			/* Reset on wake */
			goto reset;
		}
    }
}

//         chan_table[ch].ss = (RSSI ^ 0x80);
//         chan_table[ch].max = MAX(chan_table[ch].ss, chan_table[ch].max);
//         SSN = LOW;
//         setCursor(2, 30);
//         printf("%3u", chan_table[ch].ss);
//         setCursor(2, 84);
//         printf("%3u", chan_table[ch].max);
//
//         SSN = HIGH;

/* Handle incoming packet. This is called within an ISR so keep it short. */
void packet_rx_callback(const __data u8 *buf)
{
    packetDone = 1;
}

/*
 * "If you have multiple source fles in your project, interrupt service routines
 *  can be present in any of them, but a prototype of the isr MUST be present or
 *  included in the file that contains the function main."
 *   http://sdcc.sourceforge.net/doc/sdccman.pdf
 */

void rftxrx_isr(void) __interrupt(RFTXRX_VECTOR);
void rf_isr(void) __interrupt(RF_VECTOR);
