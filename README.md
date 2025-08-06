# ScriptPNP

Motion controller software using script to organize workflow. Requires a Raspberry Pi 3/4/5 and a '[weenyPRU](https://github.com/iforce2d/weenyPRU)' realtime unit.

Introduction video: https://youtu.be/k-ZdHFlI904

The system consists of a server (with no user interface) and a client with a graphical user interface. The server runs on a Raspberry Pi and connects via SPI to the weenyPRU realtime unit. The client is a graphical user interface (Linux or MacOS) which connects to the server over network. The client requires a decent OpenGL implementation to be usable, eg. hardware accelerated video. Pi3 and Pi4 are not suitable for the client (too low framerate), but a Pi5 can be used to run both the server and client on a single computer. For the client, only Linux (Fedora40 and Ubuntu24.04) have been well tested.

The main intended use case is for 'pick and place' assembly of surface mount components on a PCB, but there is no specific built-in logic to handle this. As such, your own script (written in AngelScript) will be required to tie various procedures together. For example scripts have access to / control of:

- all machine state (position, mode, motion parameters etc)
- head movement and nozzle rotation (4 stepper axes)
- multiple UVC webcams (camera settings and frame-buffer content)
- SQLite database
- digital input and output pins
- two analog inputs
- one analog (PWM) output
- one vacuum sensor
- one load cell amp
- one encoder (eg. manually operated rotary, not fast for motor feedback)
- messaging over serial connection
- height probing via load cell, vacuum sensor 'sniffle' or switch

Scripts are typically run by clicking a custom button in the user interface. Frame-buffer scripts run after each frame is fetched from the camera.

Motion control features include:
- jerk-limited trajectory generator
- corner blending
- 1kHz position control stream
- triggering of digital outputs during move (time offset)
- triggerring of nozzle rotation during move (time offset)

Camera frame-buffer related features include:

- blur
- thresholding (HSV, RGB)
- blob detection
- QR code detection
- grow/shrink
- minAreaRect from blobs
- drawing (line, circle, rect, text)

Database related features include:

- SQL statement execution (via script)
- table duplication
- automatically generated script class per table


## Real-time Linux on Raspberry Pi

In order to be useful as a motion controller, the Raspberry Pi must be running a real-time version of Linux. The easiest way to get this is via a [LinuxCNC image](https://forum.linuxcnc.org/9-installing-linuxcnc/55192-linuxcnc-the-raspberry-pi-4-5-official-images-only), although this has not been tested. You can also also build your own real-time kernel for the standard RaspiOS as [explained here for Pi5](realtime/rpi5rt.txt). This is not as difficult as it sounds, and is likely to be more reliable.



## Initial setup

See info on building the server and client in the 'pnpServer' and 'pnpClient' folders.

Most of the fundamental machine settings are in the [Server] panel (found under 'Setup' in the main menu). Settings in this panel are stored on the server, so they will be the same regardless of connecting from different clients.

## Cameras

Webcams must be UVC compliant, which is typical of most cameras. To open camera views, first use the [USB cameras] panel (found under 'Setup' in the main menu) to enumerate and list them.

## Script editing

You can open [script editor] views (found under 'Editors' in the main menu) to edit and execute scripts. Opening multiple editor views is possible, with each view showing the same content - this may be useful for looking at two files side by side.

## Database tables

To view database tables, first open the [DB tables] panel (found under 'DB' in the main menu). This panel shows a list of tables which can be shown by clicking their checkbox. Options to modify the list, and to duplicate or delete tables can also be found under 'DB' in the main menu.


<br/><br/><br/><br/><br/><br/><br/>This page is a work in progress, stuff below is my notes, plz ignore<br/><br/><br/>
Architecture
    server Raspberry Pi 3/4 (Pi5 not yet)
        RT OS
        weenyPRU
    client Fedora, Ubuntu, MacOS
    message/communication loops
        1000Hz SPI
        200Hz publish
        60Hz GUI update
    server tasks
    client tasks
        scripting
        camera

Hardware
    4 steppers
    Digital in/out
    TMC2209 or external stepper driver
    XGZP vacuum
    HX711 / HX717 load cell ADC
    libuvc camera
    serial port
    WS2812 (if no load cell)
    PWM out

Jerk limited
    3 axis + 1
    blended corners


Panels

Server setup
    tabs

Jogging
    jog speed

Status feedback
    Plots
    Log

Homing

Command lists
    m, w
    Preview
    runtime calculation
    speed scale (preview, run)
    motion limits
    blending limits
    r
    sync
    d 1

Scripting
    Angelscript
    entry function
    run text commands
    runCommandList
    Preview
    substitutions

Tweaks
    access from script

hooks
    Function key
    Button

Workspace layout
    load via script

setDigitalOut, setPWMOut, getVacuum, getADC, getDigitalIn/Out, getLoadCell, getWeight

setDBNumber, setMemoryNumber

Database
    Importing via script
    Edit tables
    import/export editor files

Overrides

Serial port

Probing
    Digital input
    Vacuum sensor
    Load cell

Vision/camera scripting
    set camera params
    save/load image
    HSV threshold
    blur
    find blobs
    minAreaRect
    draw rect/line/cross/circle

QR code

Find blob with machine

Affine transform for board fiducials

Angelscript overview

domain name

Libs
    imgui
    implot
    sqlite
    libuvc
    libAssimp
    ZeroMQ
    Angelscript
    ZXing
    native file dialog
    libserialport




