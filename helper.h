#ifndef helper_H
#define helper_H

#include <stdio.h>
#include <vector>
#include <string>
#include <arpa/inet.h>
#include <map>
#include <time.h>
#include <ctime>
#include <string.h>
#include <limits.h>

using namespace std;

#define inf INT_MAX
#define max_routers 6

class router
{
	bool validity;
	int nexthop_ports;
	char nexthop_names;
	int costs;
public: 
	int nexthop_port()  {
		if(invalid())
			return inf;
		else 
			return nexthop_ports;
	}
	char nexthop_name() {
		if(invalid())
			return '0';
		else 
			return nexthop_names;
	}
	int cost() {
		if(invalid())
			return inf;
		else 
			return costs;
	}
	bool invalid(){ return validity;}
	void setnexthop_port(int n){nexthop_ports=n;}
	char show_nexthop(){return nexthop_names;}
	void setnexthop_name(char n){nexthop_names=n;}
	void setcost(int c){costs=c;}
	void setvalidity(){validity=false;}
	void setinvalidity(){validity=true;}
};
class r_node
{
public:
	char name;
	int port;
	timespec start_time;
	sockaddr_in addr;	
};
class DistVector{
	private:
		int dv_self;	//index of self
		router dv_entries[max_routers];
		router initial_entries[max_routers];
		vector<r_node> dv_neighbours;	//list of neighbouring nodes/routers
		sockaddr_in dv_myaddr; 			//MY SOCKET ADDRESS
		map<char, int> dv_portno;

		int calc_minDist(int original, int self_Intermediate, int intermediate_Destination, char originalName, char newName, bool &updated);
		void print(router R[], char name, string msg, bool curr_time);

	public:
		router *getRouter() {return dv_entries; }
		int getSize() { return sizeof(dv_entries);}
		char getName() {return nameOf(dv_self);}
		router routeTo(char dest) {return dv_entries[indexOf(dest)]; }
		vector<r_node> neighbours() { return dv_neighbours; }
		sockaddr_in myaddr() { return dv_myaddr; }
		int port() { return portNoOf(getName()); }
		
		//fn definations
		DistVector(char *fil, char *self);
		void reset(char dead);
		bool update(void *adv, char source);
		int  portNoOf(char router);
		char nameOf(int index);
		int  indexOf(char router);
		void init_addr(int portno);
		void startTimer(r_node &n);
		bool timeExpired(r_node &n);

};
#endif