/*
  MEGA65 Partial emulator GS4510 CPU

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

#include "mega65.h"
#include "instructions.h"

int gs4510_read_memory(struct mega65_machine_state *machine, int long_address)
{
  if (long_address<0x1f800) {
    // shadow ram of chipram    
    machine->cpu_time+=CPUCLOCK_PS;
    return machine->shadowram[long_address&0x1ffff];
  } else if (long_address<0x20000) {
    // First 2KB of colour RAM
    machine->cpu_time+=2*CPUCLOCK_PS;
    return machine->colourram[long_address&0x7ff];    
  } else if (long_address<0x40000) {
    // ROM RAM
    machine->cpu_time+=CPUCLOCK_PS;
    return machine->romram[long_address&0x1ffff];
  } else {
    // XXX - All the rest -- currently unimplemented
    machine->cpu_time+=CPUCLOCK_PS;
    return 0xbd;
  }  

}

// follows resolve_address_to_long() in gs4510.vhdl
int gs4510_resolve_address(struct mega65_machine_state *machine,int short_address,
			   int writeP)
{
  int blocknum = (short_address >>12);

  int lhc = machine->cpu_state.cpuport_value&7;
  lhc|=(~machine->cpu_state.cpuport_ddr)&7;

  int temp_address = short_address & 0xfff;

  if (blocknum==13) {
    if (writeP) {
      switch(lhc) {
      case 0x0: case 0x1: case 0x2: case 0x3: case 0x4:
	// write to RAM behind IO
	temp_address|=0x000d000; break;
      default:
	// IO access
	temp_address|=0xffd3000; 
	temp_address|=(machine->viciv_state.viciii_iomode<<12);
	break;
      }
    } else {
      switch(lhc) {
      case 0x0: case 0x4:
	// read RAM behind IO
	temp_address|=0x000d000; break;
      case 0x1: case 0x2: case 0x3:
	// read charrom
	temp_address|=0x002d000; break;
      default:
	// IO access
	temp_address|=0xffd3000; 
	temp_address|=(machine->viciv_state.viciii_iomode<<12);
	break;
      }      
    }
  }

  // Kernal ROM
  if ((machine->cpu_state.map_hi_bitmask&0x8)==0) {
    if (((blocknum&0xe)==0xe)&&(lhc&2)&&(!writeP)) {
      temp_address&=0xffff;
      temp_address|=0x0020000;
    }
  }
  // Kernal ROM
  if ((machine->cpu_state.map_hi_bitmask&0x2)==0) {
    if (((blocknum&0xe)==0xa)&&(lhc&2)&&(!writeP)) {
      temp_address&=0xffff;
      temp_address|=0x0020000;
    }
  }

  // Memory remapping
  int b=short_address>>14;
  if (short_address&0x8000) {
    if (machine->cpu_state.map_hi_bitmask&(1<<b)) {
      temp_address&=0xff;
      temp_address|=machine->cpu_state.map_hi_mb<<20;
      temp_address|=((machine->cpu_state.map_hi_offset<<8)
		     +(short_address&0xff00))&0xfff00;
    }
  } else {
    if (machine->cpu_state.map_lo_bitmask&(1<<b)) {
      temp_address&=0xff;
      temp_address|=machine->cpu_state.map_lo_mb<<20;
      temp_address|=((machine->cpu_state.map_lo_offset<<8)
		     +(short_address&0xff00))&0xfff00;
    }
  }

  // VICIV ROM select lines
  if (((blocknum&0xe)==0xe)&&(machine->viciv_state.rom_at_e000)) {
    temp_address&=0xfff;
    temp_address|=0x003E;
  }
  if (((blocknum&0xf)==0xc)&&(machine->viciv_state.rom_at_c000)) {
    temp_address&=0xfff;
    temp_address|=0x002C;
  }
  if (((blocknum&0xe)==0x8)&&(machine->viciv_state.rom_at_8000)) {
    temp_address&=0x1fff;
    temp_address|=0x0038;
  }

  return temp_address;
}

int gs4510_next_instruction(struct mega65_machine_state *machine)
{

  // XXX - Advance machine->cpu_clock
  // XXX - Call mega65_advance_clock()

  int pc_address = gs4510_resolve_address(machine,machine->cpu_state.pc,MEMORY_READ);

  // gs4510_read_memory() does the advancing of the CPU clock based on the cycles required for the read.
  int opcode = gs4510_read_memory(machine,pc_address)&0xff;
  int instruction = instruction_table[opcode].op;
  int addressing_mode = instruction_table[opcode].mode;

  return 0;
}

int gs4510_setup_hypervisor_entry(struct mega65_machine_state *machine,
				  int trap)
{
  // Hypervisor entry point
  machine->cpu_state.pc=0x8000+(trap<<2);
  // Hypervisor mode
  machine->cpu_state.hypervisor_mode=1;

  // MAP 16KB hypervisor ROM/RAM
  machine->cpu_state.map_hi_offset=0xf00;
  machine->cpu_state.map_hi_bitmask=0x3;
  machine->cpu_state.map_hi_mb=0xff;

  // Point B and SP to the end of the 16KB of hypervisor ROM/RAM
  machine->cpu_state.b=0xbf;
  machine->cpu_state.sph=0xbe;

  // Set 8-bit stack and set I to disable further interrupts
  machine->cpu_state.flags|=CPUFLAG_I|CPUFLAG_E;

  // Enable MEGA65 IO
  machine->viciv_state.viciii_iomode=3;

  return 0;
}

int gs4510_reset(struct mega65_machine_state *machine)
{
  // Why on earth do we preset the low map with the following?
  machine->cpu_state.map_lo_offset=0x000;
  machine->cpu_state.map_lo_bitmask=0x4;
  machine->cpu_state.map_lo_mb=0x80;

  machine->cpu_state.flags=CPUFLAG_I|CPUFLAG_E;
  machine->cpu_state.sph=0x01;
  machine->cpu_state.spl=0xff;
  
  machine->cpu_state.cpuport_value=0x35;
  machine->cpu_state.cpuport_ddr=0xff;
  
  gs4510_setup_hypervisor_entry(machine,0x40);
}
