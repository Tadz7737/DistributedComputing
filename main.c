#include "mpi.h"
#include "def.h"
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>

//how to run program
//mpicc main.c -o main
//mpiexec -np <JEDINUMBER> ./main
//mpirun -n 4 main

int randomValue(int left, int right){
    //losuje wartosc z przedzialu <left;right>
    return (rand() % (right + 1 - left) + left);
}
int assignToPost(){
    return randomValue(1,POSTNUMBER); //z przedzialu <1;POSTNUMBER>
}
int receiveDamage(){
    return randomValue(1,MAXDAMAGE);
}
void fight(int *hp){
    sleep(10); //co 3 sekundy sprawdza czy oberwal
    int chances = randomValue(1,10);
    if(chances<=5){
        *hp = *hp - receiveDamage();
    }
}

void criticalSection(int x, int *hp, int *postID){
    printf("%d: I am in critical section!\n",x);
    sleep(randomValue(1,HEALINGTIME));  //proces jest zatrzymywany na czas leczenia
    *hp = MAXHP;    //atrybut zdrowie jest maksymalizowany
    *postID = assignToPost();   //przypisanie nowego stanowiska
    printf("%d: I am leaving critical section!\n",x);
}

int main( int argc, char **argv )
{
    int seed;
    time_t tt;

	int rank,size,receiver,it,receivedCounter,requester, moment;
    int hp, postID;
  
    //TODO przydzial do losowego posterunku
	int msg[MSG_SIZE];
	MPI_Status status;
    int priorities[JEDINUMBER];
	MPI_Init(&argc, &argv);

	MPI_Comm_rank( MPI_COMM_WORLD, &rank );

    seed = time(&tt)+rank;
    srand(seed);
    postID = assignToPost();
    receivedCounter = 0;
    hp = MAXHP;
    printf("%d: receieved post %d\n",rank,postID);
    
    //TODO
    //Implementation of logical clock DONE
    //Local section DONE
    //priorytet na podstawie zdrowia i ranka (im mniejsze zdrowie tym wiekszy priorytet, potem rank)
    //Critical section in teleporter
    //czas leczenia musi byc logiczny
    //czas teleportacji musi byc logiczny
    //przy wyjsciu z sekcji krytycznej wysyla komunikaty z kolejnymi momentami od wejscia do wyjscia np. <7,9>
    //przesylanie hp w wiadomosciach
    //Wounds generator DONE
    //healingtime losowy DONE
    //bufor na przyszle wiadomosci -> lista jednokierunkowa

    //sending MSG_REQUEST to everyone
    moment = 0;
    while(1){
        fight(&hp);
        if(hp<MAXHP){
            printf("%d: I've been hit (moment %d)!\n",rank, moment);
            for(receiver=0;receiver<JEDINUMBER;receiver+=1){
                if(receiver==rank)
                    continue;
                msg[0] = rank;
                MPI_Comm_size( MPI_COMM_WORLD, &size );
                msg[1] = size;
                msg[2] = MSG_REQUEST;
                msg[3] = moment;
                printf("I %d: Sending MSG_REQUEST in moment %d to %d\n", rank, moment, receiver);
                MPI_Send( msg, MSG_SIZE, MPI_INT,receiver/*receiver*/, MSG_HELLO, MPI_COMM_WORLD );
            }
        

        //determine first process to enter critical section
        //main loop
            while(receivedCounter<(JEDINUMBER-LAZARETSPACE)){
                //sprawdz bufor (cala liste), jezeli natrafi na starsza wiadomosc to usuwa,
                //jezeli natrafi na wiadomosc z przyszlosci to zostawia
                //jezeli ma wiadomosc z tego momentu to dodaje receivedCounter i usuwa
                //jeszcze raz sprawdz warunek petli
                //if(recivedCounter>=(JEDINUMBER-LAZARETSPACE))
                //  break;
                odbieranie:
                MPI_Recv(msg, MSG_SIZE, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);                
                MPI_Get_count( &status, MPI_INT, &size);
                printf("I %d: Received %d values: (rank: %d message: %d moment: %d) from %d\n", rank, size, msg[0], msg[2], msg[3], status.MPI_SOURCE);
                //sprawdzanie momentu
                if(msg[3]<moment) {
                    printf("Ignore message\n");
                    goto odbieranie;
                }
                //przyszle wiadomosci
                if(msg[3]>moment){
                    //TODO        
                    //zapiszdoBufora()
                    //bufferChecked() <- wydajnosciowe
                }
                requester = msg[0];
                priorities[requester] = requester;
                if(msg[2] == MSG_REQUEST){ //if entry to critical section is requested
                    if(priorities[requester]>rank){
                        msg[0] = rank;
                        msg[2] = MSG_YES;
                        msg[3] = moment;
                        MPI_Comm_size(MPI_COMM_WORLD, &size);
                        msg[1] = size;
                        printf("I %d: Sending approval in moment %d to %d\n", rank, moment, requester);
                        MPI_Send( msg, MSG_SIZE, MPI_INT,requester/*requester*/, MSG_HELLO, MPI_COMM_WORLD );
                    }
                }
                else if(msg[2] == MSG_YES){
                    receivedCounter+=1;
                }
             }
        

        if(receivedCounter==JEDINUMBER-LAZARETSPACE) {
            criticalSection(rank, &hp, &postID); //enter critical section
            printf("%d: received post %d\n",rank,postID);
        }  
        //send approval after leaving the critical section
        }
        sleep(5);
        for(it=0;it<JEDINUMBER;it+=1){
            if(it==rank)
                continue;            

            msg[0] = rank;               
            msg[1] = size;    
            msg[2] = MSG_YES; 
            msg[3] = moment;
            MPI_Comm_size(MPI_COMM_WORLD, &size);
            printf("II %d:  Sending approval in moment %d to %d\n", rank, moment, it);
            MPI_Send( msg, MSG_SIZE, MPI_INT,it/*receiver*/, MSG_HELLO, MPI_COMM_WORLD );
        }
        moment = moment + 1;
    }

	MPI_Finalize();
    return 0;
}
