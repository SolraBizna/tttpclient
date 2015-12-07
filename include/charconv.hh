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
