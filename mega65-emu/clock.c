/*
  MEGA65 Partial emulator clock function

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

// Called by the CPU whenever it wants to advance the machine clock.
int mega65_advance_clock(struct mega65_machine_state *machine, int ns)
{
  machine->clock_time+=ns;

  while(machine->viciv_time<machine->clock_time) {
    viciv_docycle(machine);
  }
  while(machine->ethernet_time<machine->clock_time) {
    ethernet_docycle(machine);
  }
  return 0;
}
