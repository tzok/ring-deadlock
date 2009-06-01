/*
 * node.cpp
 *
 *  Created on: 2009-05-16
 *      Author: chriss, tzok
 */

#include "def.h"

using namespace std;

bool busy = true;
int myId; /* nodes ordinal number */
int tids[NODENUM]; /* node task ids */
int route[NODENUM]; /* 1-next hop */
map<int, int> resources;/* owners */
map<int, list<int> > myResources;/* who currently uses them / -1 available */

/* currently requested resources */
list<int> request;
list<int> resInUse;
int minNeeded;

inline int pos(int base, int offset) {
	return (base + offset + NODENUM) % NODENUM;
}

inline int abs(int a){
	return a > 0 ? a : -a;
}

/*
 * Read message from some other node
 */
void fetchMsg(Msg& msg, int msgtag) {
	int buf[BUF_SIZE];
	msg.setType(msgtag);
	int size = msg.size - 1;

	pvm_upkint(buf, size, 1);
	msg.setMsg(myId, buf);
}

/*
 * Send message to some other node
 */
void sendMsg(Msg msg) {
	int buf[BUF_SIZE];
	int size = msg.toIntArray(buf);



	pvm_initsend(PvmDataDefault);
	pvm_pkint(buf, size, 1);
	pvm_send(tids[route[msg.dest]], msg.type);
}

/*
 * Forward message
 */
void forwardMsg(int destination, int msgtag) {
	int size = Msg::getSize(msgtag) - 1;
	int dest = destination;


	printf("*%d forwarding to %d\n", myId, route[dest]);
	/*
	 * Receive remaining data
	 */
	int buf[BUF_SIZE];
	pvm_upkint(buf, size, 1);

	/*
	 * Pack the data
	 */
	pvm_initsend(PvmDataDefault);
	pvm_pkint(&dest, 1, 1);
	pvm_pkint(buf, size, 1);

	/*
	 * Route it
	 */
	pvm_send(tids[route[dest]], msgtag);
}

/*
 * Grant a resource
 */
void grantResource(int resId) {
	if (myResources[resId].empty())
		return;
	Msg msg;
	msg.setType(MSG_GRANT);
	msg.data = resId;
	msg.from = myId;
	msg.dest = *myResources[resId].begin();
	sendMsg(msg);
}

/*
 * Free a resource
 */
void freeResources() {

	list<int>::iterator it;
	printf("Freeing resources: ");
		for(it = resInUse.begin();it!=resInUse.end();++it)
			printf("%d ", *it);
		printf("\n");
		fflush(0);
		fflush(0);

	Msg msg;
	msg.setType(MSG_FREE);
	msg.from = myId;


	for (it = resInUse.begin(); it != resInUse.end(); ++it) {
		if (resources[*it] == myId) {
			myResources[*it].erase(myResources[*it].begin());
			grantResource(*it);
		} else {
			msg.data = *it;
			msg.dest = resources[*it];
			sendMsg(msg);
		}
	}
	resInUse.clear();
}

/*
 * Cancel resource allocation
 */
void cancelResources() {

	list<int>::iterator it;
	printf("Cancelling resources: ");
		for(it = request.begin();it!=request.end();++it)
			printf("%d ", *it);
		printf("\n");
		fflush(0);
		fflush(0);

	Msg msg;
	msg.setType(MSG_CANCEL);
	msg.from = myId;


	for (it = request.begin(); it != request.end(); ++it) {
		if (resources[*it] == myId) {
			myResources[*it].erase(myResources[*it].begin());
			grantResource(*it);//check if sb is waiting for res
		} else {
			msg.data = *it;
			msg.dest = resources[*it];
			sendMsg(msg);
		}
	}
	request.clear();
}

/*
 * Action when a resource was granted
 */
void resourceGranted(int resId) {
	printf("Res GRANTED %d\n", resId);
	fflush(0);
	fflush(0);
	resInUse.insert(resInUse.begin(), resId);
	request.remove(resId);

	if (resInUse.size() >= minNeeded)
		cancelResources();
}

/*
 * Request a resource
 */
void requestRes() {
	request.clear();
	resInUse.clear();

	/*
	 * Prepare list of available resources
	 */
	int i;
	list<int> available;
	for (i = 0; i < RESOURCE_NUM; ++i)
		available.insert(available.begin(), i);

	/*
	 * Get random number of resources to request
	 */
	int count = REQUEST_MIN + rand() % (REQUEST_MAX - REQUEST_MIN + 1);
	minNeeded = 1 + rand() % count;

	/*
	 * Generate random request list
	 */
	list<int>::iterator it;
	while (count--) {//get $count resources from all possible
		i = 0;
		it = available.begin();
		int pos = rand() % available.size();
		while (i++ < pos)
			++it;

		request.insert(request.begin(), *it);
		available.erase(it);
	}

	printf("\n\nResources needed(%d): ", minNeeded);
	for(it = request.begin();it!=request.end();++it)
		printf("%d ", *it);
	printf("\n");
	fflush(0);
	fflush(0);

	/*
	 * Request resources
	 */
	printf("my resources: ");
	for (it = request.begin(); it != request.end(); ++it) {
		if (resources[*it] == myId) {//add myself to my queue ^^
			if (myResources[*it].empty()) {//
				resInUse.insert(resInUse.begin(), *it);
				printf("%d ", *it);
			}
			myResources[*it].insert(myResources[*it].end(), myId);

		} else {//send requests
			Msg msg;
			msg.setType(MSG_REQUEST);
			msg.dest = resources[*it];
			msg.from = myId;
			msg.data = *it;
			sendMsg(msg);
		}
	}

	printf("\n");
	fflush(0);
	fflush(0);
	for (it = resInUse.begin(); it != resInUse.end(); ++it)
		request.remove(*it);
}

/*
 * Prepare routing table
 */
void createRoute() {
	//int edge = pos(myId, NODENUM / 2);
	int next;
	route[myId] = -1;
	for (int i = myId+1; i < NODENUM + myId; ++i) {
		if (i-myId< NODENUM/2)
			next = +1;
		else
			next = -1;

		route[i%NODENUM] = pos(myId,next);
	}
}

///////////////////////////////////////////////////////////////////////////////
int main() {
	/*
	 * Wait until all other nodes are active
	 */
	pvm_joingroup("init");
	pvm_barrier("init", NODENUM + 1);

	/*
	 * Read data
	 */
	int i, resOwner;

	pvm_recv(-1, MSG_INIT);
	pvm_upkint(&myId, 1, 1);
	pvm_upkint(tids, NODENUM, 1);
	for (i = 0; i < RESOURCE_NUM; ++i) {
		pvm_upkint(&resOwner, 1, 1);
		resources[i] = resOwner;
	}

	/*
	 * Prepare routing over token topology
	 */
	srand(myId);
	createRoute();



	/*
	 * ACTIVITY
	 */
	while (true) {
		/*
		 * Prepare timeout structure
		 */
		struct timeval to;
		to.tv_sec = 1;
		to.tv_usec = 0;

		/*
		 * If no requests were made and no resources are in use
		 */
		if(!myId)
			sleep(1+rand()%5);


		if ((myId==0) && request.empty() && resInUse.empty())
			if (((float) rand() / (float) RAND_MAX) < REQUEST_PROB) {
				requestRes();
				if (resInUse.size() < minNeeded)
					busy = false;
				else
					busy = true;
			}

		/*
		 * If no requests were made, but some resources are in use
		 */
		if (request.empty() && !resInUse.empty()) {
			busy = true;
			if (((float) rand() / (float) RAND_MAX) < FREE_PROB)
				freeResources();
		}

		/*
		 * Check incoming messages
		 */
		int bufid;
		usleep(100);
		if (!(bufid = pvm_trecv(-1, -1, &to)))
			continue;

		/*
		 * Read incoming message
		 */
		int dest, msgsize, msgtag, sender;
		pvm_bufinfo(bufid, &msgsize, &msgtag, &sender);
		pvm_upkint(&dest, 1, 1);

		/*
		 * Forward message
		 */
		if (dest != myId) {
			printf("* %d dest %d *\n", myId, dest);
			forwardMsg(dest, msgtag);
			continue;
		}

		/*
		 * Or process it
		 */
		Msg msg;
		fetchMsg(msg, MSG_REQUEST);


		switch (msgtag) {
		case MSG_REQUEST:
			printf("%d Resource Requestd %d\n", myId, msg.data);

			/*
			 * Put the resource into the queue and grant it immediately if it
			 * was free before
			 */
			myResources[msg.data].push_back(msg.from);
			if (*myResources[msg.data].begin() == msg.from){
				printf("granted!\n");
				grantResource(msg.data);

			}
			else
				printf("THIS SHOULD NOT HAPPEN\n");

			break;
		case MSG_FREE:
			printf("%d Resource freed %d\n", myId, msg.data);
			/*
			 * Grant the resource to the next node on the list
			 */
			if (*myResources[msg.data].begin() == msg.from) {
				myResources[msg.data].pop_front();
				grantResource(msg.data);
			}
			break;
		case MSG_CANCEL:
			printf("%d Resource canceled %d\n", myId, msg.data);
			/*
			 * Remove the cancelling node from the list and grant the
			 * resource to some other node if necessary
			 */
			if (*myResources[msg.data].begin() == msg.from) {
				myResources[msg.data].pop_front();
				grantResource(msg.data);
			} else
				myResources[msg.data].remove(msg.from);
			break;
		case MSG_GRANT:
			resourceGranted(msg.data);
			break;
		default:
			break;
		}

		fflush(0);
		fflush(0);

	}
	pvm_exit();
}