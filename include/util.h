#pragma once

#include <string>
#include <stdint.h>

extern "C" {
    #include <libavformat/avformat.h>
}

std::string slurp(const std::string& filename);
bool endswith(const std::string& data, const std::string& pattern);
bool parse_float(float& into, char* from);
bool parse_int(int& into, char* from);

void fatal(std::string why);

std::string print_timestamp(int64_t time);

// ----------------------------------------------------------------------------
void test_screen_parse();
void monitor_test();

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame);

