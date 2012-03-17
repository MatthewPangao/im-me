/*
 * Copyright 2010 Michael Ossmann
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

/*
 * There is one channel per column of the display.  The radio is tuned to one
 * channel at a time and RSSI is displayed for that channel.
 */
#define NUM_CHANNELS 1

/*
 * Normal IM-ME devices have a 26 MHz crystal, but
 * some of them are shipping with 27 MHz crystals.
 * Use this preprocessor macro to compensate if your
 * IMME is so afflicted. See this link for more info:
 * <http://madscientistlabs.blogspot.com/2011/03/fix-for-im-me-specan-frequency-offset.html>
 */
#ifndef FREQ_REF
#define FREQ_REF	(27000000)
#endif

/* frequencies in Hz */
#define DEFAULT_FREQ     (902382395)
#define STEP_1MHZ        (1000000)
#define STEP_100KHZ      (100000)
#define STEP_10KHZ       (10000)
#define STEP_1KHZ        (1000)

/* band limits in MHz */
#define MIN_900  848
#define MAX_900  962

/* power button debouncing for wake from sleep */
#define DEBOUNCE_COUNT  4
#define DEBOUNCE_PERIOD 50

#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))

/* Keeping track of all this for each channel allows us to tune faster. */
typedef struct {
    /* frequency in Hz */
    u32 freq;

	/* frequency setting */
	u8 freq2;
	u8 freq1;
	u8 freq0;
	
	/* frequency calibration */
	u8 fscal3;
	u8 fscal2;
	u8 fscal1;

	/* signal strength */
	u8 ss;
	u8 max;
} channel_info;

void clear();
void putchar(char c);
u8 getkey();
void radio_setup();
void printHeader();
void set_radio_freq(u32 freq);
u32 calibrate_freq(u32 freq, u8 ch);
u32 set_center_freq(u16 freq);
void tune(u8 ch);
void poll_keyboard();
void main(void);
