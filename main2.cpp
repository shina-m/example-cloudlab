#include <iostream>
#include <cstdlib>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sstream>
#include <fstream>
#include <time.h>
#include <math.h>
#include <cmath>
#include <netdb.h>
#include <sys/time.h>
#include <map>
#include <arpa/inet.h>
#include <pthread.h>
#include <vector>
#include <string>
#include <sstream>
#include <iterator>
#include <iostream>
#include <cstdlib>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sstream>
#include <fstream>
#include <time.h>
#include <math.h>
#include <cmath>
#include <netdb.h>
#include <sys/time.h>
#include <map>
#include <arpa/inet.h>
#include <pthread.h>



using namespace std;

#define NODE_COUNT 6
#define HOST_NAME_LEN 2
#define SLEEP_TIME 10

struct sockaddr_in address;
pthread_mutex_t lck;
int sock;
unsigned int port;
char hostname;

void displayError(const char *errorMsg);

struct route_info{

    char dest;
    char nextHop;
    int dist;

}route_table[NODE_COUNT];

struct label_address{

    char name;
    string ip;
    in_addr_t addr;

}neighbors [NODE_COUNT];

struct element_ {
    char  dest;
    int   dist;
};

struct distance_vector_ {
    char sender;
    int  num_of_dests;
    struct element_ content[NODE_COUNT];
} curr_dist_vec;


bool read_config_file(char *LANfileName) {
    string delimiter = " ";
    string token;


    // open the LAN file
    std::ifstream inFile(LANfileName);

    //output error if file doesn't open
    if (!inFile) {
        cout << "Unable to open file \n";
        exit(1);
    }

    //copy each line into a string
    std::string line;

    int j = 0;
    char full_host_name[20];
    gethostname(full_host_name, 20);
    hostname = toupper(full_host_name[0]);
    route_table[0] = {hostname, '-', 0};
    while (std::getline(inFile, line)) {

        istringstream iss(line);
        if (j == 0) {

            port = stoi(line);
            j++;
            continue;
        }

        vector<string> tokens;
        string buf;
        stringstream ss(line); // Insert the string into a stream
        while (ss >> buf)
            tokens.push_back(buf);

        route_table[j] = {tokens[0][0], tokens[0][0], stoi(tokens[1].c_str())};
        neighbors[j - 1] = {tokens[0][0], tokens[2], inet_addr(tokens[2].c_str())};

        j++;
    }

}

int print_routing_table() {

    cout << "Routing Table" << "\n" << "****************" << "\n";
    for (int i = 0; i < (sizeof(route_table) / sizeof(route_info)); i++) {
        if (route_table[i].dest== '\0')
            continue;

        cout << route_table[i].dest << "\t" << route_table[i].dist << "\t" << route_table[i].nextHop <<"\n";
    }

    cout <<"\n";
}

int print_neighbor_info() {

    cout << "Neighbor Info Table" << "\n"
         << "****************" << "\n";
    for (int i = 0; i < (sizeof(neighbors) / sizeof(label_address)); i++) {
        if (neighbors[i].name== '\0')
            continue;

        cout << neighbors[i].name << "\t" << neighbors[i].ip << "\t" << neighbors[i].addr <<"\n";
    }

    cout <<"\n";
}

int getRouteLoc(char a){
    for (int k = 0; k < NODE_COUNT; k++) {

        if (a == route_table[k].dest) {
            return k;
        }
    }
    return -1;
}

void createSocket(int port) {



    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
        cout <<"Error 1 \n";
        //  displayError("There is problem while sending request!");
    }

    int one = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT,&one, sizeof(one));
    const int       optVal = 1;
    const socklen_t optLen = sizeof(optVal);

    int rtn = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void*) &optVal, optLen);



    int leng = sizeof(address);
    //bzero(&address, leng);
    memset(&address, 0, leng);
    address.sin_family = PF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htons(INADDR_ANY);

    if(::bind(sock, (struct sockaddr *) &address, leng) < 0){
        // if (::bind(sock, (struct sockaddr *) &address,leng) < 0) {
        printf("\"createSocket() failed\n\"");
        // exit;
        //displayError("There is problem while sending request!");
    }

}

void sendAdv() {
    struct sockaddr_in neighbor_addr;

    neighbor_addr.sin_family = AF_INET;


    cout << "sending distance vector to neighbor: ";
    for (int i = 0; i < NODE_COUNT && neighbors[i].name != '\0'; i++) {

       /* memset(&echoServAddr, 0, sizeof(echoServAddr));    *//* Zero out structure *//*
        echoServAddr.sin_family = AF_INET;
        echoServAddr.sin_addr.s_addr = inet_addr(servIP);  *//* Server IP address *//*
        echoServAddr.sin_port = htons(echoServPort);*/

        bzero((char *) &neighbor_addr, sizeof(neighbor_addr));
        neighbor_addr.sin_family = AF_INET;
        //inet_pton(AF_INET, neighbors[i].ip.c_str(), &(neighbor_addr.sin_addr.s_addr));
        neighbor_addr.sin_addr.s_addr = inet_addr(neighbors[i].ip.c_str());
        neighbor_addr.sin_port = htons(port);
        unsigned int len=sizeof(struct sockaddr_in);

        cout << neighbors[i].name <<" | ";


        int no = sendto(sock, &curr_dist_vec, sizeof(curr_dist_vec), 0, (const struct sockaddr *) &neighbor_addr, len);
        if (no < 0) {
            cout <<"Error 2 \n";
            //   displayError("There is problem while sending request!");
        }
    }

    cout << "\nsent to all neigbors..." << endl << endl;
}

int update_route(distance_vector_ recv_dist_vec) {

    //  cout << "hello3 \n";

    int dist_to_recv;

    //get cost to sender_node
    for (int i = 0; i < NODE_COUNT; i++) {
        if (neighbors[i].name != '\0' && neighbors[i].name ==  recv_dist_vec.sender) {
            for (int j = 0; j < NODE_COUNT; j++) {
                if (neighbors[i].name == route_table[j].dest) {
                    dist_to_recv = route_table[j].dist;
                }
            }

        }
    }

    pthread_mutex_lock(&lck);

    int rid;

    for(int m=0;m<NODE_COUNT;m++) {
        if (recv_dist_vec.content[m].dest == '\0' || recv_dist_vec.content[m].dest == hostname) {
            continue;
        }

        //for (int k = 0; k < NODE_COUNT; k++) {

        //if route for this node already exists
        rid = getRouteLoc(recv_dist_vec.content[m].dest);
        if (rid != -1) {
            //check is new cost will be smaller
            // cout << "what4\n\n";
            // cout << recv_dist_vec.content[m].dest << "\n\n";
            if ((recv_dist_vec.content[m].dist + dist_to_recv) < route_table[rid].dist) {
                // if yes, update cost and nexthop
                route_table[rid].dist = recv_dist_vec.content[m].dist + dist_to_recv;
                route_table[rid].nextHop = recv_dist_vec.sender;
            }

        } else
            for (int k = 0; k < NODE_COUNT; k++) {

                if (route_table[k].dest == '\0') {
                    route_table[k].dest = recv_dist_vec.content[m].dest;
                    route_table[k].dist = recv_dist_vec.content[m].dist + dist_to_recv;
                    route_table[k].nextHop = recv_dist_vec.sender;
                    break;
                }
            }
    }



   pthread_mutex_unlock(&lck);

}

int generate_distance_vector(){
    int j = 0;

    for (int i = 0; i < NODE_COUNT; i++) {

        if (route_table[i].dest== '\0')
            continue;
        curr_dist_vec.content[j] = {route_table[i].dest, route_table[i].dist};
        j++;

    }

    curr_dist_vec.sender = toupper(hostname);
    curr_dist_vec.num_of_dests=j;

}

int print_distance_vector(distance_vector_ dist_vec) {

    cout << "Generated Distance Vector" << "\n" << "****************" << "\n";
    cout << dist_vec.sender << "\n";
    cout << dist_vec.num_of_dests << "\n";

    for (int i = 0; i < NODE_COUNT; i++) {
        if (dist_vec.content[i].dest == '\0')
            continue;

        cout << dist_vec.content[i].dest << "\t" << dist_vec.content[i].dist << "\n";
    }

    cout <<"\n";
}

void receive(int rsock){

    cout << "hello2 \n";
    distance_vector_ recv_dist_vec;

    unsigned int len=sizeof(struct sockaddr_in);
    int no=1;

    no = recvfrom(rsock,&recv_dist_vec,sizeof(recv_dist_vec),0,(struct sockaddr *)&address, &len);
    if(no<0){
        cout <<"Error3\n";
        // displayError("recv error");
    }

    cout<<"Routing table received from: " << recv_dist_vec.sender <<endl;

    update_route(recv_dist_vec);
    print_routing_table();
    generate_distance_vector();
}

/*
 * Function to call a new thread
 */
void *recv_adv(void *recv_sock){
        cout << "hello1 \n";
        int rsock = *(int *) recv_sock;

        cout << "Created receive thread\n\n";

        while(1){
            receive(rsock);
            cout <<"hello3";
        }

}


int main(int argc, char *argv[]) {

    pthread_t recv_thread;

    char config_file_dir[500];

    strcpy(config_file_dir, argv[1]);

    if (argc != 2)         //Test for correct number of parameters
    {
        cout << "Usage:  No config file \n";
        exit(1);
    }

    read_config_file(config_file_dir);
    generate_distance_vector();
    cout << "\nPort No: " << port << "\n****************" <<"\n\n";
    print_neighbor_info();
    print_routing_table();
    print_distance_vector(curr_dist_vec);

    createSocket(port);
   // sendAdv();
   /* distance_vector_ recv_dist_vec;

    recv_dist_vec.sender = 'B';
    recv_dist_vec.num_of_dests = 4;
    recv_dist_vec.content[0] = {'S', 1};
    recv_dist_vec.content[1] ={'A',4};
    recv_dist_vec.content[2] ={'C',2};
    recv_dist_vec.content[3] ={'F',3};

    update_route(recv_dist_vec);
    print_routing_table();*/

    //Initializes mutex lock
   if (pthread_mutex_init(&lck, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }

    pthread_create(&recv_thread,NULL,recv_adv,(void*)&sock);

    sleep(SLEEP_TIME);

    for(;;){
        sendAdv();
        sleep(SLEEP_TIME);


    }

   pthread_mutex_destroy(&lck);

    return 0;
}
