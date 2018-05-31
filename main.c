#include "mpi.h"
#include "def.h"
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>


//how to compile program
//mpicc main.c -o main
//how to run program
//mpiexec -np <JEDI_NUMBER> ./main
//mpirun -n 4 main


//definicja typu zlozonego message
typedef struct message {
    int sender;
    int size;
    int type;
    int moment;
    int hp;
    struct message *next;
} message_t;

//dodawanie do listy
void addToList(message_t **head, int sender, int size, int type, int moment, int hp) {
    message_t * new_node;
    new_node = malloc(sizeof(message_t));
    new_node->sender = sender;
    new_node->size = size;
    new_node->type = type;
    new_node->moment = moment;
    new_node->hp = hp;
    new_node->next = *head;
    *head = new_node;
}

//usuwanie z listy wg indeksu
void removeByIndex(message_t **head, int n) {
    int i = 0;
    message_t *current = *head;
    message_t *temp_node = NULL;
    message_t * next_node = NULL;
 
    if (n == 0) {
        next_node = (*head)->next;
        free(*head);
        *head = next_node;
    } else {
        for (i = 0; i < n-1; i++) {
            current = current->next;
        }
        temp_node = current->next;
        current->next = temp_node->next;
        free(temp_node);
    }
}

//wypiywanie listy
void printList(message_t **head) {
    message_t * current = *head;
    while (current != NULL) {
        printf("Type: %d, Sender: %d, Moment: %d Hp: %d \n", current->type, current->sender, current->moment, current->hp);
        current = current->next;
    }
}
 
//wybor elementu wg indeksu
message_t * getByIndex(message_t **head, int n) {
    int i = 0;
    message_t * current = *head;
    if(n > 0) {
        for (i = 0; i < n-1; i++) {
            current = current->next;
        }
    }
    return current;
}

//losowanie wartosci z przedzialu <left;right>
int randomValue(int left, int right){
    return (rand() % (right + 1 - left) + left);
}

//przypisywanie do posterunku
int assignToPost(){
    return randomValue(1,POST_NUMBER); //z przedzialu <1;POST_NUMBER>
}

//generowanie obrazen
int receiveDamage(){
    return randomValue(1,MAX_DAMAGE);
}

//walka, sekcja lokalna
void fight(int *hp){
    sleep(5); //spowolnienie programu o 5 sek
    int chances = randomValue(1,10);
    if(chances<=5){
        *hp = *hp - receiveDamage();
    }
}

//sekcja krytyczna
int criticalSection(int x, int *hp, int *postID, int *moment){
    int time;
    printf("%d: entering critical section!\n",x);
    time = randomValue(MIN_HEAL_TIME,MAX_HEAL_TIME);
    sleep(time);    //proces jest zatrzymywany na czas leczenia 
    *hp = MAX_HP;    //atrybut zdrowie jest maksymalizowany
    *postID = assignToPost();   //przypisanie nowego stanowiska
    *moment = *moment +time;
    printf("%d: leaving critical section!\n",x);
    return time;
}

int main( int argc, char **argv )
{
    int seed;
    time_t tt;

	int rank,size,receiver,it,receivedCounter,requester, moment, elementCount, i, skipReceiving;
    int hp, postID;
  
	int msg[MSG_SIZE];
	MPI_Status status;
    int priorities[JEDI_NUMBER];
    int healthpoints[JEDI_NUMBER];
	MPI_Init(&argc, &argv);

	MPI_Comm_rank( MPI_COMM_WORLD, &rank );

    message_t *head = NULL;
    seed = time(&tt)+rank;
    srand(seed);
    receivedCounter = 0;
    elementCount = 0;
    hp = MAX_HP;
    postID = assignToPost();
    printf("%d: receieved post %d\n",rank,postID);
    
    /* 
    *TODO
    -czas teleportacji musi byc logiczny
    *DONE
    -Implementation of logical clock DONE
    -bufor na przyszle wiadomosci -> lista jednokierunkowa DONE
    -Local section DONE
    -przesylanie hp w wiadomosciach DONE
    -priorytet na podstawie zdrowia i ranka (im mniejsze zdrowie tym wiekszy priorytet, potem rank) DONE
    -Wounds generator DONE
    -healingtime losowy DONE
    -czas leczenia musi byc logiczny DONE
    -przy wyjsciu z sekcji krytycznej wysyla komunikaty z kolejnymi momentami od wejscia do wyjscia np. <7,9> DONE
    */

    moment = 0;
    while(1){
        fight(&hp); //sekcja lokalna
        /* jezeli  jedi zostal ranny to wypisuje rank, hp i moment oraz wysyla do wszystkich
           pozostalych rycerzy zadanie dostepu do sekcji krytycznej */
        if(hp<MAX_HP){
            printf("%d: I've been hit, hp: %d, moment: %d!\n",rank, hp, moment); 
            for(receiver=0;receiver<JEDI_NUMBER;receiver+=1){ 
                if(receiver==rank)                           
                    continue;
                msg[0] = rank;
                msg[2] = MSG_REQUEST;
                msg[3] = moment;
                msg[4] = hp;
                MPI_Comm_size( MPI_COMM_WORLD, &size );
                msg[1] = size;
                printf("%d: Sent MSG_REQUEST, moment: %d to %d\n", rank, moment, receiver);
                MPI_Send( msg, MSG_SIZE, MPI_INT,receiver/*receiver*/, MSG_HELLO, MPI_COMM_WORLD );
            }
        
    
            /* ustalanie pierszenstwa w dostepie do sekcji krytycznej
            tak dlugo jak nie dostal pozwolenia na wejscie od
            liczby rycerzy pomniejszonej o miejsca w lazarecie czeka na pozwolenie  */
            while(receivedCounter<(JEDI_NUMBER-LAZARET_SPACE)){
                /* sprawdzanie bufora wiadomosci odlozonych na pozniej:
                   -jezeli napotka na starsze wiadomosci niz jego aktualny moment, to je usuwa
                    (usuwa je poniewaz, wysylal wczesniej potwierdzenie dla wszystkich, wiec sa
                    nieaktualne)
                   -jezeli napotka na wiadomosc z biezacego momentu to:
                     a)jezeli wiadomosc jest zgoda to zwieksza licznik odebranych zgod
                       i usuwa wiadomosc z listy                      
                     b)jezeli wiadomosc jest zadaniem to oznacza wiadomosc jako biezaca, 
                       usuwa ja z listy, ustawia zmienna skipReceiving na 1
                       (w celu pominiecia odbierania wiadomosci)
                       i przechodzi do uzgadniania pierszenstwa wejscia
                   -jezeli napotka na wiadomosc z przyszlosci, to zostawia ja w buforze
                */          
                if(head == NULL){ }
                else {
                    message_t *tmp;
                    for(i=0;i<elementCount;i=i+1){
                        tmp = getByIndex(&head, i);
                        if(tmp->moment<moment){ //jezeli natrafi na starsza wiadomosc to usuwa,
                            removeByIndex(&head,i);
                            i = i-1;
                            elementCount = elementCount - 1;
                        }
                        else if(tmp->moment == moment){//jezeli ma wiadomosc z tego momentu
                            if(tmp->type == MSG_YES){//i jest zgoda to zwieksza receivedCounter i usuwa
                                receivedCounter = receivedCounter + 1;
                                removeByIndex(&head, i);
                                i = i-1;
                                elementCount = elementCount - 1;
                            }
                            if(tmp->type == MSG_REQUEST){
                                skipReceiving = 1;
                                msg[0] = tmp->sender;
                                msg[2] = tmp->type;
                                msg[3] = tmp->moment;
                                msg[4] = tmp->hp;
                            }
                        }
                        //jezeli natrafi na wiadomosc z przyszlosci to zostawia
                    }
                }
                //jeszcze raz sprawdzony zostaje warunek petli ze wzgledu na przejrzane wiadomosci w buforze
                if(receivedCounter>=(JEDI_NUMBER-LAZARET_SPACE)){
                    break;
                }      

                /* odbieranie wiadomosci, 
                   jezeli ze wzgledu na przejscie w buforze
                   zostala ustawiona zmienna skipReceiving, to pomijamy odbieranie,
                   w przeciwnym wypadku odbieramy wiadomosci,
                   jezeli odebrana wiadomosc pochodzi z przeszlosci to ja ignorujemy,
                   jezeli odebrana wiadomosc pochodzi z biezacego momentu to przechodzimy do ustalania priorytetow
                   jezeli odebrana wiadomosc pochodzi z przyszlosci to zapisujemy ja do bufora,
                */        
                if(skipReceiving==0){
                    do{
                        MPI_Recv(msg, MSG_SIZE, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);                
                        MPI_Get_count( &status, MPI_INT, &size);
                        printf("<in queue> %d: Received %d values: (rank: %d message: %d moment: %d) from %d\n", rank, size, msg[0], msg[2], msg[3], status.MPI_SOURCE);
                        //sprawdzanie momentu wyslania wiadomosci
                        if(msg[3]<moment){ //wiadomosci z przeszlosci sa ignorowane
                            printf("Ignore message\n");
                        }
                        //przyszle wiadomosci
                        if(msg[3]>moment){ //wiadomosci z przyszlosci sa zapisywane do bufora
                            addToList(&head,msg[0],msg[1],msg[2],msg[3],msg[4]); 
                            elementCount = elementCount +1;
                        }
                    }while(msg[3]!=moment);
                }
                else
                    skipReceiving=0;

                requester = msg[0];
                priorities[requester] = requester;
                healthpoints[requester] = msg[4];

                /* -jezeli odebrana wiadomosc jest zadaniem, nastepuje ustalenie pierszenstwa wejscia 
                    do lazaretu przez porownanie zdrowia rycerzy, a jezeli bylby taki sam, to przez
                    zmienna rank (przybycie rycerza na pole bitwy)
                   -jezeli odebrana wiadomosc jest zgoda, to nastepuje zwiekszenie licznika zgod
                    (zmienna receivedCounter) */                 
                if(msg[2] == MSG_REQUEST){ //if entry to critical section is requested
                    if(healthpoints[requester]<hp){ //check who is more wounded
                        msg[0] = rank;
                        msg[2] = MSG_YES;
                        msg[3] = moment;
                        msg[4] = hp;
                        MPI_Comm_size(MPI_COMM_WORLD, &size);
                        msg[1] = size;
                        printf("<in queue> %d: Sent approval to %d, moment: %d\n", rank, requester, moment);
                        MPI_Send( msg, MSG_SIZE, MPI_INT,requester/*requester*/, MSG_HELLO, MPI_COMM_WORLD );
                    }
                    else if(healthpoints[requester]==hp){ //if wounds are equal then
                    printf("healthpoints[requester]: %d, hp: %d\n",healthpoints[requester],hp);
                        if(priorities[requester]>rank){ //check rank 
                            msg[0] = rank;
                            msg[2] = MSG_YES;
                            msg[3] = moment;
                            msg[4] = hp;
                            MPI_Comm_size(MPI_COMM_WORLD, &size);
                            msg[1] = size;
                            printf("<in queuee> %d: Sent approval to %d, moment: %d\n", rank, requester, moment);
                            MPI_Send( msg, MSG_SIZE, MPI_INT,requester/*requester*/, MSG_HELLO, MPI_COMM_WORLD );
                        }
                    }
                }
                else if(msg[2] == MSG_YES){
                    receivedCounter+=1;
                }
             }
        
        /*jezeli liczba odebranych zgod jest wystarczajaca do wejscia do sekcji krytycznej  
          nastepuje wejscie do sekcji krytycznej
          spedzamy tam czas (zmienna moment zostaje zwiekszona o wartosc z przedzialu
          <MIN_HEAL_TIME;MAX_HEAL_TIME>
          do wszystkich jedi wyslana zostaje zgoda na wejscie
          w zajetych wczesniej momemntach oprocz momentu wyjscia
        */
        if(receivedCounter==JEDI_NUMBER-LAZARET_SPACE) {
            int momentsPassed, ite;
            momentsPassed = criticalSection(rank, &hp, &postID, &moment); //enter critical section
            for(ite=moment-momentsPassed;ite<momentsPassed+moment;ite=ite+1){
                for(it=0;it<JEDI_NUMBER;it+=1){
                    if(it==rank)
                        continue;            
                    msg[0] = rank;    
                    msg[2] = MSG_YES; 
                    msg[3] = ite;
                    msg[4] = hp;
                    MPI_Comm_size(MPI_COMM_WORLD, &size);                         
                    msg[1] = size;  
                    printf("%d: Sent approval to %d, moment: %d\n", rank, it, ite);
                    MPI_Send( msg, MSG_SIZE, MPI_INT,it/*receiver*/, MSG_HELLO, MPI_COMM_WORLD );
                }
            }

            printf("%d: received post %d\n",rank,postID);
        }
        }
        //do kazdego rycerza zostaje wyslana zgoda na wejscie
        for(it=0;it<JEDI_NUMBER;it+=1){
            if(it==rank)
                continue;            

            msg[0] = rank;    
            msg[2] = MSG_YES; 
            msg[3] = moment;
            msg[4] = hp;
            MPI_Comm_size(MPI_COMM_WORLD, &size);                         
            msg[1] = size;  
            printf("%d: Sent approval to %d, moment: %d\n", rank, it, moment);
            MPI_Send( msg, MSG_SIZE, MPI_INT,it/*receiver*/, MSG_HELLO, MPI_COMM_WORLD );
        }
        //nastepuje inkrementacja momentu
        moment = moment + 1;
        sleep(1); //spowolnienie procesu na 1s(1 moment)
    }

	MPI_Finalize();
    return 0;
}
