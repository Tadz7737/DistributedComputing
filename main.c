#include "mpi.h"
#include "def.h"
#include <stdio.h>

void criticalSection(int x){
    printf("%d: I am in critical section!\n",x);
}
int main( int argc, char **argv )
{
	int rank,size,receiver,it,receivedCounter,requester;
    receivedCounter = 0;
	int msg[MSG_SIZE];
	MPI_Status status;
    int priorities[JEDINUMBER];
	MPI_Init(&argc, &argv);

	MPI_Comm_rank( MPI_COMM_WORLD, &rank );

    //TODO
    //Implementation of logical clock

    //sending MSG_REQUEST
    for(receiver=0;receiver<JEDINUMBER;receiver+=1){
        if(receiver==rank)
            continue;
        msg[0] = rank;
        MPI_Comm_size( MPI_COMM_WORLD, &size );
        msg[1] = size;
        msg[2] = MSG_REQUEST;
        printf("I %d: Sending MSG_REQUEST to %d\n", rank, receiver);
        MPI_Send( msg, MSG_SIZE, MPI_INT,receiver/*receiver*/, MSG_HELLO, MPI_COMM_WORLD );
    }

    //determine first process to enter critical section
    //main loop
    while(receivedCounter!=(JEDINUMBER-1)){
		MPI_Recv(msg, MSG_SIZE, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		MPI_Get_count( &status, MPI_INT, &size);
		printf("I %d: Received %d values: (rank: %d message: %d) from %d\n", rank, size, msg[0], msg[2], status.MPI_SOURCE);
        requester = msg[0];
        priorities[requester] = requester;
        if(msg[2] == MSG_REQUEST){ //if entry to critical section is requested
            if(priorities[requester]>rank){
                msg[0] = rank;
                msg[2] = MSG_YES;
                MPI_Comm_size(MPI_COMM_WORLD, &size);
                msg[1] = size;
                printf("I %d: Sending approval to %d\n", rank, requester);
                MPI_Send( msg, MSG_SIZE, MPI_INT,requester/*requester*/, MSG_HELLO, MPI_COMM_WORLD );
            }
        }
        else if(msg[2] == MSG_YES){
            receivedCounter+=1;
        }
    }

    if(receivedCounter==JEDINUMBER-1) 
        criticalSection(rank);   //enter critical section
    //send approval after leaving the critical section
    for(it=0;it<JEDINUMBER;it+=1){
        if(it==rank)
            continue;
        
        msg[2] = MSG_YES;
        msg[0] = rank;        
        MPI_Comm_size(MPI_COMM_WORLD, &size);
        msg[1] = size;
        printf("II %d:  Sending approval to %d\n", rank, it);
        MPI_Send( msg, MSG_SIZE, MPI_INT,it/*receiver*/, MSG_HELLO, MPI_COMM_WORLD );
    }

	MPI_Finalize();
    return 0;
}