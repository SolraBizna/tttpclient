#ifndef BREAKLINESHH
#define BREAKLINESHH

#include "tttpclient.hh"
#include <string>
#include <vector>

std::vector<std::string> break_lines(const std::string& message,
                                     unsigned int line_width);

#endif
