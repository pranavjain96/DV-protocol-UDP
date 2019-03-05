#include "helper.h"

#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>	// for fprintf
#include <string.h>	// for memcpy
#include <string>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctime>
#include <time.h>

#define buf 2048

using namespace std;
enum type
{
	TYPE_DATA, TYPE_ADVERTISEMENT, TYPE_WAKEUP, TYPE_RESET
};
struct header
{

	int type,len;
	char src,dest;
};

void print_fn(router R[], char name, string msg, bool curr_time);

void *create_packet(int type, char src, char dest, int payload_length, void *payload)
{
	int allocated_payload_length = payload_length;
	if((type != TYPE_DATA) && (type != TYPE_ADVERTISEMENT)) 	//Either the router was idle or
		allocated_payload_length = 0;							//getting valid from invalid or vise-versa
	//Create packet
	void *packet = operator new (sizeof(header)+ allocated_payload_length); //create empty packet
	header h;
		h.type = type;
		h.src = src;
		h.dest = dest;
		h.len = payload_length;
	memcpy(packet, (void*)&h, sizeof(header));
	//Append packet with payload
	memcpy((void*)((char*)packet+sizeof(header)), payload, allocated_payload_length);
	return packet;
}
header get_header(void *packet)
{
	header h;
	memcpy((void*)&h, packet, sizeof(header));
	return h;
}
void *get_payload(void *packet, int len)
{
	void *payload = operator new (len);
					//Shift pointer to the beginning of payload
	memcpy(payload, (void*)((char*)packet+sizeof(header)),len);
	return payload;
}
void advertise_cast(DistVector &dv, int socket)
{
	vector<r_node> neighbour = dv.neighbours(); //neighbouring routers' data
	for( int i=0; i<neighbour.size();i++)
	{
		void *send_packet = create_packet(TYPE_ADVERTISEMENT, dv.getName(), neighbour[i].name, dv.getSize(), (void*)dv.getRouter());
		sendto(socket, send_packet, sizeof(header)+dv.getSize(),0,(struct sockaddr *)&neighbour[i].addr, sizeof(sockaddr_in));
		free(send_packet);
	}
}
void self_cast(DistVector &dv, int socketfd,int type, char src=0, char dest=0, int payload_length=0, void *payload=0)
{
	void *send_packet = create_packet(type,src,dest,payload_length,payload);
	sockaddr_in dest_addr = dv.myaddr();
	sendto(socketfd, send_packet,sizeof(header),0,(struct sockaddr *)&dest_addr,sizeof(sockaddr_in));
	free(send_packet);
}
int main(int argc, char **argv)
{
	if(argc < 3){
		cerr<<"Not enough arguments.\nUsage: ./my-router <initialization file> <router name>\n";
		return 0;
	}
	DistVector dv(argv[1], argv[2]); 
	vector<r_node> neighbours = dv.neighbours();
	int my_port = dv.portNoOf(argv[2][0]);
	dv.init_addr(my_port);
	sockaddr_in myaddr = dv.myaddr();

	socklen_t addr_len = sizeof(sockaddr_in); //length of addresses
	int socketfd = socket(AF_INET, SOCK_DGRAM,0);// create the MAIN UDP socket
	if (socketfd < 0){
		cerr<<"cannot create socket\n";
		return 0;
	}
	if (bind(socketfd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0){ // bind the MAINsocket to localhost and myPort
		cerr<<"Bind failed";
		return 0;
	}
	if (dv.getName() == 'H'){ 
		char data[100];
		memset(data,0,100);
		cin.getline(data, 100);
		for(int i=0; i<neighbours.size(); i++)
			if(neighbours[i].name=='A') //DATA SENDING packet to router A
			{
				void *data_packet = create_packet(TYPE_DATA, dv.getName(), 'D', strlen(data), (void*)data);
				sendto( socketfd, data_packet, sizeof(header)+dv.getSize(), 0, (struct sockaddr *)&neighbours[i].addr, sizeof(sockaddr_in));
				
				header h = get_header(data_packet);
				cout<<"\n DATA packet sent.\n Source: "<<h.src<<"\n Destination: "<<h.dest
					<<"\n Length of Packet: "<<sizeof(header)+h.len<<"\n Length of Payload: "<<h.len<<"\n Payload: "<<data<<endl;
				free(data_packet);
			}
		exit(0);
	}
	int counter = 0;
	int process = fork(); 
	if(process < 0){
		cerr<<"Fork failed";
		return 0;
	}
	else if(process == 0)// SELF WAKE
		while(1){
			self_cast(dv, socketfd, TYPE_WAKEUP);
			sleep(5);
		}
	else{ //listen for advertisements
		void *rcvbuf = operator new (buf);
		sockaddr_in rem_addr;
		for(;;){
			memset(rcvbuf, 0, buf);
			int recv_len = recvfrom(socketfd, rcvbuf, buf, 0, (struct sockaddr*)&rem_addr, &addr_len);

			header h = get_header(rcvbuf);
			void *payload = get_payload(rcvbuf, h.len);
			switch(h.type){
				case TYPE_DATA:
					cout<< "Received Data Packet!"<<endl;
					time_t rawtime;
					time(&rawtime);
					cout<<"Current Time: "<< ctime(&rawtime);
					cout<<"Source node ID: "<< h.src << endl;
					cout<<"Destination ID: "<< h.dest << endl;
					cout<<"UDP port in which packet arrived: "<< my_port<<endl;
					if(h.dest != dv.getName()){ //FORWARD ONLY WHEN ROUTER IS NOT THE DESTINATION
						if(dv.routeTo(h.dest).nexthop_port()== inf){
							cerr<<"ERROR: Packet forwarding failed."<<endl;
						}
						else{
							cout<<"Packet forwarded through UDP port: "<< dv.routeTo(h.dest).nexthop_port()<<endl;
							cout<<"Packet forwarded to node ID: "<< dv.routeTo(h.dest).nexthop_name() << endl;
							void *forwardPacket = create_packet(TYPE_DATA, h.src, h.dest, h.len, (void*)payload);
							for (int i = neighbours.size(); i>=0  ; i--)
							{
								if (neighbours[i].name == dv.routeTo(h.dest).nexthop_name())
									sendto(socketfd, forwardPacket, sizeof(header) + dv.getSize(), 0, (struct sockaddr *)&neighbours[i].addr, sizeof(sockaddr_in));
							}
							free(forwardPacket);
						}
						cout<< endl;
					}
					else{ //DESTINATION ROUTER; EXTRACT DATA
						char data[100];
						memset(data, 0, 100);
						memcpy((void*)data, payload, h.len);
						cout << "Data payload: " << data << endl << endl;
					}
					break;
				case TYPE_ADVERTISEMENT: //ADVERTISE ITS' ROUTER
					for(int i = 0; i < neighbours.size(); i++){
						if(neighbours[i].name == h.src)
							dv.startTimer(neighbours[i]);
					}
					if(dv.update(payload, h.src))
						counter = 0;
					break;
				case TYPE_WAKEUP: //UPDATE WAKEUP CALLS 
					for(int i = 0; i< neighbours.size(); i++){
						 r_node curr_neighbour = neighbours[i];
						 if((dv.getRouter()[dv.indexOf(curr_neighbour.name)].cost()!= inf) && dv.timeExpired(neighbours[i])){
						 	self_cast(dv, socketfd, TYPE_RESET, dv.getName(), neighbours[i].name, dv.getSize() / sizeof(router) );
						 }
					}
					counter++;
					advertise_cast(dv, socketfd);
					if(counter ==5){
						print_fn(dv.getRouter(), dv.getName(), "CONVERGENCE OBSERVED", true);
						//counter = 0;
					}
					break;
				case TYPE_RESET: 
					//counter = 0;
					int hopcount = (int)h.len-1;
					dv.reset(h.dest);
					if(hopcount > 0){
						void *fwd_packet = create_packet(TYPE_RESET,  dv.getName(), h.dest, hopcount, 0);
						for( int i = 0; i< neighbours.size(); i++){
							if (neighbours[i].name != h.src)
								sendto(socketfd, fwd_packet, sizeof(header), 0, (struct sockaddr *)&neighbours[i].addr, sizeof(sockaddr_in));
						}
					}
					break;


			}
		}
		free(rcvbuf);
	}
}

void print_fn(router R[], char name, string msg, bool curr_time){
	int count = 0;
	for(int i=0; i< max_routers; i++)
		if(R[i].cost()==inf)
			count++;
	if(count == 1){
		cout <<"\t"<< msg << ": " << name << endl;
		if (curr_time)
		{
			time_t rawtime;			//declare time var
			time(&rawtime);			//store current time
			cout <<"\t"<< ctime(&rawtime);//convert time_t to string and 
									//print in time formmat
		}
		cout<<" | Destination |  A  |  B  |  C  |  D  |  E  |  F  |\n | Cost        |  ";
		for (int dest = 0; dest < max_routers; dest++)
		{
			if(R[dest].cost() == inf)
				cout<<"0  |  ";
			else
				cout<<R[dest].cost()<<"  |  ";
		}
	}
	cout<<endl;
}