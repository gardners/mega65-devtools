/*
  MEGA65 Partial emulator machine state.

  (C) Copyright Paul Gardner-Stephen 2015.

-- *  This program is free software; you can redistribute it and/or modify
-- *  it under the terms of the GNU General Public License as
-- *  published by the Free Software Foundation; either version 3 of the
-- *  License, or (at your option) any later version.
-- *
-- *  This program is distributed in the hope that it will be useful,
-- *  but WITHOUT ANY WARRANTY; without even the implied warranty of
-- *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- *  GNU General Public License for more details.
-- *
-- *  You should have received a copy of the GNU General Public License
-- *  along with this program; if not, write to the Free Software
-- *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
-- *  02111-1307  USA.

*/

struct gs4510_cpu_state {
  // User-visible CPU state
  unsigned short pc;
  unsigned char a;
  unsigned char x;
  unsigned char y;
  unsigned char z;
  unsigned char spl;
  unsigned char sph;
  unsigned char b;
  unsigned char flags;

  // 4510 MAP state
  unsigned short map_lo_offset;
  unsigned short map_hi_offset;
  unsigned char map_lo_bitmask;
  unsigned char map_hi_bitmask;
  // GS4510 MAP extensions
  unsigned char map_lo_mb;
  unsigned char map_hi_mb;

  // XXX - Hypervisor and other stuff to add here
  
};

struct viciv_state {
  int frame_x;
  int frame_y;

  // XXX - Lots of registers to add here
};

struct mega65_machine_state {
  // Memory arrays.
  // Lower 8 bits are contents.
  // Upper 8 bits are flags, e.g., for breakpoints and watchpoints.
  unsigned short chipram[128*1024];
  unsigned short shadowram[128*1024];
  unsigned short romram[128*1024];
  unsigned short colourram[32*1024];
  unsigned short hypervisor[16*1024];

  struct gs4510_cpu_state cpu_state;

  struct viciv_state viciv_state;

  // Time point of each major component of the machine
  long long cpu_time;
  long long clock_time;
  long long viciv_time;
  long long ethernet_time;
};

extern struct mega65_machine_state *machine;

struct mega65_machine_state *mega65_new_machine();
int gs4510_next_instruction(struct mega65_machine_state *machine);
int mega65_advance_clock(struct mega65_machine_state *machine, int ns);

#define addressmode_ 0
#define addressmode__nnX_ 1
#define addressmode_nn 2
#define addressmode_Inn 3
#define addressmode_A 4
#define addressmode_nnnn 5
#define addressmode_rr 6
#define addressmode__nn_Y 7
#define addressmode_nnrr 8
#define addressmode_nnnnX 9
#define addressmode_nnnnY 10
#define addressmode_nnX 11
#define addressmode_nnY 12
#define addressmode__nnnn_ 13
#define addressmode__nnnnX_ 14
#define addressmode_rrrr 15
#define addressmode__nn_Z 16
#define addressmode__nnSP_Y 17
#define addressmode_Innnn 18

