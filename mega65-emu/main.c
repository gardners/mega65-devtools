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

int main(int argc,char **argv)
{
  usage();

  return 0;
}
