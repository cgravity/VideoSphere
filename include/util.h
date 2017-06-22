#pragma once

#include <string>

std::string slurp(const std::string& filename);
bool endswith(const std::string& data, const std::string& pattern);
bool parse_float(float& into, char* from);
bool parse_int(int& into, char* from);

void fatal(std::string why);


// ----------------------------------------------------------------------------
void test_screen_parse();
void monitor_test();

