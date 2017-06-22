#pragma once

#include <vector>
#include <string>

#include <GL/glew.h>
#include <GLFW/glfw3.h>


// given a list of filenames, loads files, compiles, and links them
// returns the program id if successful, or quits with error if not
GLuint load_shaders(std::vector<std::string> filenames);

