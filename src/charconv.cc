#include "charconv.hh"

#include "gen/char_table.hh"

uint8_t* convert_utf8_to_cp437(const uint8_t* inp, uint8_t* outp, size_t inlen,
                               std::function<void(uint8_t* sofar,
                                                  size_t sofarlen,
                                                  tttp_scancode code)> cc) {
  uint8_t* orig_outp = outp;
  while(inlen > 0) {
    uint8_t a = *inp++; --inlen;
    tttp_scancode scancode = (tttp_scancode)0;
    switch(a) {
    case 0x8: case 0x9: case 0x10: case 0x1B: case 0x7F:
      scancode = (tttp_scancode)a;
      break;
    }
    if(scancode) {
      if(cc) {
        cc(orig_outp, outp - orig_outp, scancode);
        outp = orig_outp;
      }
      continue;
    }
    if(a < 0x20) continue; // unhandled control code
    else if(a < 0x80) *outp++ = a;
    else if(a < 0xC0) {
      continue; /* invalid code unit */
    }
    else {
      uint32_t code_point;
      if(a < 0xE0) {
        /* 2-byte code */
        if(inlen < 1) continue; /* invalid code unit */
        uint8_t b = *inp++; --inlen;
        if(b < 0x80 || b >= 0xC0) continue;
        code_point = ((a&0x1F)<<7)|(b&0x3F);
        if(code_point < 0x80) continue; /* wasteful coding */
      }
      else if(a < 0xF0) {
        /* 3-byte code */
        if(inlen < 2) continue; /* invalid code unit */
        uint8_t b = *inp++; --inlen;
        if(b < 0x80 || b >= 0xC0) continue;
        uint8_t c = *inp++; --inlen;
        if(c < 0x80 || c >= 0xC0) continue;
        code_point = ((a&0xF)<<14)|((b&0x3f)<<7)|(c&0x3F);
        if(code_point < 0x800) continue; /* wasteful coding */
      }
      else if(a < 0xF8) {
        /* 4-byte code */
        if(inlen < 3) continue; /* invalid code unit */
        uint8_t b = *inp++; --inlen;
        if(b < 0x80 || b >= 0xC0) continue;
        uint8_t c = *inp++; --inlen;
        if(c < 0x80 || c >= 0xC0) continue;
        uint8_t d = *inp++; --inlen;
        if(d < 0x80 || d >= 0xC0) continue;
        code_point = ((a&0xF)<<21)|((b&0x3f)<<14)|((c&0x3F)<<7)|(d&0x3F);
        if(code_point < 0x10000) continue; /* wasteful coding */
      }
      else {
        /* 5- or 6-byte code, always invalid */
        continue;
      }
      switch(code_point) {
      case 0x263A: *outp++ = 0x01; break;
      case 0x263B: *outp++ = 0x02; break;
      case 0x2665: *outp++ = 0x03; break;
      case 0x2666: *outp++ = 0x04; break;
      case 0x2663: *outp++ = 0x05; break;
      case 0x2660: *outp++ = 0x06; break;
      case 0x2022: *outp++ = 0x07; break;
      case 0x25D8: *outp++ = 0x08; break;
      case 0x25CB: *outp++ = 0x09; break;
      case 0x25D9: *outp++ = 0x0A; break;
      case 0x2642: *outp++ = 0x0B; break;
      case 0x2640: *outp++ = 0x0C; break;
      case 0x266A: *outp++ = 0x0D; break;
      case 0x266B: *outp++ = 0x0E; break;
      case 0x263C: *outp++ = 0x0F; break;
      case 0x25BA: *outp++ = 0x10; break;
      case 0x25C4: *outp++ = 0x11; break;
      case 0x2195: *outp++ = 0x12; break;
      case 0x203C: *outp++ = 0x13; break;
      case 0x00B6: *outp++ = 0x14; break;
      case 0x00A7: *outp++ = 0x15; break;
      case 0x25AC: *outp++ = 0x16; break;
      case 0x21A8: *outp++ = 0x17; break;
      case 0x2191: *outp++ = 0x18; break;
      case 0x2193: *outp++ = 0x19; break;
      case 0x2192: *outp++ = 0x1A; break;
      case 0x2190: *outp++ = 0x1B; break;
      case 0x221F: *outp++ = 0x1C; break;
      case 0x2194: *outp++ = 0x1D; break;
      case 0x25B2: *outp++ = 0x1E; break;
      case 0x25BC: *outp++ = 0x1F; break;
      case 0x2302: *outp++ = 0x7F; break;
      case 0x00C7: *outp++ = 0x80; break;
      case 0x00FC: *outp++ = 0x81; break;
      case 0x00E9: *outp++ = 0x82; break;
      case 0x00E2: *outp++ = 0x83; break;
      case 0x00E4: *outp++ = 0x84; break;
      case 0x00E0: *outp++ = 0x85; break;
      case 0x00E5: *outp++ = 0x86; break;
      case 0x00E7: *outp++ = 0x87; break;
      case 0x00EA: *outp++ = 0x88; break;
      case 0x00EB: *outp++ = 0x89; break;
      case 0x00E8: *outp++ = 0x8A; break;
      case 0x00EF: *outp++ = 0x8B; break;
      case 0x00EE: *outp++ = 0x8C; break;
      case 0x00EC: *outp++ = 0x8D; break;
      case 0x00C4: *outp++ = 0x8E; break;
      case 0x00C5: *outp++ = 0x8F; break;
      case 0x00C9: *outp++ = 0x90; break;
      case 0x00E6: *outp++ = 0x91; break;
      case 0x00C6: *outp++ = 0x92; break;
      case 0x00F4: *outp++ = 0x93; break;
      case 0x00F6: *outp++ = 0x94; break;
      case 0x00F2: *outp++ = 0x95; break;
      case 0x00FB: *outp++ = 0x96; break;
      case 0x00F9: *outp++ = 0x97; break;
      case 0x00FF: *outp++ = 0x98; break;
      case 0x00D6: *outp++ = 0x99; break;
      case 0x00DC: *outp++ = 0x9A; break;
      case 0x00A2: *outp++ = 0x9B; break;
      case 0x00A3: *outp++ = 0x9C; break;
      case 0x00A5: *outp++ = 0x9D; break;
      case 0x20A7: *outp++ = 0x9E; break;
      case 0x0192: *outp++ = 0x9F; break;
      case 0x00E1: *outp++ = 0xA0; break;
      case 0x00ED: *outp++ = 0xA1; break;
      case 0x00F3: *outp++ = 0xA2; break;
      case 0x00FA: *outp++ = 0xA3; break;
      case 0x00F1: *outp++ = 0xA4; break;
      case 0x00D1: *outp++ = 0xA5; break;
      case 0x00AA: *outp++ = 0xA6; break;
      case 0x00BA: *outp++ = 0xA7; break;
      case 0x00BF: *outp++ = 0xA8; break;
      case 0x2310: *outp++ = 0xA9; break;
      case 0x00AC: *outp++ = 0xAA; break;
      case 0x00BD: *outp++ = 0xAB; break;
      case 0x00BC: *outp++ = 0xAC; break;
      case 0x00A1: *outp++ = 0xAD; break;
      case 0x00AB: *outp++ = 0xAE; break;
      case 0x00BB: *outp++ = 0xAF; break;
      case 0x2591: *outp++ = 0xB0; break;
      case 0x2592: *outp++ = 0xB1; break;
      case 0x2593: *outp++ = 0xB2; break;
      case 0x2502: *outp++ = 0xB3; break;
      case 0x2524: *outp++ = 0xB4; break;
      case 0x2561: *outp++ = 0xB5; break;
      case 0x2562: *outp++ = 0xB6; break;
      case 0x2556: *outp++ = 0xB7; break;
      case 0x2555: *outp++ = 0xB8; break;
      case 0x2563: *outp++ = 0xB9; break;
      case 0x2551: *outp++ = 0xBA; break;
      case 0x2557: *outp++ = 0xBB; break;
      case 0x255D: *outp++ = 0xBC; break;
      case 0x255C: *outp++ = 0xBD; break;
      case 0x255B: *outp++ = 0xBE; break;
      case 0x2510: *outp++ = 0xBF; break;
      case 0x2514: *outp++ = 0xC0; break;
      case 0x2534: *outp++ = 0xC1; break;
      case 0x252C: *outp++ = 0xC2; break;
      case 0x251C: *outp++ = 0xC3; break;
      case 0x2500: *outp++ = 0xC4; break;
      case 0x253C: *outp++ = 0xC5; break;
      case 0x255E: *outp++ = 0xC6; break;
      case 0x255F: *outp++ = 0xC7; break;
      case 0x255A: *outp++ = 0xC8; break;
      case 0x2554: *outp++ = 0xC9; break;
      case 0x2569: *outp++ = 0xCA; break;
      case 0x2566: *outp++ = 0xCB; break;
      case 0x2560: *outp++ = 0xCC; break;
      case 0x2550: *outp++ = 0xCD; break;
      case 0x256C: *outp++ = 0xCE; break;
      case 0x2567: *outp++ = 0xCF; break;
      case 0x2568: *outp++ = 0xD0; break;
      case 0x2564: *outp++ = 0xD1; break;
      case 0x2565: *outp++ = 0xD2; break;
      case 0x2559: *outp++ = 0xD3; break;
      case 0x2558: *outp++ = 0xD4; break;
      case 0x2552: *outp++ = 0xD5; break;
      case 0x2553: *outp++ = 0xD6; break;
      case 0x256B: *outp++ = 0xD7; break;
      case 0x256A: *outp++ = 0xD8; break;
      case 0x2518: *outp++ = 0xD9; break;
      case 0x250C: *outp++ = 0xDA; break;
      case 0x2588: *outp++ = 0xDB; break;
      case 0x2584: *outp++ = 0xDC; break;
      case 0x258C: *outp++ = 0xDD; break;
      case 0x2590: *outp++ = 0xDE; break;
      case 0x2580: *outp++ = 0xDF; break;
      case 0x03B1: *outp++ = 0xE0; break;
      case 0x03B2: case 0x00DF: *outp++ = 0xE1; break;
      case 0x0393: *outp++ = 0xE2; break;
      case 0x03A0: case 0x220F: case 0x03C0: *outp++ = 0xE3; break;
      case 0x2211: case 0x03A3: *outp++ = 0xE4; break;
      case 0x03C3: *outp++ = 0xE5; break;
      case 0x03BC: case 0x00B5: *outp++ = 0xE6; break;
      case 0x03C4: *outp++ = 0xE7; break;
      case 0x03A6: *outp++ = 0xE8; break;
      case 0x0398: *outp++ = 0xE9; break;
      case 0x2126: case 0x03A9: *outp++ = 0xEA; break;
      case 0x2202: case 0x00F0: case 0x03B4: *outp++ = 0xEB; break;
      case 0x221E: *outp++ = 0xEC; break;
      case 0x2205: case 0x03D5: case 0x2300: case 0x00F8: case 0x03C6: *outp++ = 0xED; break;
      case 0x2208: case 0x20AC: case 0x03B5: *outp++ = 0xEE; break;
      case 0x2229: *outp++ = 0xEF; break;
      case 0x2261: *outp++ = 0xF0; break;
      case 0x00B1: *outp++ = 0xF1; break;
      case 0x2265: *outp++ = 0xF2; break;
      case 0x2264: *outp++ = 0xF3; break;
      case 0x2320: *outp++ = 0xF4; break;
      case 0x2321: *outp++ = 0xF5; break;
      case 0x00F7: *outp++ = 0xF6; break;
      case 0x2248: *outp++ = 0xF7; break;
      case 0x00B0: *outp++ = 0xF8; break;
      case 0x2219: *outp++ = 0xF9; break;
      case 0x00B7: *outp++ = 0xFA; break;
      case 0x221A: *outp++ = 0xFB; break;
      case 0x207F: *outp++ = 0xFC; break;
      case 0x00B2: *outp++ = 0xFD; break;
      case 0x25A0: *outp++ = 0xFE; break;
      case 0x00A0: *outp++ = 0xFF; break;
      }
    }
  }
  return outp;
}

uint8_t* convert_cp437_to_utf8(const uint8_t* inp, uint8_t* outp,size_t inlen){
  while(inlen-- > 0) {
    const uint8_t* outchar = char_table + char_map[*inp++];
    while((*outp++ = *outchar++)) {}
  }
  return outp;
}
