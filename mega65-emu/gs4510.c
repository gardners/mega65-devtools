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

#include <stdio.h>
#include <stdlib.h>
#include "mega65.h"
#include "instructions.h"

int gs4510_wait_one_cycle(struct mega65_machine_state *machine)
{
  machine->cpu_time+=CPUCLOCK_PS;
  return 0;
}

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

int gs4510_set_nz(struct mega65_machine_state *machine, int v)
{
  v&=0xff;
  if (v) machine->cpu_state.flags&=~CPUFLAG_Z;
  else machine->cpu_state.flags|=CPUFLAG_Z;
  if (v&0x80) machine->cpu_state.flags|=CPUFLAG_N;
  else machine->cpu_state.flags&=~CPUFLAG_N;
  return 0;
}

int gs4510_next_instruction(struct mega65_machine_state *machine)
{

  // XXX - Advance machine->cpu_clock
  // XXX - Call mega65_advance_clock()

  int pc_address = gs4510_resolve_address(machine,machine->cpu_state.pc,MEMORY_READ);
  machine->cpu_state.pc++;
  
  // gs4510_read_memory() does the advancing of the CPU clock based on the cycles required for the read.
  int opcode = gs4510_read_memory(machine,pc_address)&0xff;
  int instruction = instruction_table[opcode].op;
  int addressing_mode = instruction_table[opcode].mode;
  int check_interrupts=1;
  int store=0;

  int reg_value;
  int reg_addr;
  int reg_vector;
  int reg_result;
  int reg_store;

  // Get value for operation
  switch(addressing_mode) {
  case addressmode_:
    // Implied or accumulator mode -- nothing much to do
    // XXX - Most implied mode instructions cannot be followed by an interrupt
    check_interrupts=0;
    reg_value = machine->cpu_state.a;
    break;
  case addressmode_rr: // 8-bit relative mode
    // Fall through
  case addressmode_Inn:
    pc_address = gs4510_resolve_address(machine,machine->cpu_state.pc,MEMORY_READ);
    machine->cpu_state.pc++;
    reg_value = gs4510_read_memory(machine,pc_address)&0xff;
    reg_addr = -1;
    break;
  case addressmode_rrrr:
    // Fall through
  case addressmode_Innnn:
    pc_address = gs4510_resolve_address(machine,machine->cpu_state.pc,MEMORY_READ);
    machine->cpu_state.pc++;
    reg_value = gs4510_read_memory(machine,pc_address)&0xff;
    pc_address = gs4510_resolve_address(machine,machine->cpu_state.pc,MEMORY_READ);
    machine->cpu_state.pc++;
    reg_value |= (gs4510_read_memory(machine,pc_address)&0xff)<<8;
    reg_addr = -1;
    break;
  case addressmode_nn: // $nn
    pc_address = gs4510_resolve_address(machine,machine->cpu_state.pc,MEMORY_READ);
    machine->cpu_state.pc++;
    reg_addr = gs4510_read_memory(machine,pc_address)&0xff;
    reg_addr |= (machine->cpu_state.b<<8);
    gs4510_wait_one_cycle(machine);
    break;
  case addressmode_A:
    reg_value = machine->cpu_state.a;
    reg_addr = -1;
    break;
  case addressmode__nnX_: // ($nn,X)
    // Get 1st argument byte
    pc_address = gs4510_resolve_address(machine,machine->cpu_state.pc,MEMORY_READ);
    machine->cpu_state.pc++;
    // Calculate address of vector
    reg_addr = gs4510_read_memory(machine,pc_address)&0xff;
    reg_addr += machine->cpu_state.x;
    reg_addr &= 0xff;
    reg_addr |= (machine->cpu_state.b<<8);
    reg_addr = gs4510_resolve_address(machine,reg_addr,MEMORY_READ);
    gs4510_wait_one_cycle(machine);
    // Read vector into reg_addr
    reg_vector = gs4510_read_memory(machine,reg_addr)&0xff;
    if ((reg_addr&0xff) == 0xff) reg_addr &= 0xff00; else reg_addr++;
    reg_vector |= (gs4510_read_memory(machine,reg_addr)&0xff)<<8;
    reg_addr = reg_vector;
    gs4510_wait_one_cycle(machine);
    break;
  case addressmode_nnnn:
    pc_address = gs4510_resolve_address(machine,machine->cpu_state.pc,MEMORY_READ);
    machine->cpu_state.pc++;
    reg_addr = gs4510_read_memory(machine,pc_address)&0xff;
    pc_address = gs4510_resolve_address(machine,machine->cpu_state.pc,MEMORY_READ);
    machine->cpu_state.pc++;
    reg_addr |= (gs4510_read_memory(machine,pc_address)&0xff)<<8;
    gs4510_wait_one_cycle(machine);
    break;
  case addressmode_nnnnX:
    pc_address = gs4510_resolve_address(machine,machine->cpu_state.pc,MEMORY_READ);
    machine->cpu_state.pc++;
    reg_addr = gs4510_read_memory(machine,pc_address)&0xff;
    pc_address = gs4510_resolve_address(machine,machine->cpu_state.pc,MEMORY_READ);
    machine->cpu_state.pc++;
    reg_addr |= (gs4510_read_memory(machine,pc_address)&0xff)<<8;
    reg_addr += machine->cpu_state.x;
    reg_addr &= 0xffff;
    gs4510_wait_one_cycle(machine);
    break;
  case addressmode_nnnnY:
    pc_address = gs4510_resolve_address(machine,machine->cpu_state.pc,MEMORY_READ);
    machine->cpu_state.pc++;
    reg_addr = gs4510_read_memory(machine,pc_address)&0xff;
    pc_address = gs4510_resolve_address(machine,machine->cpu_state.pc,MEMORY_READ);
    machine->cpu_state.pc++;
    reg_addr |= (gs4510_read_memory(machine,pc_address)&0xff)<<8;
    reg_addr += machine->cpu_state.y;
    reg_addr &= 0xffff;
    gs4510_wait_one_cycle(machine);
    break;
  case addressmode__nn_Y: // ($nn),Y
    // Get 1st argument byte
    pc_address = gs4510_resolve_address(machine,machine->cpu_state.pc,MEMORY_READ);
    machine->cpu_state.pc++;
    // Calculate address of vector
    reg_addr = gs4510_read_memory(machine,pc_address)&0xff;
    reg_addr |= (machine->cpu_state.b<<8);
    reg_addr = gs4510_resolve_address(machine,reg_addr,MEMORY_READ);
    gs4510_wait_one_cycle(machine);
    // Read vector into reg_addr
    reg_vector = gs4510_read_memory(machine,reg_addr)&0xff;
    if ((reg_addr&0xff) == 0xff) reg_addr &= 0xff00; else reg_addr++;
    reg_vector |= (gs4510_read_memory(machine,reg_addr)&0xff)<<8;
    reg_vector = (reg_vector&0xff00)|((reg_vector+machine->cpu_state.y)&0xff);
    gs4510_wait_one_cycle(machine);
    break;
  case addressmode__nn_Z: // ($nn),Z
    // Get 1st argument byte
    pc_address = gs4510_resolve_address(machine,machine->cpu_state.pc,MEMORY_READ);
    machine->cpu_state.pc++;
    // Calculate address of vector
    reg_addr = gs4510_read_memory(machine,pc_address)&0xff;
    reg_addr |= (machine->cpu_state.b<<8);
    reg_addr = gs4510_resolve_address(machine,reg_addr,MEMORY_READ);
    gs4510_wait_one_cycle(machine);
    // Read vector into reg_addr
    reg_vector = gs4510_read_memory(machine,reg_addr)&0xff;
    if ((reg_addr&0xff) == 0xff) reg_addr &= 0xff00; else reg_addr++;
    reg_vector |= (gs4510_read_memory(machine,reg_addr)&0xff)<<8;
    reg_addr = (reg_vector&0xff00)|((reg_vector+machine->cpu_state.z)&0xff);
    gs4510_wait_one_cycle(machine);
    break;
  case addressmode__nnSP_Y:
    // Get 1st argument byte
    pc_address = gs4510_resolve_address(machine,machine->cpu_state.pc,MEMORY_READ);
    machine->cpu_state.pc++;
    // Calculate address of vector
    reg_addr = gs4510_read_memory(machine,pc_address)&0xff;
    reg_addr += (machine->cpu_state.sph<<8);
    reg_addr += machine->cpu_state.spl;
    reg_addr &= 0xffff;
    reg_addr = gs4510_resolve_address(machine,reg_addr,MEMORY_READ);
    gs4510_wait_one_cycle(machine);
    // Read vector into reg_addr
    reg_vector = gs4510_read_memory(machine,reg_addr)&0xff;
    if ((reg_addr&0xff) == 0xff) reg_addr &= 0xff00; else reg_addr++;
    reg_vector |= (gs4510_read_memory(machine,reg_addr)&0xff)<<8;
    // XXX - 32-bit vector support goes here.

    reg_addr = (reg_vector&0xff00)|((reg_vector+machine->cpu_state.y)&0xff);
    gs4510_wait_one_cycle(machine);
    break;
  case addressmode_nnX:
    pc_address = gs4510_resolve_address(machine,machine->cpu_state.pc,MEMORY_READ);
    machine->cpu_state.pc++;
    reg_addr = (gs4510_read_memory(machine,pc_address)+machine->cpu_state.x)&0xff;
    reg_addr |= (machine->cpu_state.b<<8);
    gs4510_wait_one_cycle(machine);
    break;
  case addressmode_nnY:
    pc_address = gs4510_resolve_address(machine,machine->cpu_state.pc,MEMORY_READ);
    machine->cpu_state.pc++;
    reg_addr = (gs4510_read_memory(machine,pc_address)+machine->cpu_state.y)&0xff;
    reg_addr |= (machine->cpu_state.b<<8);
    gs4510_wait_one_cycle(machine);
    break;
  case addressmode__nnnn_:
    // Get address of vector
    pc_address = gs4510_resolve_address(machine,machine->cpu_state.pc,MEMORY_READ);
    machine->cpu_state.pc++;
    reg_vector = gs4510_read_memory(machine,pc_address)&0xff;
    pc_address = gs4510_resolve_address(machine,machine->cpu_state.pc,MEMORY_READ);
    machine->cpu_state.pc++;
    reg_vector |= (gs4510_read_memory(machine,pc_address)&0xff)<<8;
    // Now read the two bytes at that location
    reg_addr = gs4510_resolve_address(machine,reg_vector,MEMORY_READ);
    reg_value = gs4510_read_memory(machine,reg_addr)&0xff;
    reg_vector++; reg_vector &= 0xffff;
    reg_addr = gs4510_resolve_address(machine,reg_vector,MEMORY_READ);
    reg_addr = reg_value | ((gs4510_read_memory(machine,reg_addr)&0xff)<<8);
    // XXX - 32-bit jmp/jsr support goes here
    break;
  case addressmode__nnnnX_:
    // Get address of vector
    pc_address = gs4510_resolve_address(machine,machine->cpu_state.pc,MEMORY_READ);
    machine->cpu_state.pc++;
    reg_vector = gs4510_read_memory(machine,pc_address)&0xff;
    pc_address = gs4510_resolve_address(machine,machine->cpu_state.pc,MEMORY_READ);
    machine->cpu_state.pc++;
    reg_vector |= (gs4510_read_memory(machine,pc_address)&0xff)<<8;
    reg_vector += machine->cpu_state.x;
    reg_vector &= 0xffff;
    // Now read the two bytes at that location
    reg_addr = gs4510_resolve_address(machine,reg_vector,MEMORY_READ);
    reg_value = gs4510_read_memory(machine,reg_addr)&0xff;
    reg_vector++; reg_vector &= 0xffff;
    reg_addr = gs4510_resolve_address(machine,reg_vector,MEMORY_READ);
    reg_addr = reg_value | ((gs4510_read_memory(machine,reg_addr)&0xff)<<8);
    // XXX - 32-bit jmp/jsr support goes here
    break;
  case addressmode_nnrr:
  default:
    fprintf(stderr,"Addressing mode 0x%02x not supported.\n",
	    addressing_mode);
    exit(-1);
    break;
  }

  // Do memory read if required. Also process all implied mode instructions and other
  // branch and utility instructions.
  switch (instruction) {
  case INSTR_BBR:
    break;
  case INSTR_BBS:
    break;
  case INSTR_BCC:
    break;
  case INSTR_BCS:
    break;
  case INSTR_BEQ:
    break;
  case INSTR_BMI:
    break;
  case INSTR_BNE:
    break;
  case INSTR_BPL:
    break;
  case INSTR_BRA:
    break;
  case INSTR_BRK:
    // XXX not yet implemented
    break;
  case INSTR_BSR:
    break;
  case INSTR_BVC:
    break;
  case INSTR_BVS:
    break;    
  case INSTR_CLC: machine->cpu_state.flags&=~CPUFLAG_C; return 0;
  case INSTR_CLD: machine->cpu_state.flags&=~CPUFLAG_D; return 0;
  case INSTR_CLE: machine->cpu_state.flags&=~CPUFLAG_E; return 0;
  case INSTR_CLI: machine->cpu_state.flags&=~CPUFLAG_I; return 0;
  case INSTR_CLV: machine->cpu_state.flags&=~CPUFLAG_V; return 0;
  case INSTR_DEX: gs4510_set_nz(machine,--machine->cpu_state.x); return 0;
  case INSTR_DEY: gs4510_set_nz(machine,--machine->cpu_state.y); return 0;
  case INSTR_DEZ: gs4510_set_nz(machine,--machine->cpu_state.z); return 0;
  case INSTR_EOM: machine->cpu_state.mapping=0; return 0;
  case INSTR_INX: gs4510_set_nz(machine,++machine->cpu_state.x); return 0;
  case INSTR_INY: gs4510_set_nz(machine,++machine->cpu_state.y); return 0;
  case INSTR_INZ: gs4510_set_nz(machine,++machine->cpu_state.z); return 0;
  case INSTR_JMP:
    break;
  case INSTR_JSR:
    break;
  case INSTR_MAP:
    machine->cpu_state.mapping=1;
    check_interrupts=0;
    if (machine->cpu_state.x==0x0f) machine->cpu_state.map_lo_mb=machine->cpu_state.a;
    if (machine->cpu_state.y==0x0f) machine->cpu_state.map_hi_mb=machine->cpu_state.y;
    machine->cpu_state.map_lo_offset =
      ((machine->cpu_state.x&0xf)<<8)|machine->cpu_state.a;
    machine->cpu_state.map_hi_offset =
      ((machine->cpu_state.y&0xf)<<8)|machine->cpu_state.y;
    machine->cpu_state.map_lo_bitmask=machine->cpu_state.x>>4;
    machine->cpu_state.map_hi_bitmask=machine->cpu_state.z>>4;
    return 0;
  case INSTR_NEG:
    machine->cpu_state.a=0xff-machine->cpu_state.a;
    gs4510_set_nz(machine,machine->cpu_state.a);
    return 0;
  case INSTR_PHA:
    break;
  case INSTR_PHX:
    break;
  case INSTR_PHY:
    break;
  case INSTR_PHZ:
    break;
  case INSTR_PLA:
    break;
  case INSTR_PLX:
    break;
  case INSTR_PLY:
    break;
  case INSTR_PLZ:
    break;
  case INSTR_RTI:
    break;
  case INSTR_RTS:
    break;
  case INSTR_SEC: machine->cpu_state.flags|=CPUFLAG_C; return 0;
  case INSTR_SED: machine->cpu_state.flags|=CPUFLAG_D; return 0;
  case INSTR_SEE: machine->cpu_state.flags|=CPUFLAG_E; return 0;
  case INSTR_SEI: machine->cpu_state.flags|=CPUFLAG_I; return 0;
    // XXX - set CPU flags based on the following instructions
  case INSTR_TAB: machine->cpu_state.b=machine->cpu_state.a; return 0;
  case INSTR_TBA: machine->cpu_state.a=machine->cpu_state.b; return 0;
  case INSTR_TAX: machine->cpu_state.x=machine->cpu_state.a; return 0;
  case INSTR_TAY: machine->cpu_state.y=machine->cpu_state.a; return 0;
  case INSTR_TAZ: machine->cpu_state.z=machine->cpu_state.a; return 0;
  case INSTR_TSX: machine->cpu_state.x=machine->cpu_state.spl; return 0;
  case INSTR_TSY: machine->cpu_state.y=machine->cpu_state.sph; return 0;
  case INSTR_TXA: machine->cpu_state.a=machine->cpu_state.x; return 0;
  case INSTR_TXS: machine->cpu_state.spl=machine->cpu_state.x; return 0;
  case INSTR_TYA: machine->cpu_state.a=machine->cpu_state.y; return 0;
  case INSTR_TYS: machine->cpu_state.sph=machine->cpu_state.y; return 0;
  case INSTR_TZA: machine->cpu_state.a=machine->cpu_state.z; return 0;
  case INSTR_STA:
  case INSTR_STX:
  case INSTR_STY:
  case INSTR_STZ:
    // no read for store instructions
    break;
  default:
    // Do memory fetch
    if (reg_addr!=-1) {
      reg_addr = gs4510_resolve_address(machine,reg_vector,MEMORY_READ);
      reg_value = gs4510_read_memory(machine,reg_addr)&0xff;
    }
  }

  // Now do instructions that operate on the read value
  switch (instruction) {
  case INSTR_ADC:
    break;
  case INSTR_AND:
    break;
  case INSTR_ASL:
    reg_result = (reg_value<<1)&0xfe;
    if (machine->cpu_state.flags&CPUFLAG_C) reg_result|=0x01;
    if (reg_value&0x80) machine->cpu_state.flags|=CPUFLAG_C;
    else machine->cpu_state.flags&=~CPUFLAG_C;
    if (addressing_mode != addressmode_) {      
      store = 1;
      reg_store = reg_result;
    } else
      machine->cpu_state.a = reg_result;
      gs4510_set_nz(machine,machine->cpu_state.a);
    break;
  case INSTR_ASR:
    break;
  case INSTR_BIT:
    break;
  case INSTR_CMP:
    break;
  case INSTR_CPX:
    break;
  case INSTR_CPY:
    break;
  case INSTR_CPZ:
    break;
  case INSTR_DEC:
    break;
  case INSTR_DEW:
    break;
  case INSTR_EOR:
    break;
  case INSTR_INC:
    break;
  case INSTR_INW:
    break;
  case INSTR_LDA:
    machine->cpu_state.a = reg_value;
    gs4510_set_nz(machine,reg_value);
    break;
  case INSTR_LDX:
    machine->cpu_state.x = reg_value;
    gs4510_set_nz(machine,reg_value);
    break;
  case INSTR_LDY:
    machine->cpu_state.y = reg_value;
    gs4510_set_nz(machine,reg_value);
    break;
  case INSTR_LDZ:
    machine->cpu_state.z = reg_value;
    gs4510_set_nz(machine,reg_value);
    break;
  case INSTR_LSR:
    reg_result = (reg_value>>1);
    if (reg_value&1) machine->cpu_state.flags|=CPUFLAG_C;
    else machine->cpu_state.flags&=~CPUFLAG_C;
    if (addressing_mode != addressmode_) {      
      store = 1;
      reg_store = reg_result;
    } else
      machine->cpu_state.a = reg_result;
      gs4510_set_nz(machine,machine->cpu_state.a);
    break;
  case INSTR_ORA:
    break;
  case INSTR_RMB:
    break;
  case INSTR_ROL:
    reg_result = (reg_value<<1)&0xfe;
    if (machine->cpu_state.flags&CPUFLAG_C) reg_result|=0x01;
    if (reg_value&0x80) machine->cpu_state.flags|=CPUFLAG_C;
    else machine->cpu_state.flags&=~CPUFLAG_C;
    if (addressing_mode != addressmode_) {      
      store = 1;
      reg_store = reg_result;
    } else
      machine->cpu_state.a = reg_result;
      gs4510_set_nz(machine,machine->cpu_state.a);
    break;
  case INSTR_ROR:
    reg_result = (reg_value>>1);
    if (machine->cpu_state.flags&CPUFLAG_C) reg_result|=0x80;
    if (reg_value&1) machine->cpu_state.flags|=CPUFLAG_C;
    else machine->cpu_state.flags&=~CPUFLAG_C;
    if (addressing_mode != addressmode_) {      
      store = 1;
      reg_store = reg_result;
    } else
      machine->cpu_state.a = reg_result;
      gs4510_set_nz(machine,machine->cpu_state.a);
    break;
  case INSTR_ROW:
    break;
  case INSTR_SBC:
    break;
  case INSTR_SMB:
    break;
  case INSTR_STA:
    store=1;
    reg_store = machine->cpu_state.a;
    break;
  case INSTR_STX:
    store=1;
    reg_store = machine->cpu_state.x;
    break;
  case INSTR_STY:
    store=1;
    reg_store = machine->cpu_state.y;
    break;
  case INSTR_STZ:
    store=1;
    reg_store = machine->cpu_state.z;
    break;
  case INSTR_TRB:
    break;
  case INSTR_TSB:
    break;
  default:
    break;
  }

  if (store) {
    // Resolve store address

    // store reg_store 
  }
  
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

  return 0;
}
