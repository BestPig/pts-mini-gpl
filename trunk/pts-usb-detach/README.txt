Short description (I need to write the whole thing up but I'm not
there yet).

Detach is a program written by Bob Pwwer (http://www.bobsplace.com/ilinks/). 

Its purpose is to detach a driver from a specific USB device. Bob's
original version was written to specificly detach the Insteon USB
PLC. I've (Neil Cherry, http://www.linuxha.com/, ncherry@linuxha.com)
modified it so I can use it on any USB device.

To compile this you will need libusb installed. Then type make. To use
./detach to get the device's Vendor ID and Product ID type:

$ cat /proc/bus/usb/devices

T:  Bus=01 Lev=01 Prnt=01 Port=00 Cnt=01 Dev#=  3 Spd=1.5 MxCh= 0
D:  Ver= 1.10 Cls=00(>ifc ) Sub=00 Prot=00 MxPS= 8 #Cfgs=  1
P:  Vendor=10bf ProdID=0004 Rev= 4.00
S:  Manufacturer=SmartHome
S:  Product=SmartHome PowerLinc USB E
C:* #Ifs= 1 Cfg#= 1 Atr=a0 MxPwr=500mA
I:  If#= 0 Alt= 0 #EPs= 2 Cls=03(HID  ) Sub=01 Prot=00 Driver=iplc
E:  Ad=81(I) Atr=03(Int.) MxPS=   8 Ivl=10ms
E:  Ad=01(O) Atr=03(Int.) MxPS=   8 Ivl=10ms

T:  Bus=01 Lev=01 Prnt=01 Port=01 Cnt=02 Dev#=  4 Spd=1.5 MxCh= 0
D:  Ver= 1.10 Cls=00(>ifc ) Sub=00 Prot=00 MxPS= 8 #Cfgs=  1
P:  Vendor=0bc7 ProdID=0001 Rev= 1.00
S:  Manufacturer=X10 Wireless Technology Inc
S:  Product=USB ActiveHome Interface
C:* #Ifs= 1 Cfg#= 1 Atr=e0 MxPwr=  2mA
I:  If#= 0 Alt= 0 #EPs= 2 Cls=00(>ifc ) Sub=00 Prot=00 Driver=cm15a
E:  Ad=81(I) Atr=03(Int.) MxPS=   8 Ivl=10ms
E:  Ad=02(O) Atr=03(Int.) MxPS=   8 Ivl=10ms

Look for the specific device you're interested in, say the Smart
PowerLinc USB E, and the use it ID's (10bf 0004) as the cli parameters
to pass to detach. Like this:

$ detach 10bf 0004

If it worked you'll get no additional output, errors will return an
error message and an errno. If you have more than one device then
detach will currently only detach the first device matching the cli
parameters. I'm working on a way to pass the correct parameters to
detach the correct device.
