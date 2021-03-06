#!/usr/bin/python2

from multiprocessing import Pool
from subprocess import call, Popen, PIPE
import os
import sys

program_path = ""
SERVER_ADDRESS = ""
CONFIG_PATH=""

extra_args = []

def file_exists(path):
    return os.path.isfile(path)

def run_program(arg):
    if len(arg) == 2:
        host, display = arg
        mcgroup = None
    elif len(arg) == 4:
        host, display, mcgroup, mciface = arg
    else:
        print "Bad config:", arg
        return
    
    cmd = [
        "ssh", host, 
        "cd ", os.path.dirname(program_path), " && ",
        "DISPLAY=" + display + " ",
        program_path,
        "--loop", 
        "--client", SERVER_ADDRESS,
        "--config", CONFIG_PATH,
        "--host", host,
        "--monitor",  display[display.rfind(".")+1:],
    ]
    
    if mcgroup:
        cmd += ["--mcgroup", mcgroup, "--mciface", mciface]
    
    if extra_args:
        cmd += extra_args
    
    cmd += [">", "/tmp/video_sphere_log", "2>&1"]
    
    print " ".join(cmd)
    call(cmd)
    
def main():
    global program_path
    global CONFIG_PATH
    global SERVER_ADDRESS
    global extra_args
    
    if "--help" in sys.argv:
        print "USAGE: cluster_video.py [--stereo] [--stereo-interleaved]"
        return
    
    if not file_exists("config.txt"):
        print "Could not find config.json!"
    
    extra_args = sys.argv[1:]
    
    # config file format
    # first line is IP of this computer to use as the server
    # second line is path to config file
    # all lines after are <computer IP/hostname> <display>
    #
    # example:
    #
    #   192.168.0.1
    #   /cluster/shared/config_file.xml
    #   192.168.0.2 :0.0
    #   192.168.0.2 :0.1
    #   192.168.0.3 :0.0
    #   192.168.0.3 :0.1
    #
    #   ^^ this lists the head node as 192.168.0.1 with four monitors
    #   to display on (two on each of 192.168.0.2 and 192.168.0.3)
    
    with open("config.txt") as fp:
        config = fp.readlines()
    
    SERVER_ADDRESS = config[0].strip()
    CONFIG_PATH = config[1].strip()
    
    config = config[2:] # remove first two config settings to get display list
    config = [x.strip().split(" ") for x in config if len(x.strip()) > 0]
    
    # simple sanity checks
    for i, x in enumerate(config):
        if len(x) != 2 and len(x) != 4:
            print "Wrong number of entries on line", i+2
            return
        
        if x[1].rfind(".") == -1:
            print "expected '.' in display format specifier on line", i

    program_path = os.path.join(os.getcwd(), "video_sphere")
    
    if not file_exists(program_path):
        print "ERROR: can't find video_sphere in current directory!"
        return
    
    #p = Popen([program_path, "--server", "--video", sys.argv[1]], 
    #    stdout=PIPE, stderr=PIPE)
    
    INSTANCES = len(config)
    pool = Pool(INSTANCES)
    pool.map(run_program, config)

if __name__ == '__main__':
    main()
    

