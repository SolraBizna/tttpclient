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

#ifndef CHARCONVHH
#define CHARCONVHH

#include <functional>
#include "tttp_scancodes.h"

/* if cc is given, it's a function to call when a control code (such as
   newline) is encountered; it is passed the text so far, and the scancode
   corresponding to that control code, then conversion continues, outputting
   to the beginning of outp
   (returned value - outp) = length of converted text not handled by cc*/
uint8_t* convert_utf8_to_cp437(const uint8_t* inp, uint8_t* outp, size_t inlen,
                               std::function<void(uint8_t* sofar,
                                                  size_t sofarlen,
                                                  tttp_scancode code)> cc);
/* assumes the text contains no control codes; make outp at least 3*inlen bytes
   long */
uint8_t* convert_cp437_to_utf8(const uint8_t* inp, uint8_t* outp,size_t inlen);

#endif
