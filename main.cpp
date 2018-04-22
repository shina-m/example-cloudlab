#include <iostream>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sstream>
#include <fstream>
#include <time.h>
#include <cmath>
#include <map>
#include <arpa/inet.h>
#include <vector>

using namespace std;

#define NODE_COUNT 10 //Maximum number of nodes the network can have
#define SLEEP_TIME 15 //interval between each dv table advert

struct sockaddr_in address;
pthread_mutex_t lck;
int sock; //for socket to be created
unsigned int port; // port number for advertisement
char hostname; //the hostname for node currently running this program

/*
 * stores the routing information for each destination
 * */
struct route_info{
    char dest;
    char nextHop;
    int dist;

}route_table[NODE_COUNT];

/*
 * maps node name to its ip address
 * */
struct label_address{

    char name;
    string ip;
    in_addr_t addr;

}neighbors [NODE_COUNT];


struct element_ {
    char  dest;
    int   dist;
};

/*
 * Struct for distance vector to be advertised
 * */
struct distance_vector_ {
    char sender;
    int  num_of_dests;
    struct element_ content[NODE_COUNT];
} curr_dist_vec;

/*
 * This generates an up-to-date distance vector based on the current routing table
 * */
void generate_distance_vector(){
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


/*
 * Reads the config file for this node and uses it to generate the neighbors table and
 * initial routing table
 * */
void read_config_file(char *config_file) {
    string delimiter = " ";
    string token;

    // open the config file
    std::ifstream inFile(config_file);

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

    //generate intial distance vector
    generate_distance_vector();

    //display port number
    cout << "\nPort No: " << port << "\n" << "****************" << "\n\n";

}

/*
 * Prints out the routing table
 * */
void print_routing_table() {

    cout << "Current Routing Table" << "\n" << "****************\n";
    for (int i = 0; i < (sizeof(route_table) / sizeof(route_info)); i++) {
        if (route_table[i].dest== '\0')
            continue;

        cout << route_table[i].dest << "\t" << route_table[i].dist << "\t" << route_table[i].nextHop <<"\n";
    }

    cout << "****************\n\n";
}

/*
 * Prints out the neighbors table
 * */
void print_neighbor_info() {

    cout << "Neighbors" << "\n"
         << "**********" << "\n";
    for (int i = 0; i < (sizeof(neighbors) / sizeof(label_address)); i++) {
        if (neighbors[i].name== '\0')
            continue;

        cout << neighbors[i].name << "\t" << neighbors[i].ip <<"\n";
    }

    cout << "**********" << "\n\n";
}

/*
 * This gets the location of a route information from the route table
 * */
int get_route(char a){
    for (int k = 0; k < NODE_COUNT; k++) {
        if (a == route_table[k].dest) {
            return k;
        }
    }
    return -1;
}

/*
 * Creates a socket to be used for sending and receiving distance vectors
 * */
void createSocket(int port) {
    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
        cout <<"Error: Socket could not be created \n";
        exit(0);
    }

    //This allows multiple processes to bind to the same port concurrently
    int one = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT,&one, sizeof(one));
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void*) &one, sizeof(one));

    //sets up socket parameters and binds process to specified port
    int leng = sizeof(address);
    bzero(&address, leng);
    address.sin_family = PF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htons(INADDR_ANY);

    if(::bind(sock, (struct sockaddr *) &address, leng) < 0){
        printf("\"createSocket() failed\n\"");
         exit(0);
    }
}

/*
 * Sends its current distance vector to all its neighbours
 * */
void send_dv() {
    struct sockaddr_in neighbor_addr;

    neighbor_addr.sin_family = AF_INET;

    cout << "sending distance vector to neighbor: ";

    for (int i = 0; i < NODE_COUNT && neighbors[i].name != '\0'; i++) {

        inet_pton(AF_INET, neighbors[i].ip.c_str(), &(neighbor_addr.sin_addr.s_addr));
        neighbor_addr.sin_port = htons(port);
        unsigned int len = sizeof(struct sockaddr_in);
        cout << neighbors[i].name << "  ";

        int no = sendto(sock, &curr_dist_vec, sizeof(curr_dist_vec), 0,
                        (const struct sockaddr *) &neighbor_addr, len);
        if (no < 0) {
            cout <<"Error Sending an advert to " <<neighbors[i].name << "\n";
            //exit(0);
        }
    }

    cout << "\nsent to all neighbors..." << endl << "\n";
}


/*
 * This is called after receiving a distance vector from a neighbor. it updates the routing table
 * */
void update_route(distance_vector_ recv_dist_vec) {
    cout <<"Updating routing table....\n\n";

    int dist_to_recv;
    int route_index;

    //get the cost from this node to the sender node
    for (int i = 0; i < NODE_COUNT; i++) {
        if (neighbors[i].name != '\0' && neighbors[i].name ==  recv_dist_vec.sender) {
            for (int j = 0; j < NODE_COUNT; j++) {
                if (neighbors[i].name == route_table[j].dest) {
                    dist_to_recv = route_table[j].dist;
                }
            }
        }
    }

    // add a lock for this method so that only one process can edit the routing table at a time
    pthread_mutex_lock(&lck);

    for(int m=0;m<NODE_COUNT;m++) {
        //if the current vector is for this node, skipp
        if (recv_dist_vec.content[m].dest == '\0' || recv_dist_vec.content[m].dest == hostname) {
            continue;
        }

        route_index = get_route(recv_dist_vec.content[m].dest);

        //if route for this node already exists
        if (route_index != -1) {
            //If the calculate cost is less than the current cost, update routing table
            if ((recv_dist_vec.content[m].dist + dist_to_recv) < route_table[route_index].dist) {

                route_table[route_index].dist = recv_dist_vec.content[m].dist + dist_to_recv;
                route_table[route_index].nextHop = recv_dist_vec.sender;
            }

        }
        // Else if the destination is not already in the routing table, add its entry
        else
        {
            for (int k = 0; k < NODE_COUNT; k++) {

                if (route_table[k].dest == '\0') {
                    route_table[k].dest = recv_dist_vec.content[m].dest;
                    route_table[k].dist = recv_dist_vec.content[m].dist + dist_to_recv;
                    route_table[k].nextHop = recv_dist_vec.sender;
                    break;
                }
            }
        }
    }

    //remove lock
    pthread_mutex_unlock(&lck);

}


/*
 * Outputs the current distance vector
 * */
void print_distance_vector(distance_vector_ dist_vec) {

    cout << "****************" << "\n";
    cout << dist_vec.sender << "\n";
    cout << dist_vec.num_of_dests << "\n";

    for (int i = 0; i < NODE_COUNT; i++) {
        if (dist_vec.content[i].dest == '\0')
            continue;

        cout << dist_vec.content[i].dest << "\t" << dist_vec.content[i].dist << "\n";
    }

    cout << "****************" << "\n\n";
}


/*
 * Function to await receipt of dv from neighbours
 * */
void receive(int rsock){

    distance_vector_ recv_dist_vec;

    unsigned int len=sizeof(struct sockaddr_in);
    int no=1;

    no = recvfrom(rsock,&recv_dist_vec,sizeof(recv_dist_vec),0,(struct sockaddr *)&address, &len);
    if(no<0){
        cout <<"Error Creating socket listener\n";
        exit(0);
    }

    cout<<"The following DV was received from: " << recv_dist_vec.sender <<endl;
    print_distance_vector(recv_dist_vec);

    update_route(recv_dist_vec);
    print_routing_table();
    generate_distance_vector();
}

/*
 * Function to call a new thread for receiving dv
 */
void *recv_adv(void *recv_sock){

    cout<<"receiver thread created\n\n";
    while(1){
        int rsock=*(int*)recv_sock;
        receive(rsock);
    }

}

int main(int argc, char *argv[]) {
    //initializes recieve thread
    pthread_t recv_thread;
    char config_file_dir[1000];

    if (argc != 2)         //Test for correct number of parameters
    {
        cout << "Usage:  No config file \n";
        exit(1);
    }

    //get config directory
    strcpy(config_file_dir, argv[1]);

    //parse through config file
    read_config_file(config_file_dir);

    //display neighbor info
    print_neighbor_info();

    //display intial routing table
    print_routing_table();

    //display initial distance vector
    cout << "Current Distance Vector" << "\n";
    print_distance_vector(curr_dist_vec);

    //create socket
    createSocket(port);

    //send first distance vector
    send_dv();

    //Initializes mutex lock
    if (pthread_mutex_init(&lck, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }

    //create parallel thread for receiving dv
    pthread_create(&recv_thread,NULL,recv_adv,(void*)&sock);

    //continue sending dv periodically
    while(1){
        sleep(SLEEP_TIME);
        send_dv();

    }

    close(sock);
    pthread_mutex_destroy(&lck);
    return 0;
}
