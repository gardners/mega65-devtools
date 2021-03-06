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
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "mega65.h"

struct mega65_machine_state *machine=NULL;

struct mega65_machine_state *mega65_new_machine()
{
  struct mega65_machine_state *machine = calloc(sizeof(struct mega65_machine_state),1);
  assert(machine);
  mega65_reset(machine);
  return machine;
}

int mega65_reset(struct mega65_machine_state *machine)
{
  gs4510_reset(machine);
  viciv_reset(machine);
  return 0;
}
