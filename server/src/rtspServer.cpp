//============================================================================
// Name        : netServer.cpp
// Author      : Obada Sawalha
// Version     : 0.01
// Copyright   :
// Description :
// Sources 	   : TCP Libraries: Rob Tougher (http://tldp.org/LDP/LG/issue74/tougher.html#4)
//			   : UDP Functions based loosely on tutorial: (http://www.ibm.com/developerworks/linux/tutorials/l-sock2/section4.html)
//			   : Need to include the Boost Library for multi-threading
//			   : Using Concurrent Queue "queue" class by Anthony Williams (http://www.justsoftwaresolutions.co.uk/threading/implementing-a-thread-safe-queue-using-condition-variables.html)
//============================================================================

//Include required libraries
#include <boost/thread.hpp>
#include "TCPLib/ServerSocket.h"
#include "TCPLib/SocketException.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>


using namespace std;

#include <iostream>

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <csignal>      // for SIGQUIT, etc. (CTRL C)

// Need to investigate how the new form of libraries are

#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include "RTPpacket.h"

// For timer

#include <sys/time.h>

// Call Concurrent Queue Class and initialise a variable
#include "queue.cpp"
concurrent_queue<const char*> qBuffQueue;
concurrent_queue<int> qBuffQueueSize;
char * pVidHeader;
int iVidHeaderSize;

//RTSP variables
//----------------
//rtsp states
int INIT = 0;
int READY = 1;
int PLAYING = 2;
//rtsp message types
int SETUP = 3;
int PLAY = 4;
int PAUSE = 5;
int TEARDOWN = 6;

//Open output file stream for write
//ofstream fFile ("file.mkv", ios::out | ios::binary);

//Declare integer to hold RTP payload size
int iPacketNo = 0;
//Declare integer to track the time in ms
int iTime = 0;
int readSoFar = 0;
//Declare byte array (pointer) to handle RTP packet and its size
char* pRTP_Packet;
int iRTP_PacketSize;

//Other RTSP Variables
int iRTSP_State; //RTSP Server state == INIT or READY or PLAY
int RTSP_ID = 123456; //ID of the RTSP session
int RTSPSeqNb = 0; //Sequence number of RTSP messages within the session

//Define UDP Socket Global Variables
int iUDP_Sock;
struct sockaddr_in structUDP_Client;

//Function to display error on screen and exit
void Die(string errMsg) {
	perror(errMsg.c_str());
	exit(1);
}// END Die function

/*
//Function to write frames in queue to file
void writeToFile(const char * pWriteFrame, int pWriteFrameSize){

 	//Write frame to file and flush to insure it is written directly to file not memory
	fFile.write(pWriteFrame, pWriteFrameSize);
	fFile.flush();

}//END writeToFile function
*/

//Funstion to reset (queue) to its minimum size
void resetBuffer(int iQueueMinSize)
{
    if(1==0)
    {
	    //Declare and initialise counter to current size of queue
	    int iSize;
	    iSize = qBuffQueue.size();
	
	    if(iRTSP_State != PLAYING && iSize>iQueueMinSize)
        {
		    //Declare pointer to a disposable array and a corresponding integer for its size
		    const char* pBinArray;
		    int iBinArraySize;
		    //While Queue is bigger than the minimum - reset to minimum
		    while(iSize>iQueueMinSize)
            {
			    //pop the queue and assign the array to the disposable pointer
			    qBuffQueue.try_pop(pBinArray);
			    qBuffQueueSize.try_pop(iBinArraySize);
			    //decrement counter
			    iSize--;
		    }
		    //Delete disposable pointer
		    delete pBinArray;
	    }
    }
}//END resetBuffer function

void sendHeader()
{
	//Integer to hold size of sent packet
    int iSent = 0;
    
    //Declare byte array (pointer) to handle RTP packet and its size
    char* pRTP_Packet;
    int iRTP_PacketSize;

	//Build an RTPpacket object containing the video frame
	RTPpacket objRTP_Packet(105, 0, 0, pVidHeader, iVidHeaderSize);

	//get the total length of the rtp packet to send
	iRTP_PacketSize = objRTP_Packet.getlength();

	//retrieve the packet bitstream and store it in an array (pointer)
	pRTP_Packet = new char[iRTP_PacketSize];
	objRTP_Packet.getpacket(pRTP_Packet);

	// Send UDP datagram containing RTP wrapped frame
	iSent = sendto(iUDP_Sock, pRTP_Packet, iRTP_PacketSize, 0, (struct sockaddr *) &structUDP_Client, sizeof(structUDP_Client));

	//Give the screen an update
	cout << "Sending header of size " << iSent << " bytes" << endl;

}//END sendHeader function

void sendRTP_Packet(const char * pVideoFrame, int iVideoFrameSize)
{
	//Integer to hold size of sent packet
	int iSent = 0;

	//iterate packet number counter
	iPacketNo++;

	//write array to file
	//writeToFile(pVideoFrame,iVideoFrameSize);

	//Build an RTPpacket object containing the video frame
	RTPpacket objRTP_Packet(105, iPacketNo, iTime, pVideoFrame, iVideoFrameSize);

	//reset video frame size integer to zero
	iVideoFrameSize = 0;

	//get the total length of the rtp packet to send
	iRTP_PacketSize = objRTP_Packet.getlength();

	//retrieve the packet byte data into byte array (pointer)
	pRTP_Packet = new char[iRTP_PacketSize];
	objRTP_Packet.getpacket(pRTP_Packet);

	// Send UDP datagram containing RTP wrapped video frame
	iSent = sendto(iUDP_Sock, pRTP_Packet, iRTP_PacketSize, 0, (struct sockaddr *) &structUDP_Client, sizeof(structUDP_Client));
	
	//Give the screen an update
	cout << "Sending packet " << iPacketNo << " of size " << iSent << " bytes. Queue size: " << qBuffQueue.size() << endl;
}

void startUDP()
{
	//Declare byte array (pointer) to handle incoming video frame and its size
    const char* pVideoFrame;
    int iVideoFrameSize = 0;

    struct timeval start, end;
    long seconds = 0, useconds = 0;

    gettimeofday(&start, NULL);
	
	//loop to pop queue and send byte arrays over UDP after wrapping in RTP
	while (1) 
    {
		//sleep for 1ms - (prevents from using 100% cpu and allows us to timekeep
		usleep(1000);
		//iterate time counter
		//iTime++;
    
        //////////////////////////////////////////////// HANS AND STELIOS' ADDITION ////////////////////////////////////////////////////

        gettimeofday(&end, NULL);

        seconds  = end.tv_sec  - start.tv_sec;
        useconds = end.tv_usec - start.tv_usec;

        iTime = (int) (((seconds) * 1000 + useconds/1000.0) + 0.5);

        /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		
		//if the queue is not empty
		if(!qBuffQueue.empty())
        {	
			//Retrieve a video frame and its respective size from the queues
			qBuffQueue.try_pop(pVideoFrame);
			qBuffQueueSize.try_pop(iVideoFrameSize);

			//send RTP Packet
			//sendRTP_Packet(pVideoFrame,iVideoFrameSize);
		}
	}

}//END startUDP function

void setupUDP(int iUDP_Port)
{	
	//Declare UDP socket address
    struct sockaddr_in sockUDP_Server;	

	//Declare integer to hold UDP client socket address size
    unsigned int iClientSize = sizeof(structUDP_Client);
    
    //Set the byte array to store the client welcome message
    char cUDP_RcvWelcomeBuffer[150];

	//Integer to hold size of recieved UDP packet
    int iReceived = 0;

	//Tell screen UDP session starting
    cout << "Starting UDP" << endl;

    // Create the UDP socket
	if ((iUDP_Sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		Die("Failed to create socket");
	}

	// Construct the server sockaddr_in structure
	memset(&sockUDP_Server, 0, sizeof(sockUDP_Server));       	// Clear struct
	sockUDP_Server.sin_family = AF_INET;                  		// Set Internet/IP
	sockUDP_Server.sin_addr.s_addr = htonl(INADDR_ANY);   		// Set Any IP address
	sockUDP_Server.sin_port = htons(iUDP_Port);       			// Set server port

	// Bind the socket
	if (bind(iUDP_Sock, (struct sockaddr *) &sockUDP_Server, sizeof(sockUDP_Server)) < 0) {
		Die("Failed to bind server socket");
	}

	// Receive welcome message from the client     
	iReceived = recvfrom(iUDP_Sock, cUDP_RcvWelcomeBuffer, sizeof(cUDP_RcvWelcomeBuffer), 0, (struct sockaddr *) &structUDP_Client, &iClientSize);
	//If failed to recieve welcome message display error and die
	if (iReceived < 0){
		Die("Failed to receive message");
	}

	//Send client IP address and message to screen
	cout << "Client connected: " << inet_ntoa(structUDP_Client.sin_addr) << endl;
	cout << "Client: " << cUDP_RcvWelcomeBuffer << endl;

	//Send video header
	sendHeader();
}//END setupUDP function

//Function to prevent a thread dying
void dontDie()
{
	//sleep for 10 seconds - and loop
	while(1)
		usleep(10*1000*1000);
}//END dintDie function

void threadStartServerRTSP(int iQueueMinSize, int iTCP_Port,int iSetUDP)
{	
	cout << "RTSP Server running on port... \n";

	//Try to run code else catch exception
	try 
    {
		// Create the initial socket with the TCP port number
		ServerSocket objInitTCP_Socket ( iTCP_Port );

		//loop to wait for socket accept
		while(true) 
        {
			// Create connection socket on accept and set RTSP state to INIT
			ServerSocket objTCP_Socket;
			objInitTCP_Socket.accept ( objTCP_Socket );
			iRTSP_State = INIT;

			//Try to run code else catch exception
			try 
            {
				//Declare string to store incoming data
				string sIncomingData;
				
				//loop to recieve data from TCP port and act on it
				while(true) 
                {	
					//recieve data
					objTCP_Socket >> sIncomingData;
					
					//If command was "SETUP"
					if (sIncomingData.compare("SETUP") == 0){
						//Update client
						objTCP_Socket << "Setting up";
						//Run setupUDP function in new thread and wait for it to join
						boost::thread workerThread(setupUDP, iSetUDP);
						workerThread.join();
						//Set RTSP state to READY
						iRTSP_State = READY;
						//run resetBuffer function
						resetBuffer(iQueueMinSize);
						
					//If command was "PLAY"
					}else if(sIncomingData.compare("PLAY") == 0){
						//Update client
						objTCP_Socket << "Playing";
						//run resetBuffer function
						resetBuffer(iQueueMinSize);
						//Set RTSP state to PLAYING
						iRTSP_State = PLAYING;
						//Run startUDP function in new thread
						boost::thread workerThread(startUDP);

					//If command was "TERMINATE"
					}else if(sIncomingData.compare("TERMINATE") == 0){
						//Update client
						objTCP_Socket << "Terminating";

					//If command was not recognised it is a signal
					}else{
						cout << "\nClient signal: " << sIncomingData << endl;
						//Tell client
						objTCP_Socket << "Signal Recieved";
					}//END if/ifelse statement
				}//END while loop 2 - (TCP port recieve data)
			}
			//catch exception
			catch ( SocketException& ) {}

		}//END while loop 1 - (socket accept loop)
	}
	//catch exception
	catch ( SocketException& e ) { cout << "Exception was caught:" << e.description() << "\nExiting.\n"; }
}//END threadStartServerRTSP function

//Role of this function is to simply start function threadStartServerRTSP in a new thread
extern "C" void startServerRTSP(int iSetQueueSize, int iSetTCP, int iSetUDP)
{
	boost::thread serverRTSP(&threadStartServerRTSP, iSetQueueSize, iSetTCP, iSetUDP);
	//serverRTSP.join();
	//dontDie();
}//END startServerRTSP function

//Function to add video frame to the queue
extern "C" void addFrame(char* pFrameIn, int iFrameSize)
{
	//Declare new byte array (pointer) and initialise it with required size
	char * pFrameQ;
	pFrameQ = new char [iFrameSize];

	//Copy the incoming byte array and set to the new pointer
	memcpy(pFrameQ, pFrameIn, iFrameSize);

	//send RTP Packet
	if(iRTSP_State == PLAYING)
		sendRTP_Packet(pFrameQ,iFrameSize);

	//place video frame and its size in their respective arrays
	qBuffQueue.push(pFrameQ);
	qBuffQueueSize.push(iFrameSize);

}//END addFrame function

//Function to store video header
void addHeader(char* pFrameIn, int iFrameSize)
{
	 //Set pointer to new byte array of requires size
	 pVidHeader = new char [iFrameSize];
	 
	 //Copy the incoming byte array and set to the new pointer
	 memcpy(pVidHeader, pFrameIn, iFrameSize);
	 
	 //Save the frame size to our global variable
	 iVidHeaderSize = iFrameSize;
}//END addHeader function

//void addFrameByFile(const char *filename, const char *type)
//{
    	    //int BUFFSIZE = 10000;
	    ////open file for read in binary mode
	 ////   ifstream file1(filename, ios::in | ios::binary);
	    ////byte array to store read data
	    //char readBuff [BUFFSIZE];
	    //int readX = BUFFSIZE;

	    //// Seek to end of file, find size, then seek back to start
	    //file1.seekg(0,ios::end);
	    //int fileSize = file1.tellg();
	    //file1.seekg(0,ios::beg);

	    //cout << "Filesize : " << fileSize << endl;

	    //// read file in chunks of BUFFSIZE
	    //if (file1.is_open())
	      //{
	       ////declare iterator
	 	   //int packetNo = 0;

	 	   ////while we havent reached End Of File (EOF)
	       //while (!file1.eof() && fileSize != 0 ){

	    	 //if(fileSize >= BUFFSIZE){
				 //readX = BUFFSIZE;
		     	 //fileSize -= BUFFSIZE;
	    	 //}else{
	    		 //readX = fileSize;
		     	 //fileSize = 0;
	    	 //}

	    	 ////itterate and read bytes into byte array
	     	 //packetNo++;
	     	 //file1.read(readBuff,readX);

////IMPORTANT FUNCTION //
		//if(strcmp(type, "header")==0){
	     	 ////This function adds the frame to the Network Queue
	     	 //addHeader(readBuff, readX);
		//} else if(strcmp(type, "frame")==0){
	     	 ////This function adds the frame to the Network Queue
	     	 //addFrame(readBuff, readX);
		//} else {
		 //cout << "haha didnt do jack" << endl;
		//}

	     	 ////clear the byte array
		//memset (readBuff,'\0',sizeof(readBuff));

	        //}
	        ////display update
	        //cout << "Read file. Added " << packetNo << " frames to buffer" << endl;

	      //}
	    //else cout << "Unable to open file";
	  ////Close file
	  //file1.close();
	  ////ofstream file0(filename, ios::out);
          ////file0.close();
//}
