
#include <map>
#include "serialPortInfo.h"
#include "log.h"

bool autoRefreshPorts = true;

vector<serialPortInfo*> serialPortInfos; // list of currently present devices
map<string, sp_port*> m_connectedPorts; // to speed up, instead of opening every time

string availablePortListHashStr; // used to detect when updateSerialPortList actually finds something changed

void clearSerialPortList() {
    for (vector<serialPortInfo*>::iterator it = serialPortInfos.begin(); it != serialPortInfos.end(); it++)
        delete *it;
    serialPortInfos.clear();
}


// returns empty string if not a recognized USB serial device
string isUsableUSBDevice(int usb_vid, int usb_pid) {
    if ( usb_vid == 0x0483 && usb_pid == 0x5740 )
        return "STM32 VCP";
    if ( usb_vid == 0x10c4 && usb_pid == 0xea60 )
        return "CP210x";
    if ( usb_vid == 0x0403 && usb_pid == 0x6001 )
        return "FT232R";
    if ( usb_vid == 0x0403 && usb_pid == 0x6015 )
        return "FT231X";
    if ( usb_vid == 0x1a86 && usb_pid == 0x7523 )
        return "CH340";
    return "";
}

void fillCurrentPortList(vector<serialPortInfo*> &infos) {

    sp_port** ports;

    if ( SP_OK != sp_list_ports(&ports) ) {
        g_log.log(LL_ERROR, "sp_list_ports failed: %s", sp_last_error_message());
        return;
    }

    int usb_vid = 0;
    int usb_pid = 0;

    for (int i = 0; ports[i]; i++ ) {

        sp_port* port = ports[i];

        sp_get_port_usb_vid_pid (port, &usb_vid, &usb_pid);

        string deviceType = isUsableUSBDevice(usb_vid, usb_pid);
        if ( deviceType == "" )
            continue;

        char* prodStr = sp_get_port_usb_product(port);
        string usbProduct = prodStr ? prodStr : "";

        serialPortInfo* pi = new serialPortInfo;
        pi->vid = usb_vid;
        pi->pid = usb_pid;
        pi->type = deviceType;
        pi->name = sp_get_port_name(port);
        if (sp_get_port_usb_manufacturer(port))
            pi->manufacturer = sp_get_port_usb_manufacturer(port);
        if (sp_get_port_usb_product(port))
            pi->product = sp_get_port_usb_product(port);
        if (sp_get_port_description(port))
            pi->description = sp_get_port_description(port);

        int bus;
        int address;
        sp_get_port_usb_bus_address(port, &bus, &address);

        //char* serial = sp_get_port_usb_serial(port);

        infos.push_back( pi );

        g_log.log(LL_DEBUG, "Found serial port: %s", pi->name.c_str());
        g_log.log(LL_DEBUG, "  VID PID: 0x%04X 0x%04X (%s)", pi->vid, pi->pid, pi->type.c_str());
        g_log.log(LL_DEBUG, "  Type: %s", pi->type.c_str());
        g_log.log(LL_DEBUG, "  Manufacturer: %s", pi->manufacturer.c_str());
        g_log.log(LL_DEBUG, "  Product: %s", pi->product.c_str());
        g_log.log(LL_DEBUG, "  Description: %s", pi->description.c_str());
    }

    sp_free_port_list(ports);
}

serialPortInfo* getPortInfoByName( vector<serialPortInfo*> &portInfos, string name ) {
    for (serialPortInfo* pi : portInfos) {
        if ( name == pi->name ) {
            return pi;
        }
    }
    return NULL;
}

// returns true if the list actually changed
bool updateSerialPortList()
{
    string availableBefore = availablePortListHashStr;

    vector<serialPortInfo*> currentPortInfos;
    fillCurrentPortList( currentPortInfos );

    // find ports that disappeared and close them properly
    for (std::vector<serialPortInfo*>::iterator it = serialPortInfos.begin(); it != serialPortInfos.end(); it++) {
        if ( ! getPortInfoByName( currentPortInfos, (*it)->name ) ) {
            g_log.log(LL_DEBUG, "Port has disappeared: %s", (*it)->name.c_str() );
            closePort( (*it)->name );
        }
    }

    clearSerialPortList();

    serialPortInfos = currentPortInfos;

    availablePortListHashStr = "";

    g_log.log(LL_INFO, "Current serial ports:");

    //for (std::vector<serialPortInfo*>::iterator it = serialPortInfos.begin(); it != serialPortInfos.end(); it++) {
    for ( serialPortInfo* p : serialPortInfos ) {
        string display = p->description != "" ? p->description : p->product;
        g_log.log(LL_INFO, "  %s (%s)", p->name.c_str(), display.c_str());
        availablePortListHashStr += p->name + "_" + p->product + "_" + p->description + "_";
    }

    return availableBefore != availablePortListHashStr;
}

vector<serialPortInfo *> &getSerialPortInfos()
{
    return serialPortInfos;
}

sp_port* findConnectedPort(string portName) {
    map<string, sp_port*>::iterator it = m_connectedPorts.find(portName);
    if ( it != m_connectedPorts.end() ) {
        return it->second;
    }
    return NULL;
}

sp_port* openPort(string portName, int baud)
{
    sp_port* alreadyOpen = findConnectedPort(portName);
    if ( alreadyOpen ) {
        //g_log.log(LL_DEBUG, "port is already open: %s", portName.c_str());
        return alreadyOpen;
    }

    //lwsl_user("Attempting connect to %s\n", portName.c_str());

    sp_port* port;
    if ( SP_OK != sp_get_port_by_name(portName.c_str(), &port) ) {
        g_log.log(LL_ERROR, "Could not find port %s", portName.c_str() );
        return NULL;
    }

    if ( SP_OK != sp_open(port,SP_MODE_READ_WRITE) ) {
        g_log.log(LL_ERROR, "Could not open port %s : %s", portName.c_str(), sp_last_error_message() );
        return NULL;
    }

    g_log.log(LL_INFO, "Opened port %s", portName.c_str() );

    if ( SP_OK != sp_set_baudrate(port, baud) ) {
        g_log.log(LL_ERROR, "Could not set baud rate for port %s : %s", portName.c_str(), sp_last_error_message() );
    }

    sp_port_config* config;
    sp_new_config(&config);
    sp_get_config( port, config );

    sp_set_config_xon_xoff( config, SP_XONXOFF_DISABLED );
    sp_set_config( port, config );

    sp_get_config( port, config );

    int bits = 0;
    int stopbits = 0;
    sp_parity parity;
    int baudrate = 0;
    sp_cts cts;
    sp_dsr dsr;
    sp_dtr dtr;
    sp_rts rts;
    sp_xonxoff xonxoff;
    sp_get_config_baudrate(config, &baudrate);
    sp_get_config_bits(config, &bits);
    sp_get_config_stopbits(config, &stopbits);
    sp_get_config_parity(config, &parity);
    sp_get_config_cts(config, &cts);
    sp_get_config_dsr(config, &dsr);
    sp_get_config_dtr(config, &dtr);
    sp_get_config_rts(config, &rts);
    sp_get_config_xon_xoff(config, &xonxoff);
/*
    printf("Proceeding with port settings:\n");
    printf("    Baudrate: %d\n", baudrate);
    printf("    Bits: %d\n", bits);
    printf("    Stop bits: %d\n", stopbits);
    printf("    Parity: %d\n", parity);

    printf("    cts: %d\n", cts);
    printf("    dsr: %d\n", dsr);
    printf("    dtr: %d\n", dtr);
    printf("    rts: %d\n", rts);
    printf("    XonXoff: %d\n", xonxoff);

    fflush(stdout);
*/
    //m_connectedPort = port;
    //m_connectedPortName = portName;

    sp_free_config(config);

    m_connectedPorts[portName] = port;

    return port;
}

void closePort(string portName)
{
    closePort( findConnectedPort(portName) );
}

void closePort(sp_port* port) {
    if ( ! port )
        return;

    string portName = sp_get_port_name(port);

    if ( SP_OK != sp_close(port) ) {
        g_log.log(LL_ERROR, "Error when closing port %s : %s", portName.c_str(), sp_last_error_message());
    }

    //clearPortToPSSMapping(port);
    //clearRelaysBySerialPort( port );

    sp_free_port(port);

    g_log.log(LL_INFO, "Closed port %s", portName.c_str());

    m_connectedPorts.erase(portName);

}

void closeAllPorts() {
    vector<sp_port*> toClose;
    //ITERATE_MAP(string, sp_port*, m_connectedPorts) {
    for ( pair<string,sp_port*> p : m_connectedPorts ) {
        toClose.push_back(p.second);
    }
    for (sp_port* p : toClose) {
        closePort( p );
    }
}

