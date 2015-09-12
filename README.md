Reliable file transfer mechanism traditionally involves hand shaking procedures. It inherently runs over Transmission Control Protocol (TCP). TCP has a lot of overhead processing because of the handshaking procedures and congestion control. Congestion control creates a number of issues because it needs to get acknowledgement for every packet sent and it does not increase the window size unless it is sure that all the packets in that window have been acknowledged. In order to achieve faster transmission and reception, we need to remove most of the overhead processing and send packets quickly and efficiently. For doing so, we went ahead with User Datagram Protocol (UDP).
UDP has simple transmission model with minimal overhead processing. It runs over Internet Protocol and hence can be routed. Since it does not have any overhead processing like TCP, it is unreliable and has losses. UDP is a connectionless protocol and packets are in the form of datagrams. Since, it does not involve overhead processing, it is very fast and is used in applications where loss is acceptable but are sensitive to time. To make the protocol reliable and transmit data at a higher rate than what Secure Copy (SCP) normally gives, we have incorporated some features that are explained in the design decisions. 


Execution:
1.	Copy the files on to the required directories.
2.	Compile Server code on one node and client code on another node.
3.	Use the command “gcc <FILENAME> -g -W –o <OBJECT> -lpthread -lrt” to compile and create objects.
4.	Use the command ./<SERVEROBJECT> -i <IPADDRESSOFCLIENT> -p<PORTNUMBER> -r<RECEIVEFILENAME>.
5.	Use the command ./<CLIENTOBJECT> -i <IPADDRESS> -p<PORTNUMBER> -f<FILETOSEND> -r<RECEIVEFILENAME> to run client.

Results and Analysis:
LINKSPEED	 RTT	LOSS	 FILESIZE  TIME	   THROUGHPUT
 100Mbps	 10ms	  1%	  100MB	   29.88s	  55.357Mbps
 100Mbps	 200ms 20%	   1GB	   328.89s	52.176Mbps

