# LBeacon

LBeacon (Location Beacon): BeDIPS uses LBeacons to deliver to users the 3D coordinates and textual descriptions of their locations. Basically, LBeacon is a low-cost, Bluetooth Smart Ready device. At deployment and maintenance times, the 3D coordinates and location description of every LBeacon are retrieved from BeDIS (Building/environment Data and Information System) and stored locally. Once initialized, each LBeacon broadcast its coordinates and location description to Bluetooth enabled mobile devices nearby within its coverage area.

Alpha version of LBeacon was demonstrated during Academia Sinica open house and will be experimented with and assessed by collaborators of the project.

### Installing Raspberry Pi

[Download](https://www.raspberrypi.org/downloads/raspbian/) the Raspberry Pi operating system and follow its installation guide.

On a computer, [enable](https://www.raspberrypi.org/documentation/configuration/wireless/wireless-cli.md) SSH server on a terminal. SSH to the Raspberry Pi and install packages by running the following command:
```sh
$ sudo apt-get install -y git bluez libbluetooth-dev fuse libfuse-dev libexpat1-dev swig python-dev ruby ruby-dev libusb-1.0-0-dev default-jdk xsltproc libxml2-utils cmake doxygen
```
Download open source code for obexftp and openobex libraries:
```sh
$ git clone https://gitlab.com/obexftp/mainline obexftp
$ git clone https://gitlab.com/openobex/mainline openobex
```
To compile and install openftp and openobex libraries:
```sh
$ cd openftp
$ mkdir build
$ cd build
$ cmake ..
$ sudo make
$ sudo make install
$ cd ..
$ cd openobex
$ mkdir build
$ cd build
$ cmake ..
$ sudo make
$ sudo make install
```
Update the links/cache the dynamic loader uses:
```sh
$ sudo ldconfig -v
```

### Compiling and Running LBeacon
```sh
$ gcc LBeacon.c -g -o LBeacon -L/usr/local/lib -lrt -lpthread -lmulticobex -lbfb -lbluetooth -lobexftp -lopenobex
$ sudo ./LBeacon
```