/*
  MEGA65 Partial emulator main function.

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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "mega65.h"

int usage()
{
  fprintf(stderr,
	  "MEGA65 Partial Emulator for software developers.\n"
	  "\n"
	  "usage: mega65-emu\n"
	  );
  exit(-1);
}

int load_file(char *filename,unsigned short *mem,int length)
{
  FILE *f=fopen(filename,"r");
  if (!f) {
    fprintf(stderr,"Could not load memory file '%s': could not open\n",filename);
    exit(-1);
  }

  int count=0;

  while(count<length) {
    int c = fgetc(f);
    if (c!=EOF) mem[count++]=c; else {
      if (count<length) {
	fprintf(stderr,"Could not load memory file '%s': too short",filename);
	exit(-1);    
      }
      if (count>length) {
	fprintf(stderr,"Could not load memory file '%s': too long",filename);
	exit(-1);    
      }
    }
  }

  fclose(f);
  return 0;
  
}

int main(int argc,char **argv)
{
  if (argc>1) usage();

  // Create new machine instance
  machine = mega65_new_machine();
  assert(machine);

  load_file("kickstart65gs.bin",machine->hypervisor,16*1024);
  
  // Run machine.
  // CPU triggers VIC-IV and other component cycles as apparent time elapses
  // by calling mega65_cpu_time_elapsed(nanoseconds)
  while(1) {
    printf("PC=$%04x, A=$%02x, X=$%02x, Y=$%02x, Z= $%02x, "
	   "MAPLO=$%02x:%c%c%c%c:$%02x, "
	   "MAPHI=$%02x:%c%c%c%c:$%02x\n",
	   machine->cpu_state.pc,
	   machine->cpu_state.a,
	   machine->cpu_state.x,
	   machine->cpu_state.y,
	   machine->cpu_state.z,
	   machine->cpu_state.map_lo_mb,
	   machine->cpu_state.map_lo_bitmask&1?'1':'0',
	   machine->cpu_state.map_lo_bitmask&2?'1':'0',
	   machine->cpu_state.map_lo_bitmask&4?'1':'0',
	   machine->cpu_state.map_lo_bitmask&8?'1':'0',
	   machine->cpu_state.map_lo_offset,
	   machine->cpu_state.map_hi_mb,
	   machine->cpu_state.map_hi_bitmask&1?'1':'0',
	   machine->cpu_state.map_hi_bitmask&2?'1':'0',
	   machine->cpu_state.map_hi_bitmask&4?'1':'0',
	   machine->cpu_state.map_hi_bitmask&8?'1':'0',
	   machine->cpu_state.map_hi_offset
	   );
    gs4510_next_instruction(machine);
  }

  return 0;
}
