/*
 *  ax25.cpp
 *  trackuino
 *
 *  Created by javi on 30/01/10.
 *  Copyright 2010 __MyCompanyName__. All rights reserved.
 *
 */

#include "ax25.h"
#include "modem.h"

// These are shared with the modem module
extern unsigned char packet[512];
extern int packet_size; // in bits

// Module globals
unsigned short int crc;
int ones_in_a_row;

// Constants
const int TX_DELAY = 75;  // real tx delay = TX_DELAY * 6.67 ms (@ 1200 baud)


void
ax25_update_crc(unsigned char bit) 
{
  crc ^= bit;
  if (crc & 1)
    crc = (crc >> 1) ^ 0x8408;  // X-modem CRC poly
  else
    crc = crc >> 1;
}

void
ax25_send_byte(unsigned char byte)
{
  int i;
  for (i = 0; i < 8; i++) {
    ax25_update_crc((byte >> i) & 1);
    if ((byte >> i) & 1) {
      packet[packet_size >> 3] |= (1 << (packet_size & 7));
      packet_size++;
      if (++ones_in_a_row < 5) continue;
    }
    packet[packet_size >> 3] &= ~(1 << (packet_size & 7));
    packet_size++;
    ones_in_a_row = 0;
  }
}

void
ax25_send_flag()
{
  unsigned char byte = 0x7e;
  int i;
  for (i = 0; i < 8; i++, packet_size++) {
    if ((byte >> i) & 1)
      packet[packet_size >> 3] |= (1 << (packet_size & 7));
    else
      packet[packet_size >> 3] &= ~(1 << (packet_size & 7));
  }
}

void
ax25_send_string(const char *string)
{
  int i;
  for (i = 0; string[i]; i++) {
    ax25_send_byte(string[i]);
  }
}

void
ax25_send_header(const struct s_address *addresses, int num_addresses)
{
  int i, j;
  packet_size = 0;
  ones_in_a_row = 0;
  crc = 0xffff;
  
  // Send TXDELAY flags (TXDELAY * 8 / 1200 segs)
  for (i = 0; i < TX_DELAY; i++) {
    ax25_send_flag();
  }
  
  for (i = 0; i < num_addresses; i++) {
    // Transmit callsign
    for (j = 0; addresses[i].callsign[j]; j++)
      ax25_send_byte(addresses[i].callsign[j] << 1);
    // Transmit pad
    for ( ; j < 6; j++)
      ax25_send_byte(' ' << 1);
    // Transmit SSID. Termination signaled with last bit = 1
    if (i == num_addresses - 1)
      ax25_send_byte(('0' + addresses[i].ssid) << 1 | 1);
    else
      ax25_send_byte(('0' + addresses[i].ssid) << 1);
  }
  
  // Control field: 3 = APRS-UI frame
  ax25_send_byte(0x03);
  
  // Protocol ID: 0xf0 = no layer 3 data
  ax25_send_byte(0xf0);
}

void 
ax25_send_footer()
{
  // Save the crc so that ax25_send_byte doesn't change it half way
  unsigned short int final_crc = crc;
  
  // Send the CRC
  ax25_send_byte(~(final_crc & 0xff));
  final_crc >>= 8;
  ax25_send_byte(~(final_crc & 0xff));
  
  // Signal the end of frame
  ax25_send_flag();
}

void
ax25_flush_frame()
{
  // Key the transmitter and send the frame
  modem_flush_frame();
}

