# Building the server

First, install the ZeroMQ dev libraries. If you already have them (eg. by building from source as shown in the client build steps) this step will not be necessary.

```
sudo apt install libczmq-dev
```

After this, the server should build by just running `make` in this folder.

To access the SPI hardware, you'll need to run the server as root, ie.:

```
sudo ./pnpServer
```
