#include "shader.h"
#include "util.h"
#include <iostream>
#include <cstdlib>
using namespace std;

GLuint load_shaders(vector<string> filenames)
{
    GLint program = glCreateProgram();
    
    for(size_t i = 0; i < filenames.size(); i++)
    {
        string src = slurp(filenames[i]);
        const char* src_c = src.c_str();
        
        GLint type = 0;
        
        if(endswith(filenames[i], ".frag"))
            type = GL_FRAGMENT_SHADER;
        else if(endswith(filenames[i], ".vert"))
            type = GL_VERTEX_SHADER;
        else
        {
            cerr << "Unsupported shader type!\n";
            cerr << "File: " << filenames[i] << '\n';
            exit(EXIT_FAILURE);
        }
        
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &src_c, NULL);
        glCompileShader(shader);
        
        GLint shader_success = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &shader_success);
        
        if(shader_success == GL_FALSE)
        {
            cerr << "Failed to compile shader!\n";
            cerr << "File: " << filenames[i] << '\n';
            cerr << "Error:\n";
            
            GLint logSize = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logSize);
            
            char* error_msg = (char*)malloc(logSize);
            
            glGetShaderInfoLog(shader, logSize, NULL, error_msg);
            cerr << error_msg << '\n';
            
            free(error_msg);
            exit(EXIT_FAILURE);
        }
        
        glAttachShader(program, shader);
    }    
    
    glLinkProgram(program);
    
    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if(success == GL_FALSE)
    {
        cerr << "Failed to link shaders!\n";
        
        GLint logSize = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logSize);
        
        char* error_msg = (char*)malloc(logSize);
        
        glGetProgramInfoLog(program, logSize, NULL, error_msg);
        cerr << error_msg << '\n';
        
        free(error_msg);
        exit(EXIT_FAILURE);
    }
    
    return program;
}

