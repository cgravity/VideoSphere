VideoSphere
============

VideoSphere is a program for playing back 360 degree video on immersive CAVE VR systems such as UCSD's WAVE and SunCAVE, and EVL's CAVE2. It can also be run on planar tiled display walls.

It uses FFMPEG to decode video, so any format supported by FFMPEG should be playable with VideoSphere as well.

Videos can be interacted with using the keyboard or a PS3 controller (via oscjoystick) to turn the video, seek forwards and backwards in time, and pause. 

## Dependencies

- [FFMPEG](https://www.ffmpeg.org/download.html) 3.3.1
- [GLFW](http://www.glfw.org/download.html) 3.2.1
- [GLEW](http://glew.sourceforge.net/) 2.1.0
- [PortAudio](http://www.portaudio.com/download.html) v19
- oscpack 1.1.0 (included and built automatically)

## Build Instructions

### Local build of dependencies (local.mk)
On most systems that VideoSphere is used on, a local build of the dependencies is required. (If you have compatible versions of the dependencies installed globally on the sytem, you can skip this section.) You can specify a local install location using the prefix setting when running configure or in CMake options, depending on the dependency.

To override any default system installed libraries, you can create a file called **local.mk** in the same folder as the VideoSphere Makefile. Here is a usable example (just change `<path-to-local-prefix>` to the actual local install path):

    PKG_CONFIG_PATH = /<path-to-local-prefix>/lib/pkgconfig

    INCLUDE_FFMPEG := $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) \
        pkg-config --cflags \
        libavformat libavcodec libswscale)

    LINK_FFMPEG := $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) \
        pkg-config --libs \
        libavformat libavcodec libswscale)

    INCLUDE_GLFW3 := $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) \
        pkg-config --cflags \
        glfw3)

    LINK_GLFW3 := $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) \
        pkg-config --libs --static \
        glfw3) -lGL

    INCLUDE_GLEW := $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) \
        pkg-config --cflags \
        glew)

    LINK_GLEW := $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) \
        pkg-config --libs --static \
        glew)

    INCLUDE_PA := $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) \
        pkg-config --cflags portaudio-2.0)

    LINK_PA := $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) \
        pkg-config --libs portaudio-2.0)

You may need to tweak the settings based on your system, and which packages you built from source.

### Compiling VideoSphere

Once local.mk is created (if necessary), you should be able to build VideoSphere just by running 'make'.


## Configuration

**WARNING**: The information in this section is likely to change significantly in the near future as development continues.

### Screen Configuration

In order to determine the view shown on each monitor, a screen configuration file is needed. Currently, the screen configuration file is based on a subsection of CalVR's config file. If you already have a CalVR config file specified for the system, you can copy just the relevant portion into a new file and specify it with the `--calvr-config` command line option to VideoSphere (see below).

A sample configuration for one node with two screens is:

    <LOCAL host="wave-0-0.local" >
        <ScreenConfig>
          <Screen height="681" h="0.0" width="1209" p="-67.5"   originX="-2428"  originY="317"  r="0.0" name="0" originZ="-1592"    screen="0" />
          <Screen height="681" h="0.0" width="1209" p="-67.5"   originX="-1214"  originY="317"  r="0.0" name="1" originZ="-1592"    screen="1" />
        </ScreenConfig>
    </LOCAL>

Each node is indicated by a `LOCAL` tag with `host` attribute indicating the unique hostname of that computer. Under this section, a `ScreenConfig` tag must be present, and under that should be one or more `Screen` tags. The attributes to each screen are:

Attribute | Purpose
----------|--------
width | The width of the screen in millimeters
height | The height of the screen in millimeters
originX | The X coorindate of the center of the screen in millimeters away from the origin of the display system. Positive X goes right.
originY | The Y coorindate of the center of the screen in millimeters away from the origin of the display system. Positive Y goes forwards.
originZ | The Z coorindate of the center of the screen in millimeters away from the origin of the display system. Positive Z goes up.
r | Roll of the screen, in degrees. Rotates about the Y axis.
p | Pitch of the screen, in degrees. Rotates about the X axis.
h | Heading of the screen, in degrees. Rotates about the Z axis.
screen | Index of the screen, starting from "0".

More details about the CalVR config format are available [here](http://ivl.calit2.net/wiki/index.php/CalVR_Config_File#Screens). In particular, the following comment is very important:

> The width and height are the world space size of the screen. For the time being, this is in milimeters. The originX/Y/Z are the world space coordinates of the center of the screen. Note: CalVR uses an x right, y forward, z up coord system. h/p/r are the screen rotations about its center. A screen with no rotation is assumed to be in the x/z plane. Heading is over the z axis, pitch is over the x axis and roll is over the y axis. The order of rotation is roll->pitch->heading.

### Alternative Screen Configuration

An alternative to using GLFW's window handling is to request that windows be created directly by VideoSphere using X11. As this requires information beyond what CalVR's original config format allowed, a new derivative format was created with additional parameters. You can indicate that this format is expected by using the `--config` parameter (instead of `--calvr-config`) when launching VideoSphere. In addition to the above properties, the `Screen` tag has the following additional attributes:

Attribute | Purpose
----------|--------
mode | Must be set to either "glfw" or "x11" to indicate which screen creation mode is desired.
fullscreen | Either "true" or "false" to indicate if the window should be created in fullscreen mode.
override_redirect | Either "true" or "false" to indicate if a window should be created using override redirect. Only relevant to "x11" mode.
display | X11 display string to use -- e.g. ":0.0"
x | x pixel position to create window at when using "x11" mode.
y | y pixel position to create window at when using "x11" mode.
pw | pixel width to use when creating a window in "x11" mode.
ph | pixel height to use when creating a window in "x11" mode.

Manual X11 mode has some quirks and drawbacks currently -- in particular, it does not yet support keyboard input -- but may be helpful in getting VideoSphere to work on certain finicky display systems.

### Cluster configuration

There is a `cluster_video.py` script which attempts to launch client instances of VideoSphere across a display cluster. It expects a config.txt file with a very simple format. The first line should be the IP address that clients use to connect to the headnode with. The second line should be the path to the configuration file, and each additional line should include the IP or hostname of the computer in the cluster which should be SSH'd into, followed by the X11 display string environment variable to set after connecting via SSH.

For example:

    10.0.0.1
    /path/to/config.xml
    cluster-node-0 :0.0
    cluster-node-0 :0.1
    cluster-node-1 :0.0
    cluster-node-1 :0.1
    cluster-node-2 :0.0
    cluster-node-2 :0.1
    cluster-node-3 :0.0
    cluster-node-3 :0.1

This indicates that the headnode should be contacted at 10.0.0.1, the config file should be accessible at /path/to/config.xml on each node, and that eight client instances of VideoSphere should be launched -- two on each of four computers (cluster-node-0 through cluster-node-3) with the `DISPLAY` environment variable set alternatively to :0.0 and :0.1.

## Command-line Arguments

In order to launch VideoSphere, you must specify a number of command-line arguments to tell it how it should behave. Here are the relevant options:

Option | Purpose
-------|--------
`--server` | Indicates that VideoSphere should run as the headnode. A window will be created showing the entire 360x180 degree field of view as an equirectangular projection.
`--client IP` | Indicates that VideoSphere should run as a client and connect to a headnode instance running at the specified IP address.
`--headless` | Indicates that VideoSphere act as a server, but also show a display window based on a configuration file. This is especially useful on tiled display walls, which often do not have a seperate headnode computer.
`--video PATH` | Indicates the path to the video file to play.
`--calvr-config PATH` | Indicates the path to a CalVR-style configuration file.
`--config PATH` | Indicates the path to the alternative format configuration file to use. (Specify only one of `--calvr-config` and `--config`.)
`--host NAME` | Explicitly specify the hostname to load the screen configuration of from the configuration file instead of depending on the setting on the system. Handy for testing.
`--monitor NUMBER` | Indicates the monitor index which should be used (0, 1, ...)
`--stereo` | If passed, the video is assumed to be in top/bottom format, and stereoscopic output will be drawn in top/bottom form.
`--loop` | Plays the video over and over until Escape is pressed (if using a GLFW window) or until the process is killed (e.g. with alt-tab, ctrl-c for X11).
`--audio` | Plays audio output. By default, no audio is played unless requested.
`--mcgroup IP` | Experimental multicast option. Not recommended for regular use. Indicates the IP for the multicast group which should be joined for streaming video.
`--mciface IP` | Experimental multicast option. Not recommended for regular use. Indicates the IP of the interface to use.
`--mcport PORT` | Experimental multicast option. Not recommended for regular use. Indicates what port should be used for multicast communication.
`--mcttl TTL` | Experimental multicast option. Not recommended for regular use. Indicates the multicast time-to-live count which should be set.

An instance running as `--server` must have a `--video` parameter at minimum.

An instance running as `--client` must have the IP of the server, and a `--calvr-config` or `--config` option.

An instance running as `--headless` must have a `--video` paramter, and a `--calvr-config` or `--config` parameter.

To launch VideoSphere clients across the cluster, set up the config.txt file as described above and then run `cluster_video.py x` -- it originally required an additional parameter, but this has been temporarily disabled.

It is a assumed that the video is available at the same path on all nodes. `/data/360_video/` is often used as an shared folder over NFS.

These configuration options will likely evolve significantly during future development, with some options moving into configuration files instead.

