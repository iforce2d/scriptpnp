#ifndef SERIALPORTINFO_H
#define SERIALPORTINFO_H

#include <string>
#include <vector>
#include <libserialport.h>

using namespace std;

struct serialPortInfo {
    int vid;
    int pid;
    string type;
    string name;
    string manufacturer;
    string product;
    string description;
};

void clearSerialPortList();
bool updateSerialPortList();
vector<serialPortInfo*>& getSerialPortInfos();
sp_port* findConnectedPort(string portName);
sp_port* openPort(string portName, int baud);
void closePort(string portName);
void closePort(sp_port* port);
void closeAllPorts();

#endif // SERIALPORTINFO_H
