#include "mpi.h"
#include <stdio.h>
#define MSG_HELLO 100
#define MSG_SIZE 2
#define MSG_REQUEST 44
#define MSG_YES 99
#define JEDINUMBER 5

void sekcjaKrytyczna(int x){
    printf("%d: Jestem w sekcji krytycznej!\n",x);
}
int main( int argc, char **argv )
{
	int rank,size,sender,receiver,it,receivedCounter;
    receivedCounter = 0;
	int msg[MSG_SIZE];
	MPI_Status status;
    int priorities[JEDINUMBER];
	MPI_Init(&argc, &argv);

	MPI_Comm_rank( MPI_COMM_WORLD, &rank );

    for(receiver=0;receiver<JEDINUMBER;receiver+=1){
        if(receiver==rank)
            continue;
        msg[0] = rank;
        MPI_Comm_size( MPI_COMM_WORLD, &size );
        msg[1] = size;
        printf("I %d: Wysylam (%d %d) do %d\n", rank, msg[0], msg[1], receiver);
        MPI_Send( msg, MSG_SIZE, MPI_INT,receiver/*receiver*/, MSG_HELLO, MPI_COMM_WORLD );
    }
    for(sender=0;sender<JEDINUMBER;sender+=1){
        if(sender==rank)
            continue;
		MPI_Recv(msg, MSG_SIZE, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		MPI_Get_count( &status, MPI_INT, &size);
		printf("I %d: Otrzymalem %d wartosci: (%d %d) od %d\n", rank, size, msg[0], msg[1], status.MPI_SOURCE);
        priorities[sender] = msg[0];
    }
    MPI_Barrier(MPI_COMM_WORLD);
    for(it=0;it<JEDINUMBER;it+=1){
        if(it==rank)
            continue;
        if(priorities[it]>rank){
            msg[0] = MSG_YES;
            MPI_Send( msg, MSG_SIZE, MPI_INT,it/*receiver*/, MSG_HELLO, MPI_COMM_WORLD );
            msg[1] = MPI_Comm_size(MPI_COMM_WORLD, &size);
            printf("II %d: Wysylam %d (zgoda) %d do %d\n", rank, msg[0], msg[1], it);
        }
    }
    for(it=0;it<JEDINUMBER;it+=1){
        if(it==rank)
            continue;
        MPI_Recv(msg, MSG_SIZE, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		MPI_Get_count( &status, MPI_INT, &size);
		printf("II %d: Otrzymalem %d wartosci: (%d %d) od %d\n", rank, size, msg[0], msg[1], status.MPI_SOURCE);
        if(msg[0]==MSG_YES)
            receivedCounter+=1;
    }
    if(receivedCounter==JEDINUMBER-1)
        sekcjaKrytyczna(rank);
    for(it=0;it<JEDINUMBER;it+=1){
        if(it==rank)
            continue;
        msg[0] = MSG_YES;
        MPI_Send( msg, MSG_SIZE, MPI_INT,it/*receiver*/, MSG_HELLO, MPI_COMM_WORLD );
        msg[1] = MPI_Comm_size(MPI_COMM_WORLD, &size);
        printf("II %d: Wysylam (%d(zgoda) %d) do %d\n", rank, msg[0], msg[1], it);
    }

	MPI_Finalize();
}