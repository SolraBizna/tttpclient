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

#include "break_lines.hh"

std::vector<std::string> break_lines(const std::string& message,
                                     unsigned int line_width) {
  std::vector<std::string> lines;
  auto start = message.begin();
  auto wordbreak = start;
  auto here = start;
  while(here != message.end()) {
    if(*here == '\n') {
      lines.emplace_back(start, here);
      ++here;
      start = wordbreak = here;
    }
    else if(*here == ' ') {
      ++here;
      wordbreak = here;
    }
    else {
      unsigned int line_size_with_this_char = here - start + 1;
      if(line_size_with_this_char > line_width) {
        if(start == wordbreak) {
          lines.emplace_back(start, here);
          start = wordbreak = here;
        }
        else {
          lines.emplace_back(start, wordbreak);
          start = wordbreak;
        }
      }
      ++here;
    }
  }
  if(here != start)
    lines.emplace_back(start, here);
  for(auto& line : lines) {
    while(line.length() > 0 && *line.crbegin() == ' ')
      line.resize(line.length()-1);
    if(line.length() > line_width)
      throw std::string("bug in line breaking algorithm");
  }
  return lines;
}
