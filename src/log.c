/*
 * cwdaemon - morse sounding daemon for the parallel or serial port
 * Copyright (C) 2002 - 2005 Joop Stakenborg <pg4i@amsat.org>
 *		        and many authors, see the AUTHORS file.
 * Copyright (C) 2012 - 2023 Kamil Ignacak <acerion@wp.pl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Library General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */




#include "log.h"




/**
   @brief Get string label corresponding to given log priority
*/
const char * log_get_priority_label(int priority)
{
   switch (priority) {
   case CWDAEMON_VERBOSITY_E:
      return "ERR";
   case CWDAEMON_VERBOSITY_W:
      return "WARN";
   case CWDAEMON_VERBOSITY_I:
      return "INFO";
   case CWDAEMON_VERBOSITY_D:
      return "DEBUG";
   case CWDAEMON_VERBOSITY_N:  /* "N" == None (don't display logs). */
   default:
      return "--";
   }
}

