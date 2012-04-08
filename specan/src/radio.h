#ifndef RADIO_H
#define RADIO_H 1

#include <stdbool.h>
#include "types.h"

#ifndef FREQ_REF
#define FREQ_REF    (27000000)
#endif

void radio_init(void);
u32 setFrequency(u32 freq);

__data volatile const u8 *radio_getbuf(void);

extern void packet_rx_callback(const __data u8 *buf);
#endif
