#include "screen.h"
#include "util.h"
#include "rapidxml.hpp"

#include <iostream>
using namespace std;

void ScreenConfig::debug_print() const
{
    cout << "Screen " << index << "\n"
         << "  width:   " << width << '\n'
         << "  height:  " << height << '\n'
         << "\n"
         << "  heading: " << heading << '\n'
         << "  pitch:   " << pitch << '\n'
         << "  roll:    " << roll << '\n'
         << "\n"
         << "  originX: " << originX << '\n'
         << "  originY: " << originY << '\n'
         << "  originZ: " << originZ << '\n';
}

void parse_float_attr(rapidxml::xml_node<char>* screen_node, float& into, string attr_name)
{
    rapidxml::xml_attribute<char>* attr;
    
    attr = screen_node->first_attribute(attr_name.c_str());
    if(!attr)
        fatal("Missing '" + attr_name +"' in screen config");
    
    if(!parse_float(into, attr->value()))
        fatal("Failed to parse '" + attr_name + "' in screen config");
}

void parse_int_attr(rapidxml::xml_node<>* screen_node, int& into, string attr_name)
{
    rapidxml::xml_attribute<char>* attr;
    
    attr = screen_node->first_attribute(attr_name.c_str());
    if(!attr)
        fatal("Missing '" + attr_name +"' in screen config");
    
    if(!parse_int(into, attr->value()))
        fatal("Failed to parse '" + attr_name + "' in screen config");
}

void parse_calvr_screen_config(vector<ScreenConfig>& screens_out, 
    string filename, string host)
{
    using namespace rapidxml;
    
    string xml_source = slurp(filename);
    
    xml_document<> doc;
    doc.parse<0>(&xml_source[0]);
    
    xml_node<>* node;
    
    for(node = doc.first_node("LOCAL"); node; node = node->next_sibling("LOCAL"))
    {
        xml_attribute<char>* attr = node->first_attribute("host");
        if(!attr)
            continue;
        
        if(host == attr->value())
        {
            break; // found node for this screen
        }
    }
    
    if(!node)
    {
        cerr << "Failed to find screen configuration for host: " << host << '\n';
        exit(EXIT_FAILURE);
    }
    
    xml_node<>* sc = node->first_node("ScreenConfig");
    
    if(!sc)
        fatal("Failed to find ScreenConfig");
    
    xml_node<>* screen_node = sc->first_node("Screen");
    
    if(!screen_node)
        fatal("Failed to find any Screen entries");
    
    while(screen_node)
    {
        ScreenConfig screen;
        
        parse_int_attr(screen_node, screen.index, "screen");
        
        parse_float_attr(screen_node, screen.width, "width");
        parse_float_attr(screen_node, screen.height, "height");

        parse_float_attr(screen_node, screen.heading, "h");
        parse_float_attr(screen_node, screen.pitch, "p");
        parse_float_attr(screen_node, screen.roll, "r");
        
        parse_float_attr(screen_node, screen.originX, "originX");
        parse_float_attr(screen_node, screen.originY, "originY");
        parse_float_attr(screen_node, screen.originZ, "originZ");
        
        // FIXME: Load pixel config from WindowConfig section!
        screen.pixel_width = 1920/2;
        screen.pixel_height = 1080/2;
        screen.fullscreen = false; // FIXME: remove this
        
        screens_out.push_back(screen);
        screen_node = screen_node->next_sibling("Screen");
    }
}

