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

#ifndef BREAKLINESHH
#define BREAKLINESHH

#include "tttpclient.hh"
#include <string>
#include <vector>

std::vector<std::string> break_lines(const std::string& message,
                                     unsigned int line_width);

#endif
