
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
// Constants for train direction and priority
#define EAST 'e'
#define WEST 'W'
#define HIGH_PRIORITY 1
#define LOW_PRIORITY 0
#define NANOSECOND_CONVERSION 1e9
// Train structure
typedef struct {
    int number;         // Train number
    char direction;     // 'e' for East, 'W' for West
    int priority;       // Priority level: 1 for HIGH, 0 for LOW
    float loading_time;   // Time it takes for the train to load
    float crossing_time;  // Time it takes for the train to cross
    pthread_cond_t green; // green loight to corss 
} Train;

pthread_mutex_t start;
pthread_cond_t ready;
bool print = false;
pthread_mutex_t track;
bool flag = false;
int count =  0;
pthread_cond_t signal;
pthread_cond_t dispatcher;
int waiting_trains = 0;
double global_start_time;

char* formatTime(double seconds);
void prependAndShift(char arr[], char c);
void dispatch(int num_trains, pthread_cond_t** convars);
void *train_thread(void *arg);
// Function to read the input file and initialize trains
int initialize_trains(const char *filename, Train **trains, int *num_trains) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        return -1;  // Return an error code
    }

    // First, find out how many trains are there in the file
    char ch;
    *num_trains = 0;
    while(!feof(file)) {
        ch = fgetc(file);
        if(ch == '\n') {
            (*num_trains)++;
        }
    }
    // Go back to the start of the file
    rewind(file);
    // Allocate memory for all trains
    *trains = (Train *)malloc((*num_trains) * sizeof(Train));
    if (*trains == NULL) {
        perror("Failed to allocate memory");
        fclose(file);
        return -1;
    }
    // Read file and initialize train structs
    for (int i = 0; i < *num_trains; ++i) {
        // Assuming the file format is as follows: [Direction] [Loading time] [Crossing time]
        // For example: e 10 6
        // This represents an Eastbound train taking 10 seconds to load and 6 seconds to cross
        char direction;
        float loading_time;
        float crossing_time;

        if (fscanf(file, " %c %f %f", &direction, &loading_time, &crossing_time) != 3) {
            fprintf(stderr, "Error reading file: malformed line\n");
            free(*trains);
            fclose(file);
            return -1;
        }

        (*trains)[i].number = i;  // Train number is set based on the sequence in the file
        (*trains)[i].direction = direction;
	// During your train initialization, for each train, do:
        //pthread_cond_init(&(*trains)[i].green, NULL);

        if (direction == 'E' || direction == 'W'){ (*trains)[i].priority = 1;}
      	else {(*trains)[i].priority = 0;}	// Set priority based on direction
        (*trains)[i].loading_time = loading_time / 10.0;  // Convert to seconds if necessary
        (*trains)[i].crossing_time = crossing_time / 10.0;  // Convert to seconds if necessary
    }

    fclose(file);
    return 0;  // Success
}

//Time conversstion -> Taken from sample code provided 
double timespec_to_seconds(struct timespec *ts)
{
	return ((double) ts->tv_sec) + (((double) ts->tv_nsec) / NANOSECOND_CONVERSION);
}
double get_current_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return timespec_to_seconds(&ts);
}
double get_elapsed_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return timespec_to_seconds(&ts) - global_start_time;  // Calculate elapsed time
}

typedef struct TrainNode {
    Train train;
    struct TrainNode* next;
} TrainNode;

//BOTH STATIONS

TrainNode* west_station = NULL;
TrainNode* east_station = NULL;


TrainNode* create_train_node(Train train_data) {
    TrainNode* new_node = (TrainNode*)malloc(sizeof(TrainNode));
    new_node->train = train_data;
    new_node->next = NULL;
    return new_node;
}

Train dequeue_train(TrainNode** head) {
    if (*head == NULL) {
        fprintf(stderr, "Error: Queue is empty. Cannot dequeue.\n");
        exit(1);  // or handle error as appropriate
    }

    TrainNode* temp = *head;
    Train train_data = temp->train;

    *head = (*head)->next;
    free(temp);

    return train_data;
}

void enqueue_train_based_on_priority(TrainNode** head, Train train_data) {

    TrainNode* new_node = create_train_node(train_data);
    TrainNode* current;

    // If the list is empty or the new node has higher priority than the first node
    if (*head == NULL || (*head)->train.priority < new_node->train.priority) {
        new_node->next = *head;
        *head = new_node;
    }
    else {
        current = *head;
        while (current->next != NULL && current->next->train.priority >= new_node->train.priority) {
            current = current->next;
        }
        new_node->next = current->next;
        current->next = new_node;
    }
}

void print_trains(TrainNode* head) {
    printf("Enetred Print\n");
    TrainNode* temp = head;
    while (temp != NULL) {
        printf("Train %d (Priority: %d)\n", temp->train.number, temp->train.priority);
        temp = temp->next;
    }
}

// Main function
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }
    Train *trains = NULL;
    int num_trains;
    
    if (initialize_trains(argv[1], &trains, &num_trains) != 0) {
        fprintf(stderr, "Failed to initialize trains from file\n");
        return 1;
    } 
    pthread_t tid [num_trains];
    pthread_cond_t* convars [num_trains];

    for (int i = 0; i < num_trains; ++i) {
    	pthread_create(&tid[i], NULL, train_thread, (void*)&trains[i]);
	convars[i] = &trains[i].green;
    }
    sleep(1);
    pthread_mutex_lock(&start);
    print= true;
    
    global_start_time = get_current_time();  // Set the global start time here
    

    pthread_cond_broadcast(&ready);
    pthread_mutex_unlock(&start);
    
    dispatch(num_trains ,  convars);

    for(size_t i = 0; i < num_trains; ++i)
	{
		pthread_join(tid[i], NULL);
	}
    // Clean up
    free(trains);
    return 0;
}

void precise_sleep(float seconds) {
    struct timespec ts;
    ts.tv_sec = (time_t)seconds;  // Truncate the float to get whole seconds
    ts.tv_nsec = (long)((seconds - (float)ts.tv_sec) * 1e9);  // Get the remaining fraction of a second, converted to nanoseconds
    nanosleep(&ts, NULL);
}

void prependAndShift(char arr[], char c) {
    // Shift elements to the right, starting from the last element
    for (int i = 2; i > 0; i--) {
        arr[i] = arr[i - 1];
    }
    // Add the new character at the 0th index
    arr[0] = c;
}
void dispatch(int num_trains, pthread_cond_t** convars){
   int a = 0;
   Train t;
   bool cond = false;
   char history[3] = {'\0','\0','\0'};
   while(a<num_trains){	    
            precise_sleep(0.004444);	   
	   while(west_station == NULL && east_station==NULL){
	  
		   
	   } 
	   precise_sleep(0.08);

	   if(((history[0]=='w'||history[0]=='W') && (history[1]=='w' || history[1]=='W') && (history[2]=='w'||history[2]=='W'))&& east_station!=NULL){
	   	pthread_mutex_lock(&track);
		flag=true;
		t= dequeue_train(&east_station);
		prependAndShift(history, t.direction);
		pthread_cond_signal(convars[t.number]);
		pthread_mutex_unlock(&track);
		a=a+1;
	   }
	   else if(((history[0]=='e'||history[0]=='E') && (history[1]=='e' || history[1]=='E') && (history[2]=='e'||history[2]=='E'))&& west_station!=NULL){
                pthread_mutex_lock(&track);
                flag=true;
                t= dequeue_train(&west_station);
                prependAndShift(history, t.direction);
                pthread_cond_signal(convars[t.number]);
                pthread_mutex_unlock(&track);
                a=a+1;
           }
	   else if(west_station!=NULL && east_station==NULL){
	   pthread_mutex_lock(&track);
	   flag=true;
	   t= dequeue_train(&west_station);
	   prependAndShift(history, t.direction);
	   pthread_cond_signal(convars[t.number]);
	   pthread_mutex_unlock(&track); 
	   a=a+1;
	   }
	   else if(west_station==NULL && east_station!=NULL){
	   pthread_mutex_lock(&track);
	   flag=true;
	   pthread_cond_signal(convars[east_station->train.number]);
	   t = dequeue_train(&east_station);
	   prependAndShift(history, t.direction);
	   pthread_mutex_unlock(&track);
	   a=a+1;   
	   }
	   else if(west_station!=NULL && east_station!=NULL){
		   	   	
		   if(west_station->train.priority == east_station->train.priority){
	           if(history[0]=='\0' || history[0]=='E' || history[0]=='e'){
		   pthread_mutex_lock(&track);
		   flag=true;
		   pthread_cond_signal(convars[west_station->train.number]);
		   pthread_mutex_unlock(&track);
		   t = dequeue_train(&west_station);
		   prependAndShift(history, t.direction);
		   a=a+1;
		   }
		   else if(history[0]=='W' || history[0]=='w'){
		   pthread_mutex_lock(&track);
		   flag=true;
		   pthread_cond_signal(convars[east_station->train.number]);
		   pthread_mutex_unlock(&track);
		   t = dequeue_train(&east_station);
		   prependAndShift(history, t.direction);
		   a=a+1;
		   }
		   }
		   else if(west_station->train.priority > east_station->train.priority){
		  
		   pthread_mutex_lock(&track);
		   flag=true;
		   pthread_cond_signal(convars[west_station->train.number]);
		   pthread_mutex_unlock(&track);
		   t=dequeue_train(&west_station);
		   prependAndShift(history, t.direction);
		   a=a+1;
		   }
		   else if( east_station->train.priority > west_station->train.priority){
	           pthread_mutex_lock(&track);
		   flag=true;
		   pthread_cond_signal(convars[east_station->train.number]);
		   pthread_mutex_unlock(&track);
		   t=dequeue_train(&east_station);
		   prependAndShift(history, t.direction);
		   a=a+1;
		   }
	   
	   }
   

   }
}


char* formatTime(double seconds) {
    int hours = (int)seconds / 3600;
    seconds -= hours * 3600;
    int minutes = (int)seconds / 60;
    seconds -= minutes * 60;
    int secs = (int)seconds;
    int tenths = (int)(seconds * 10) % 10; // Get the tenths of a second

    char* formattedTime = (char*)malloc(12 * sizeof(char)); // "hh:mm:ss.t\0"
    if (formattedTime == NULL) {
        perror("Failed to allocate memory for formatted time");
        exit(1);
    }

    snprintf(formattedTime, 12, "%02d:%02d:%02d.%d", hours, minutes, secs, tenths);
    return formattedTime;
}

void *train_thread(void *arg){
	Train* train = (Train*)arg;
        
	pthread_cond_init(&train->green,NULL);	
	pthread_mutex_lock(&start);

	while(!print){
	
		pthread_cond_wait(&ready,&start);
	}
	      	
	pthread_mutex_unlock(&start);
		
	float lt= train->loading_time;       
	precise_sleep(lt);
        char* direction;
        if( train->direction == 'w' || train->direction=='W'){ direction = "West";}
        else{direction="East";}
	double received_time = get_elapsed_time();  // Here, we get the elapsed time
        char* ft = formatTime(received_time);
       	printf("%s: Train %d is ready to go %s\n", ft, train->number, direction);
	
	if(train->direction == 'W' || train->direction == 'w'){
        
          enqueue_train_based_on_priority(&west_station, *train); 
         
	}
       
        else if(train->direction == 'E' || train->direction =='e'){ 
        
           enqueue_train_based_on_priority(&east_station, *train); 
        
       	}
	 
        
	pthread_mutex_lock(&track);
	
	waiting_trains++;
	while(!flag){
	
		
		pthread_cond_wait(&(train->green),&track);
	}
        	
	double r_time = get_elapsed_time();
	char* ft2 = formatTime(r_time);	
	printf("%s: Train %d is ON the main track going %s\n", ft2, train->number,direction);
	precise_sleep(train->crossing_time);
	printf("%s: Train %d is OFF the main track after going %s\n", formatTime(get_elapsed_time()), train->number,direction);
	pthread_mutex_unlock(&track);


	free(ft);
	free(ft2);
    	pthread_exit(NULL);

	return NULL;
}
