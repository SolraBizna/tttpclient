/*
  Copyright (C) 2015-2016 Solra Bizna

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MAC16HH
#define MAC16HH

#include "tttpclient.hh"

// Apple 16-color palette
extern const uint8_t mac16[48];
enum {
  MAC16_WHITE=0,
  MAC16_YELLOW,
  MAC16_ORANGE,
  MAC16_RED,
  MAC16_MAGENTA,
  MAC16_PURPLE,
  MAC16_BLUE,
  MAC16_CYAN,
  MAC16_GREEN,
  MAC16_DARK_GREEN,
  MAC16_BROWN,
  MAC16_TAN,
  MAC16_LIGHT_GRAY,
  MAC16_GRAY,
  MAC16_DARK_GRAY,
  MAC16_BLACK
};

#endif
