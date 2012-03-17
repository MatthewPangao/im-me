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
#include "pocketwx.h"
#include "pm.h"

/* globals */
__xdata channel_info chan_table[NUM_CHANNELS];
u32 center_freq;
u32 user_freq;
__bit sleepy;

void radio_setup() {
	/* IF of 457.031 kHz */
	FSCTRL1 = 0x12;
	FSCTRL0 = 0x00;

	/* disable 3 highest DVGA settings */
	AGCCTRL2 |= AGCCTRL2_MAX_DVGA_GAIN;

	/* frequency synthesizer calibration */
	FSCAL3 = 0xEA;
	FSCAL2 = 0x2A;
	FSCAL1 = 0x00;
	FSCAL0 = 0x1F;

	/* "various test settings" */
	TEST2 = 0x88;
	TEST1 = 0x31;
	TEST0 = 0x09;

	/* no automatic frequency calibration */
	MCSM0 = 0;

    /* TODO According to CC1021 datasheet, Table 18, the filter bandwidth
     * is really 51.2 kHz.  I have it set way too wide. */

    /* channel bandwidth and data rate
     * This corresponds to a 19.2 kbaud datarate, 4.9 kHz deviation, and 
     * 168.75 kHz receive filter bandwidth at 27 MHz oscillator frequency */
    MDMCFG4 = 0x99;  /* CHANBW_E, CHANBW_M, DRATE_E */
    MDMCFG3 = 0x75;  /* DRATE_M */
    MDMCFG2 = 0x14;  /* Manchester disabled, TBD GUESSED AT SYNC */
    MDMCFG1 = 0x13;  /* No FEC, minimum preamble, max channel spacing */;
    MDMCFG0 = 0xff;  /* Channel spacing - not used */

    DEVIATN = 0x14;  /* 4.94 kHz deviation (CC1021 is 4.8 kHz) */

    /* Always above 848 MHz so always select high VCO */
    FSCAL2 = 0x2A;
}

/* set the radio frequency in Hz */
void set_radio_freq(u32 freq) {
	/* the frequency setting is in units of FREQ_REF/(2^16) Hz,
	 * which is 396.728515625Hz w/FREQ_REF==26MHz. */
	u32 setting = (u32) (freq * (65536.0f/FREQ_REF) );
	FREQ2 = (setting >> 16) & 0xff;
	FREQ1 = (setting >> 8) & 0xff;
	FREQ0 = setting & 0xff;
}

/* freq in Hz */
u32 calibrate_freq(u32 freq, u8 ch) {
	set_radio_freq(freq);

    /* RFST is the RF Strobe Commands Register */

	RFST = RFST_SCAL; /* Cal freq synth and turn it off */
	RFST = RFST_SRX;  /* Enable Rx. Perform cal first if coming from idle */

	/* wait for calibration */
	sleepMillis(2);

	/* store frequency/calibration settings */
	chan_table[ch].freq2 = FREQ2;
	chan_table[ch].freq1 = FREQ1;
	chan_table[ch].freq0 = FREQ0;
	chan_table[ch].fscal3 = FSCAL3;
	chan_table[ch].fscal2 = FSCAL2;
	chan_table[ch].fscal1 = FSCAL1;

	/* get initial RSSI measurement */
	chan_table[ch].ss = (RSSI ^ 0x80); /* RSSI is 2's complement */
	chan_table[ch].max = 0;

    SSN = LOW;
    setCursor(0, 30);
    printf("%lu", freq);
    setCursor(0, 120);
    printf("%2u", ch);
    setCursor(1,18);
    printf("%02x%02x%02x", \
            chan_table[ch].freq2, chan_table[ch].freq1, \
            chan_table[ch].freq0);
    setCursor(1, 96);
    printf("%02x%02x%02x", \
            chan_table[ch].fscal3, chan_table[ch].fscal2, \
            chan_table[ch].fscal1);
    SSN = HIGH;
	RFST = RFST_SIDLE; /* Enter idle state (freq synth turned off */

    return freq;
}
void printHeader() {
    SSN = LOW;
    setCursor(0, 0);
    printf("Freq:          Chan:");
    setCursor(1, 0);
    printf(" 0x       Cal:0x");
    setCursor(2,0);
    printf("RSSI:     Max:");
    SSN = HIGH;
}

#define UPPER(a, b, c)  ((((a) - (b) + ((c) / 2)) / (c)) * (c))
#define LOWER(a, b, c)  ((((a) + (b)) / (c)) * (c))

/* set the center frequency in MHz */
u32 set_center_freq(u16 freq) {
    /* TBD SPACING AND STEP AREN'T SET. REDO HOW CAL IS DONE */
	center_freq = freq;

	return freq;
}

/* tune the radio using stored calibration */
void tune(u8 ch) {
	FREQ2 = chan_table[ch].freq2;
	FREQ1 = chan_table[ch].freq1;
	FREQ0 = chan_table[ch].freq0;

	FSCAL3 = chan_table[ch].fscal3;
	FSCAL2 = chan_table[ch].fscal2;
	FSCAL1 = chan_table[ch].fscal1;
}

void poll_keyboard() {

	switch (getkey()) {
	case 'a':
	case 'A':
		user_freq -= STEP_1MHZ;
		break;
	case 's':
	case 'S':
		user_freq -= STEP_100KHZ;
		break;
	case 'd':
	case 'D':
		user_freq -= STEP_10KHZ;
		break;
	case 'f':
	case 'F':
		user_freq -= STEP_1KHZ;
		break;
	case 'h':
	case 'H':
		user_freq += STEP_1KHZ;
		break;
	case 'j':
	case 'J':
		user_freq += STEP_10KHZ;
		break;
	case 'k':
	case 'K':
		user_freq += STEP_100KHZ;
		break;
	case 'l':
	case 'L':
		user_freq += STEP_1MHZ;
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

void main(void) {
    u8 ch;
	u16 i;

    /* Remember: IM-ME display is 132 wide x 64 high */
    /* This gives a 22 x 8 display */

reset:
	center_freq = DEFAULT_FREQ;
	user_freq = DEFAULT_FREQ;
	sleepy = 0;

	xtalClock();
	setIOPorts();
	configureSPI();
	LCDReset();
	radio_setup();
    clear();
    printHeader();
    calibrate_freq(center_freq, 0);

	while (1) {
		poll_keyboard();

        /* TBD Mod this when more than one channel */
		if (user_freq != center_freq) {
			center_freq = calibrate_freq(user_freq, 0);
        }

		/* go to sleep (more or less a shutdown) if power button pressed */
		if (sleepy) {
			clear();
			sleepMillis(1000);
			SSN = LOW;
			LCDPowerSave();
			SSN = HIGH;

			while (1) {
				sleep();

				/* power button depressed long enough to wake? */
				sleepy = 0;
				for (i = 0; i < DEBOUNCE_COUNT; i++) {
					sleepMillis(DEBOUNCE_PERIOD);
					if (keyscan() != KPWR) sleepy = 1;
				}
				if (!sleepy) break;
			}

			/* reset on wake */
			goto reset;
		}

        for (ch = 0; ch < NUM_CHANNELS; ch++) {
            /* tune(ch); */;
            RFST = RFST_SRX;

            /* Print last result while waiting for RSSI measurement */
            SSN = LOW;
            setCursor(2, 30);
            printf("%3u", chan_table[ch].ss);
            setCursor(2, 84);
            printf("%3u", chan_table[ch].max);
            SSN = HIGH;

            /* Just in case, add a bit of time for now */
            for (i = 350; i--;);

            /* read RSSI */
            chan_table[ch].ss = (RSSI ^ 0x80);
            chan_table[ch].max = MAX(chan_table[ch].ss, \
                    chan_table[ch].max);

            /* end Rx */
            RFST = RFST_SIDLE;
        }
	}
}
