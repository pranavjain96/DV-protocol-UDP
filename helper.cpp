#include "helper.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctime>
#include <time.h>
#include <unistd.h>

using namespace std;

void DistVector::init_addr(int portno)
{
	memset((char *)&dv_myaddr, 0, sizeof(dv_myaddr)); 	//set 0 to myaddr
	dv_myaddr.sin_family = AF_INET;						//Internet family
	dv_myaddr.sin_addr.s_addr = inet_addr("127.0.0.1");	//fixed IP
	dv_myaddr.sin_port = htons(portno);					//machine understandable port no
}		

void DistVector::startTimer(r_node &n)
{
	clock_gettime(CLOCK_MONOTONIC, &n.start_time);
}

bool DistVector::timeExpired(r_node &n)
{
	timespec tend={0,0};					//broken into seconds and nanoseconds
	clock_gettime(CLOCK_MONOTONIC, &tend);

	if (((double)tend.tv_sec + 1.0e-9*tend.tv_nsec) - ((double)n.start_time.tv_sec + 1.0e-9*n.start_time.tv_nsec) > 5)
		return true;		//5 second time limit (given)
	else
		return false;
}

//HELPER FUNCTIONS
//~~~~~~~~~~~~~~~~
int DistVector::calc_minDist(int original_cost, int self_Intermediate, int intermediate_Destination, char originalName, char newName, bool &updated){
	
	int newCost = self_Intermediate + intermediate_Destination;
	if(self_Intermediate == inf || intermediate_Destination == inf)
		return original_cost;		//No link; Skip
	
	else if( original_cost == inf)
	{
		updated = true;
		return newCost;			//found new link
	}
	else if (newCost < original_cost)
	{
		updated = true;
		return newCost;			//found new least cost path
	}
	else if (original_cost == newCost)	//cost same
	{	/*
		if (originalName <= newName)	//order by name
			updated = false;
		else{
			updated = true;
			return newCost;
		}*/
		return original_cost;
	}
	else
		return original_cost;			//return whatever updated
}

void DistVector::print(router R[], char name, string msg, bool curr_time){

	cout <<"\t"<< msg << ": " << name << endl;
	if (curr_time)
	{
		time_t rawtime;			//declare time var
		time(&rawtime);			//store current time
		cout <<"\t"<< ctime(&rawtime);//convert time_t to string and 
								//print in time formmat
	}
	cout << " |Destination| Cost  |  Outgoing UDP  |Destination Port |" << endl;
	for (int dest = 0; dest < max_routers; dest++)
	{	if (R[dest].cost() != inf){
			cout << " |     " << nameOf(dest) << "     | ";
			cout <<" "<< R[dest].cost()<<"    |";
			cout <<"      "<< portNoOf(nameOf(indexOf(name)))<< " "<<name<<"   |";
			cout <<"      "<< portNoOf(nameOf(dest) );
			//cout<< " -> "<<nameOf(dest);
			cout<<"      |\n";	
		}
		
	}
	cout << endl;
}
//XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXxx
DistVector::DistVector(char *fil, char *self)
{
	fstream file(fil);

	string line, field;	//current line of the file and the current token
					  	//that is to be put into the file
	char selfname = self[0];
	dv_self = indexOf(self[0]);

	//initialize the entries
	for(int dest=0; dest<max_routers; dest++)
	{
		dv_entries[dest].setnexthop_name('0');
		dv_entries[dest].setnexthop_port(inf);
		dv_entries[dest].setcost(inf);
		dv_entries[dest].setvalidity();
	}
	while(getline(file,line))// parsing file line by line
	{
		stringstream lining(line);
		router entry;
		entry.setvalidity();
		
		getline(lining,field,',');//source router
		char src = field[0];
		
		getline(lining,field,',');//destination router
		int dest = indexOf(field[0]);
		r_node n; n.name=field[0];
		entry.setnexthop_name(field[0]);
		
		getline(lining,field,',');
		int port=atoi(field.c_str());
		entry.setnexthop_port(port);
		//cout<<"Debugging area 1 "<<entry.show_nexthop()<<"\n";
		n.port=port;
//////////////////////
		memset((char *)&n.addr,0,sizeof(n.addr));
		n.addr.sin_family = AF_INET;
		n.addr.sin_addr.s_addr = inet_addr("127.0.0.1");
		n.addr.sin_port = htons(port);
////////////////////
		getline(lining,field,',');
		entry.setcost(atoi(field.c_str()));
		
		if(selfname == 'H')
		{
		 	int i;
		 	for(i = 0; i < dv_neighbours.size(); i++)
		 		if(dv_neighbours[i].name==n.name)
		 			break;
		 	if(i == dv_neighbours.size())
		 		dv_neighbours.push_back(n);	
		}
		else if(src == selfname)
		{
			startTimer(n);
			dv_neighbours.push_back(n);//store neighbours
			dv_entries[dest]=entry;
		}
		dv_portno[n.name] = n.port;
	}
	dv_portno['H'] = 10101;//special port for sending data packet
	memcpy((void*)initial_entries,(void*)dv_entries,sizeof(dv_entries));
	if(nameOf(dv_self)!='H')
		print(dv_entries,nameOf(dv_self),"INITIAL ROUTING TABLE", true);
}
void DistVector::reset(char dead)
{
	for (int i = 0; i < dv_neighbours.size(); i++)
	{
		if (dv_neighbours[i].name == dead)
		{
			if (initial_entries[indexOf(dead)].cost() != inf)
				initial_entries[indexOf(dead)].setinvalidity();
		}
	}
	memcpy((void*)dv_entries, (void*)initial_entries, sizeof(dv_entries));
	print(dv_entries, nameOf(dv_self), "RESET ROUTING TABLE", true);
}
bool DistVector::update(void *adbuf, char src)
{
	router original_entry[max_routers];
	memcpy((void*)original_entry, (void*)dv_entries,sizeof(dv_entries));

	bool updated = false;
	int intermediate = indexOf(src);
	if (initial_entries[intermediate].invalid())
	{
		initial_entries[intermediate].setvalidity();
		dv_entries[intermediate].setvalidity();
		updated = true;
	}
	router adv[max_routers];//loading advertised distance vector
	memcpy((void*)adv,adbuf,sizeof(adv)); 
	//recalculating self's distance vector
	for(int dest=0; dest<max_routers;dest++)
	{
		if(dest == dv_self)
			continue;
		bool updated_entry = false;
		dv_entries[dest].setcost(calc_minDist(dv_entries[dest].cost(), dv_entries[intermediate].cost(), adv[dest].cost(), dv_entries[dest].nexthop_name(), src, updated_entry));
		if (updated_entry)
		{
			updated = true;
			dv_entries[dest].setnexthop_port(portNoOf(src));
			dv_entries[dest].setnexthop_name(src);
			//cout<<"Debugging area 2 "<<dv_entries[dest].show_nexthop()<<" same as "<<src<<"\n";
			
		}
	}
	dv_entries[intermediate].setcost(adv[dv_self].cost());

	if (updated)
	{
		print(original_entry, nameOf(dv_self), "CHANGE DETECTED!\nROUTING TABLE BEFORE CHANGE", true);
		print(adv, src, "DV THAT CAUSED THE CHANGE", false);
		print(dv_entries, nameOf(dv_self), "ROUTING TABLE AFTER CHANGE", false);
	}
	return updated;
}
int DistVector::indexOf(char router)
{
	return router - 'A';
}
char DistVector::nameOf(int index)
{
	return (char)index + 'A';
}
int DistVector::portNoOf(char router)
{
	return dv_portno[router];
}