
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <string>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <cstdlib> 
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>

using namespace std;

#define ROOM_COUNT 			256
#define TCP_PORT			20001

#define CMD_MOVE 			0
#define CMD_SIGN_IN			1
#define CMD_SIGN_OUT		2
#define CMD_RESIGN 			3
#define CMD_REFRESH 		4
#define CMD_ERR				254
#define CMD_RESPONSE		255

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
 
struct client_t
{
    uint16_t client_id;
    int tcp_socket;
};

uint16_t rooms[ROOM_COUNT][3];
uint8_t plansze[ROOM_COUNT][64];
int resignation[ROOM_COUNT*2]; //tablica informująca czy gracz o danym id ma prawo rezygnacji z ruchu
int sockets[ROOM_COUNT*2];
pthread_t threads[ROOM_COUNT * 2];
int initialize_ind[64] = {1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 3, 0, 3, 0, 3, 3, 0, 3, 0, 3, 0, 3, 0, 0, 3, 0, 3, 0, 3, 0, 3};

struct msg_t
{
	uint8_t cmd;					//polecenie: 0 - wykonaj ruch, 1 - zapisz do pokoju, 2 - wypisz z pokoju, 3 - zrezygnuj z ruchu, 4 - odświeżenie szachownicy, 254 - błąd, 255 - odpowiedź
	uint8_t room_id;				//numer pokoju, np. 53
	uint16_t client_id;				//numer klienta, np. 106
	char move_from[2];				//pole początkowe, np. "C3"
	char move_to[2];				//pole końcowe, np. "H7"
};

//zapisywanie klienta do pokoju, tworzenie plansz
int find_next_client_id()
{
    int i;
    for(i = 0; i < ROOM_COUNT; i++)
    {
        //jeśli pierwsza kolumna jest wolna
        if(rooms[i][0] == 0xffff)
        {
            rooms[i][0] = i * 2;
			rooms[i][2] = 0;
			for(int j = 0; j < 64; j++){
				plansze[i][j] = initialize_ind[j];
			}
            return i * 2;
        }
        else
        {
            //jeśli druga kolumna jest wolna
            if(rooms[i][1] == 0xffff)
            {
                rooms[i][1] = i * 2 + 1;
				rooms[i][2] = 0;
				for(int j = 0; j < 64; j++){
					plansze[i][j] = initialize_ind[j];
				}
                return i * 2 + 1;
            }
        }
    }
    return -1;
}

//sprawdzenie czy ruch damki jest prawidłowy
int ifvalid_king(char move_f[2], char move_k[2], uint16_t c_id){
	int index = (int(move_k[0]) - 65) + (int(move_k[1]) - 49)*8;
	int index2 = (int(move_f[0]) - 65) + (int(move_f[1]) - 49)*8;
	//sprawdzenie czy ruch nie wychodzi poza planszę i czy gracz nie chce poruszyć damką przeciwnika.
	if((int(move_k[0]) < 65 || int(move_k[0]) > 72) || (int(move_k[1]) < 49 || move_k[1] > 56)){
		return 0;
	}
	if((c_id % 2 == 0 && plansze[c_id/2][index2] == 4) || (c_id % 2 == 1 && plansze[c_id/2][index2] == 2) || plansze[c_id/2][index] != 0){
		return 0;
	}
	if(abs(move_f[0] - move_k[0]) != abs(move_f[1] - move_k[1])){
		return 0;
	}
	bool beat = false;
	int pom;
	//dopasowanie ruchu i sprawdzenie jego poprawności
	if((index - index2) > 0){
		if((index - index2) % 9 == 0){
			for(int i = index-9; i > index2; i = i - 9){
				if((c_id % 2 == 0 && plansze[c_id/2][i] == 3) || (c_id % 2 == 0 && plansze[c_id/2][i] == 4) || (c_id % 2 == 1 && plansze[c_id/2][i] == 1) || (c_id % 2 == 1 && plansze[c_id/2][i] == 2)){
					if(beat == false){
						beat = true;
						pom = i;
					}
					else{
						return 0;
					}
				}
				if((c_id % 2 == 0 && plansze[c_id/2][i] == 1) || (c_id % 2 == 0 && plansze[c_id/2][i] == 2) || (c_id % 2 == 1 && plansze[c_id/2][i] == 3) || (c_id % 2 == 1 && plansze[c_id/2][i] == 4)){
					return 0;
				}
			}
			if(beat == true){
				if(c_id % 2 == 0){
					plansze[c_id/2][index] = 2;
				}
				else{
					plansze[c_id/2][index] = 4;
				}
				plansze[c_id/2][index2] = 0;
				plansze[c_id/2][pom] = 0;
				return 2;
			}
			else{
				if(resignation[c_id] == -1){
					if(c_id % 2 == 0){
						plansze[c_id/2][index] = 2;
					}
					else{
						plansze[c_id/2][index] = 4;
					}
					plansze[c_id/2][index2] = 0;
					return 1;
				}
				else{
					return 0;
				}
			}
		}
		else{
			for(int i = index-7; i > index2; i = i - 7){
				if((c_id % 2 == 0 && plansze[c_id/2][i] == 3) || (c_id % 2 == 0 && plansze[c_id/2][i] == 4) || (c_id % 2 == 1 && plansze[c_id/2][i] == 1) || (c_id % 2 == 1 && plansze[c_id/2][i] == 2)){
					if(beat == false){
						beat = true;
						pom = i;
					}
					else{
						return 0;
					}
				}
				if((c_id % 2 == 0 && plansze[c_id/2][i] == 1) || (c_id % 2 == 0 && plansze[c_id/2][i] == 2) || (c_id % 2 == 1 && plansze[c_id/2][i] == 3) || (c_id % 2 == 1 && plansze[c_id/2][i] == 4)){
					return 0;
				}
			}
			if(beat == true){
				if(c_id % 2 == 0){
					plansze[c_id/2][index] = 2;
				}
				else{
					plansze[c_id/2][index] = 4;
				}
				plansze[c_id/2][index2] = 0;
				plansze[c_id/2][pom] = 0;
				return 2;
			}
			else{
				if(resignation[c_id] == -1){
					if(c_id % 2 == 0){
						plansze[c_id/2][index] = 2;
					}
					else{
						plansze[c_id/2][index] = 4;
					}
					plansze[c_id/2][index2] = 0;
					return 1;
				}
				else{
					return 0;
				}
			}
		}
	}
	else{
		if((index - index2) % 9 == 0){
			for(int i = index+9; i < index2; i = i + 9){
				if((c_id % 2 == 0 && plansze[c_id/2][i] == 3) || (c_id % 2 == 0 && plansze[c_id/2][i] == 4) || (c_id % 2 == 1 && plansze[c_id/2][i] == 1) || (c_id % 2 == 1 && plansze[c_id/2][i] == 2)){
					if(beat == false){
						beat = true;
						pom = i;
					}
					else{
						return 0;
					}
				}
				if((c_id % 2 == 0 && plansze[c_id/2][i] == 1) || (c_id % 2 == 0 && plansze[c_id/2][i] == 2) || (c_id % 2 == 1 && plansze[c_id/2][i] == 3) || (c_id % 2 == 1 && plansze[c_id/2][i] == 4)){
					return 0;
				}
			}
			if(beat == true){
				if(c_id % 2 == 0){
					plansze[c_id/2][index] = 2;
				}
				else{
					plansze[c_id/2][index] = 4;
				}
				plansze[c_id/2][index2] = 0;
				plansze[c_id/2][pom] = 0;
				return 2;
			}
			else{
				if(resignation[c_id] == -1){
					if(c_id % 2 == 0){
						plansze[c_id/2][index] = 2;
					}
					else{
						plansze[c_id/2][index] = 4;
					}
					plansze[c_id/2][index2] = 0;
					return 1;
				}
				else{
					return 0;
				}
			}
		}
		else{
			for(int i = index+7; i < index2; i = i + 7){
				if((c_id % 2 == 0 && plansze[c_id/2][i] == 3) || (c_id % 2 == 0 && plansze[c_id/2][i] == 4) || (c_id % 2 == 1 && plansze[c_id/2][i] == 1) || (c_id % 2 == 1 && plansze[c_id/2][i] == 2)){
					if(beat == false){
						beat = true;
						pom = i;
					}
					else{
						return 0;
					}
				}
				if((c_id % 2 == 0 && plansze[c_id/2][i] == 1) || (c_id % 2 == 0 && plansze[c_id/2][i] == 2) || (c_id % 2 == 1 && plansze[c_id/2][i] == 3) || (c_id % 2 == 1 && plansze[c_id/2][i] == 4)){
					return 0;
				}
			}
			if(beat == true){
				if(c_id % 2 == 0){
					plansze[c_id/2][index] = 2;
				}
				else{
					plansze[c_id/2][index] = 4;
				}
				plansze[c_id/2][index2] = 0;
				plansze[c_id/2][pom] = 0;
				return 2;
			}
			else{
				if(resignation[c_id] == -1){
					if(c_id % 2 == 0){
						plansze[c_id/2][index] = 2;
					}
					else{
						plansze[c_id/2][index] = 4;
					}
					plansze[c_id/2][index2] = 0;
					return 1;
				}
				else{
					return 0;
				}
			}
		}
	}
}

//sprawdzenie czy ruch piona jest prawidłowy
int ifvalid(char move_f[2], char move_k[2], uint16_t c_id, uint8_t figure, uint8_t plansza[64]){
	int index = (int(move_k[0]) - 65) + (int(move_k[1]) - 49)*8, index3;
	int index2 = (int(move_f[0]) - 65) + (int(move_f[1]) - 49)*8;
	//sprawdzenie czy ruch nie wychodzi poza planszę i czy gracz nie chce poruszyć damką przeciwnika.
	if((int(move_k[0]) < 65 || int(move_k[0]) > 72) || (int(move_k[1]) < 49 || move_k[1] > 56)){
		return 0;
	}
	if((c_id % 2 == 0 && figure == 3) || (c_id % 2 == 1 && figure == 1)){
		return 0;
	}
	//sprawdzenie czy pole, na które wykonywany jest ruch jest puste i czy ruch jest biciem.
	if(plansza[index] == 0){
		if((abs(int(move_f[0]) - int(move_k[0])) == 1) && ((figure == 1 && (int(move_f[1]) - int(move_k[1])) == -1) || (figure == 3 && (int(move_f[1]) - int(move_k[1])) == 1))){
			if(plansze[c_id/2][index2] == 1){
				plansze[c_id/2][index2] = 0;
				plansze[c_id/2][index] = 1;
			}
			else{
				plansze[c_id/2][index2] = 0;
				plansze[c_id/2][index] = 3;
			}
			if(resignation[c_id] == -1){
				return 1;
			}
			else{
				return 0;
			}
		}
		if((int(move_f[0]) - int(move_k[0]) == 2) && (int(move_f[1]) - int(move_k[1]) == 2)){
			index3 = index2 - 9;
			if((plansza[index3] == 1 && c_id % 2 == 1) || (plansza[index3] == 2 && c_id % 2 == 1) || (plansza[index3] == 3 && c_id % 2 == 0) || (plansza[index3] == 4 && c_id % 2 == 0)){
				if(plansze[c_id/2][index2] == 1){
					plansze[c_id/2][index2] = 0;
					plansze[c_id/2][index] = 1;
					plansze[c_id/2][index3] = 0;
				}
				else{
					plansze[c_id/2][index2] = 0;
					plansze[c_id/2][index] = 3;
					plansze[c_id/2][index3] = 0;
				}
				return 2;
			}
			else{
				return 0;
			}
		}
		if((int(move_f[0]) - int(move_k[0]) == 2) && (int(move_f[1]) - int(move_k[1]) == -2)){
			index3 = index2 + 7;
			if((plansza[index3] == 1 && c_id % 2 == 1) || (plansza[index3] == 2 && c_id % 2 == 1) || (plansza[index3] == 3 && c_id % 2 == 0) || (plansza[index3] == 4 && c_id % 2 == 0)){
				if(plansze[c_id/2][index2] == 1){
					plansze[c_id/2][index2] = 0;
					plansze[c_id/2][index] = 1;
					plansze[c_id/2][index3] = 0;
				}
				else{
					plansze[c_id/2][index2] = 0;
					plansze[c_id/2][index] = 3;
					plansze[c_id/2][index3] = 0;
				}
				return 2;
			}
			else{
				return 0;
			}
		}
		if((int(move_f[0]) - int(move_k[0]) == -2) && (int(move_f[1]) - int(move_k[1]) == -2)){
			index3 = index2 + 9;
			if((plansza[index3] == 1 && c_id % 2 == 1) || (plansza[index3] == 2 && c_id % 2 == 1) || (plansza[index3] == 3 && c_id % 2 == 0) || (plansza[index3] == 4 && c_id % 2 == 0)){
				if(plansze[c_id/2][index2] == 1){
					plansze[c_id/2][index2] = 0;
					plansze[c_id/2][index] = 1;
					plansze[c_id/2][index3] = 0;
				}
				else{
					plansze[c_id/2][index2] = 0;
					plansze[c_id/2][index] = 3;
					plansze[c_id/2][index3] = 0;
				}
				return 2;
			}
			else{
				return 0;
			}
		}
		if((int(move_f[0]) - int(move_k[0]) == -2) && (int(move_f[1]) - int(move_k[1]) == 2)){
			index3 = index2 - 7;
			if((plansza[index3] == 1 && c_id % 2 == 1) || (plansza[index3] == 2 && c_id % 2 == 1) || (plansza[index3] == 3 && c_id % 2 == 0) || (plansza[index3] == 4 && c_id % 2 == 0)){
				if(plansze[c_id/2][index2] == 1){
					plansze[c_id/2][index2] = 0;
					plansze[c_id/2][index] = 1;
					plansze[c_id/2][index3] = 0;
				}
				else{
					plansze[c_id/2][index2] = 0;
					plansze[c_id/2][index] = 3;
					plansze[c_id/2][index3] = 0;
				}
				return 2;
			}
			else{
				return 0;
			}
		}
		else{
			return 0;
		}
	}
	return 0;
}

//sprawdzenie czy pion  może zbić figurę przeciwnika
bool check_move(int ind, uint16_t c_id, uint8_t plansza[64]){
	int ind2 = ind - 9, ind3 = ind - 7, ind4 = ind + 7, ind5 = ind + 9, ind6 = ind - 18, ind7 = ind - 14, ind8 = ind + 14, ind9 = ind + 18;
	int number = ind/8;
	int letter = ind % 8;
	//sprawdzenie wszystkich wariantów ruchu
	if(c_id % 2 == 0){
		if(number - 2 >= 0 && letter - 2 >= 0){
			if((plansza[ind2] == 3 || plansza[ind2] == 4) && plansza[ind6] == 0){
				return true;
			}
		}
		if(number - 2 >= 0 && letter + 2 <= 7){
			if((plansza[ind3] == 3 || plansza[ind3] == 4) && plansza[ind7] == 0){
				return true;
			}
		}
		if(number + 2 <= 7 && letter - 2 >= 0){
			if((plansza[ind4] == 3 || plansza[ind4] == 4) && plansza[ind8] == 0){
				return true;
			}
		}
		if(number + 2 <= 7 && letter + 2 <= 7){
			if((plansza[ind5] == 3 || plansza[ind5] == 4) && plansza[ind9] == 0){
				return true;
			}
		}		
	}
	else{
		if(number - 2 >= 0 && letter - 2 >= 0){
			if((plansza[ind2] == 1 || plansza[ind2] == 2) && plansza[ind6] == 0){
				return true;
			}
		}
		if(number - 2 >= 0 && letter + 2 <= 7){
			if((plansza[ind3] == 1 || plansza[ind3] == 2) && plansza[ind7] == 0){
				return true;
			}
		}
		if(number + 2 <= 7 && letter - 2 >= 0){
			if((plansza[ind4] == 1 || plansza[ind4] == 2) && plansza[ind8] == 0){
				return true;
			}
		}
		if(number + 2 <= 7 && letter + 2 <= 7){
			if((plansza[ind5] == 1 || plansza[ind5] == 2) && plansza[ind9] == 0){
				return true;
			}
		}	
	}
	return false;
}


//sprawdzenie czy damka może zbić figurę przeciwnika
bool check_king_move(int ind, uint16_t c_id){
	bool beat = false;
	bool empty = false;
	int number = ind/8;
	int letter = ind % 8;
	int counter = 1;
	//sprawdzenie wszystkich wariantów ruchu
	for(int i = ind + 9; i < 64; i = i + 9){
			if(number + counter <= 7 && letter + counter <= 7){
				if((plansze[c_id/2][i] == 1 && c_id % 2 == 0) || (plansze[c_id/2][i] == 2 && c_id % 2 == 0) || (plansze[c_id/2][i] == 3 && c_id % 2 == 1) || (plansze[c_id/2][i] == 4 && c_id % 2 == 1)){
					break;
				}
				if((plansze[c_id/2][i] == 3 && c_id % 2 == 0) || (plansze[c_id/2][i] == 4 && c_id % 2 == 0) || (plansze[c_id/2][i] == 1 && c_id % 2 == 1) || (plansze[c_id/2][i] == 2 && c_id % 2 == 1)){
					if(beat == true){
						break;
					}
					beat = true;
					empty = false;
				}
				if(plansze[c_id/2][i] == 0){
					empty = true;
				}
				if(beat == true && empty == true){
					return true;
				}
			}
			counter++;
	}
	empty = false;
	beat = false;
	counter = 1;
	for(int i = ind + 7; i < 64; i = i + 7){
		if(number + counter <= 7 && letter - counter >= 0){
			if((plansze[c_id/2][i] == 1 && c_id % 2 == 0) || (plansze[c_id/2][i] == 2 && c_id % 2 == 0) || (plansze[c_id/2][i] == 3 && c_id % 2 == 1) || (plansze[c_id/2][i] == 4 && c_id % 2 == 1)){
				break;
			}
			if((plansze[c_id/2][i] == 3 && c_id % 2 == 0) || (plansze[c_id/2][i] == 4 && c_id % 2 == 0) || (plansze[c_id/2][i] == 1 && c_id % 2 == 1) || (plansze[c_id/2][i] == 2 && c_id % 2 == 1)){
				if(beat == true){
					break;
				}
				beat = true;
				empty = false;
			}
			if(plansze[c_id/2][i] == 0){
				empty = true;
			}
			if(beat == true && empty == true){
				return true;
			}
		}
		counter++;
	}
	empty = false;
	beat = false;
	counter = 1;
	for(int i = ind - 7; i >= 0; i = i - 7){
		if(number - counter >= 0 && letter + counter <= 7){
			if((plansze[c_id/2][i] == 1 && c_id % 2 == 0) || (plansze[c_id/2][i] == 2 && c_id % 2 == 0) || (plansze[c_id/2][i] == 3 && c_id % 2 == 1) || (plansze[c_id/2][i] == 4 && c_id % 2 == 1)){
				break;
			}
			if((plansze[c_id/2][i] == 3 && c_id % 2 == 0) || (plansze[c_id/2][i] == 4 && c_id % 2 == 0) || (plansze[c_id/2][i] == 1 && c_id % 2 == 1) || (plansze[c_id/2][i] == 2 && c_id % 2 == 1)){
				if(beat == true){
					break;
				}
				beat = true;
				empty = false;
			}
			if(plansze[c_id/2][i] == 0){
				empty = true;
			}
			if(beat == true && empty == true){
				return true;
			}
		}
		counter++;
	}
	empty = false;
	beat = false;
	counter = 1;
	for(int i = ind - 9; i >= 0; i = i - 9){
		if(number - counter >= 0 && letter - counter >= 0){
			if((plansze[c_id/2][i] == 1 && c_id % 2 == 0) || (plansze[c_id/2][i] == 2 && c_id % 2 == 0) || (plansze[c_id/2][i] == 3 && c_id % 2 == 1) || (plansze[c_id/2][i] == 4 && c_id % 2 == 1)){
				break;
			}
			if((plansze[c_id/2][i] == 3 && c_id % 2 == 0) || (plansze[c_id/2][i] == 4 && c_id % 2 == 0) || (plansze[c_id/2][i] == 1 && c_id % 2 == 1) || (plansze[c_id/2][i] == 2 && c_id % 2 == 1)){
				if(beat == true){
					break;
				}
				beat = true;
				empty = false;
			}
			if(plansze[c_id/2][i] == 0){
				empty = true;
			}
			if(beat == true && empty == true){
				return true;
			}
		}
		counter ++;
	}
	return false;
}


//sprawdzenie czy gracz ma obowiązek bicia
int check_all(uint16_t c_id){
	for(int i = 0; i < 64; i++){
		if(c_id % 2 == 0){
			if((plansze[c_id/2][i] == 1 && check_move(i, c_id, plansze[c_id/2]) == true) || (plansze[c_id/2][i] == 2 && check_king_move(i, c_id) == true)){
				return i;
			}
		}
		else{
			if((plansze[c_id/2][i] == 3 && check_move(i, c_id, plansze[c_id/2]) == true) || (plansze[c_id/2][i] == 4 && check_king_move(i, c_id) == true)){
				return i;
			}
		}
	}
	return -1;
}

//sprawdzenie czy którykolwiek pion/damka danego gracza może się ruszyć
bool check_all_moves(uint16_t c_id){
	int number;
	int letter;
	for(int i = 0; i < 64; i++){
		number = i/8;
		letter = i % 8;
		if(c_id % 2 == 0){
			if(number + 1 <= 7 && letter - 1 >= 0 && plansze[c_id/2][i + 7] == 0){
				return true;
			}
			if(number + 1 <= 7 && letter + 1 <= 7 && plansze[c_id/2][i + 9] == 0){
				return true;
			}
		}
		else{
			if(number - 1 >= 0 && letter + 1 <= 7 && plansze[c_id/2][i - 7] == 0){
				return true;
			}
			if(number - 1 >= 0 && letter - 1 >= 0 && plansze[c_id/2][i - 9] == 0){
				return true;
			}
		}
	}
	return false;
}


//sprawdzenie warunków końca gry
bool check_end(uint16_t c_id){
	int ones=0, twos=0, threes=0, fours=0;
	//sprawdzenie czy któryś z graczy stracił wszystkie figury.
	for(int i = 0; i < 64; i++){
		switch(plansze[c_id/2][i])
		{
			case 1:
				ones++;
				break;
			case 2:
				twos++;
				break;
			case 3:
				threes++;
				break;
			case 4:
				fours++;
				break;
			default:
				break;
		}
	}
	if(((ones + twos) == 0) || ((threes + fours) == 0)){
		return true;
	}
	//sprawdzenie blokady
	if(check_all(c_id) != -1){
		return false;
	}
	if(check_all_moves(c_id) == true){
		return false;
	}
	else{
		return true;
	}
}


//wątek gracza
void *client_th(void *arg)
{
    client_t* client = (client_t*)arg;
	sockets[client->client_id] = client->tcp_socket;
	int tcp_socket = client->tcp_socket;
    uint8_t buf[sizeof(msg_t)];
	msg_t response = {
		.cmd = 255,
		.room_id = (uint8_t)(client->client_id / 2),
		.client_id = client->client_id,
		.move_from = {'-', '-'},
		.move_to =  {'-', '-'}
	};
	write(tcp_socket, &response, sizeof(msg_t));
    while(1)
    {
        if(recv(tcp_socket, &buf, sizeof(msg_t), 0) != sizeof(msg_t)) continue;
        pthread_mutex_lock(&mutex);
		msg_t response = {
			.cmd = CMD_RESPONSE,
			.room_id = ((msg_t*)buf)->room_id,
			.client_id = ((msg_t*)buf)->client_id,
			.move_from = {((msg_t*)buf)->move_from[0], ((msg_t*)buf)->move_from[1]},
			.move_to = {((msg_t*)buf)->move_to[0], ((msg_t*)buf)->move_to[1] }
		};
		switch (((msg_t*)buf)->cmd)
			{
				//wykonanie ruchu lub wykrycie błędnego ruchu
				case CMD_MOVE:
					{
						if(int(((msg_t*)buf)->move_from[0]) > 90){
							((msg_t*)buf)->move_from[0] = ((msg_t*)buf)->move_from[0] - ' ';
						}
						if(int(((msg_t*)buf)->move_to[0]) > 90){
							((msg_t*)buf)->move_to[0] = ((msg_t*)buf)->move_to[0] - ' ';
						}
						int beat = check_all(((msg_t*)buf)->client_id);
						int move;
						bool endgame = check_end(((msg_t*)buf)->client_id);
						int index = (int(((msg_t*)buf)->move_from[0]) - 65) + (int(((msg_t*)buf)->move_from[1]) - 49)*8;
						int index2 = (int(((msg_t*)buf)->move_to[0]) - 65) + (int(((msg_t*)buf)->move_to[1]) - 49)*8;
						if((rooms[((msg_t*)buf)->room_id][2] == ((msg_t*)buf)->client_id % 2) && endgame == false){
							if(resignation[((msg_t*)buf)->client_id] == index || resignation[((msg_t*)buf)->client_id] == -1){
								if(plansze[((msg_t*)buf)->room_id][index] == 1 ||plansze[((msg_t*)buf)->room_id][index] == 3 ){
									move = ifvalid(((msg_t*)buf)->move_from, ((msg_t*)buf)->move_to, ((msg_t*)buf)->client_id, plansze[((msg_t*)buf)->room_id][index], plansze[((msg_t*)buf)->room_id]);
								}
								else{
									move = ifvalid_king(((msg_t*)buf)->move_from, ((msg_t*)buf)->move_to, ((msg_t*)buf)->client_id);
								}
								if(move == 1 && beat != -1){
									plansze[((msg_t*)buf)->room_id][beat] = 0;
								}
								if(move == 2){
									if((((msg_t*)buf)->client_id % 2 == 0 && index2 > 55) || (((msg_t*)buf)->client_id % 2 == 1 && index2 < 8)){
										if(plansze[((msg_t*)buf)->room_id][index2] == 1 || plansze[((msg_t*)buf)->room_id][index2] == 3){
											plansze[((msg_t*)buf)->room_id][index2]++; 
										}
										if(((msg_t*)buf)->client_id % 2 == 0){
											rooms[((msg_t*)buf)->room_id][2] = 1;
										}
										else{
											rooms[((msg_t*)buf)->room_id][2] = 0;
										}
									}
									else{
										resignation[((msg_t*)buf)->client_id] = index2;
									}					
								}
								if(move == 1){
									if((((msg_t*)buf)->client_id % 2 == 0 && index2 > 55) || (((msg_t*)buf)->client_id % 2 == 1 && index2 < 8)){
										if(plansze[((msg_t*)buf)->room_id][index2] == 1 || plansze[((msg_t*)buf)->room_id][index2] == 3){
											plansze[((msg_t*)buf)->room_id][index2]++; 
										}
									}
									if(((msg_t*)buf)->client_id % 2 == 0){
										rooms[((msg_t*)buf)->room_id][2] = 1;
									}
									else{
										rooms[((msg_t*)buf)->room_id][2] = 0;
									}
									if(beat == index){
										plansze[((msg_t*)buf)->room_id][index2] = 0;
									}
								}
								if(move == 0){
									response.cmd = CMD_ERR;
								}
							}
							else{
								response.cmd = CMD_ERR;
							}
						}
						else{
							response.cmd = CMD_ERR;
						}
						//sprawdzenie czy gra się zakończyła
						if(endgame == true){
							resignation[((msg_t*)buf)->room_id*2] = -1;
							resignation[((msg_t*)buf)->room_id*2 + 1] = -1;
							rooms[((msg_t*)buf)->room_id][2] = 0;
							for(int j = 0; j < 64; j++){
								plansze[((msg_t*)buf)->room_id][j] = initialize_ind[j];
							}
						}
					}
					break;
				
				//wyjście z gry
				case CMD_SIGN_OUT:
					{
						if(((msg_t*)buf)->client_id != 0xffff){
							if(((msg_t*)buf)->client_id % 2 == 0){
								rooms[((msg_t*)buf)->room_id][0] = 0xffff;
							}
							else{
								rooms[((msg_t*)buf)->room_id][1] = 0xffff;
							}
							for(int j = 0; j < 64; j++){
								plansze[((msg_t*)buf)->room_id][j] = 0;
							}
						}
						response.client_id = 0xffff;
						rooms[((msg_t*)buf)->room_id][((msg_t*)buf)->client_id] = 0xffff;
						resignation[((msg_t*)buf)->client_id] = -1;
						if(((msg_t*)buf)->client_id % 2 == 0){
							resignation[((msg_t*)buf)->client_id + 1] = -1;
						}
						else{
							resignation[((msg_t*)buf)->client_id-1] = -1;
						}
						write(tcp_socket, &response, sizeof(msg_t));
						pthread_mutex_unlock(&mutex);
						close(tcp_socket);
						pthread_exit(NULL);
					}
					break;
				//rezygnacja z ruchu
				case CMD_RESIGN:
					{
						if(resignation[((msg_t*)buf)->client_id] != -1){
							if(((msg_t*)buf)->client_id % 2 == 0){
								rooms[((msg_t*)buf)->room_id][2] = 1;
							}
							else{
								rooms[((msg_t*)buf)->room_id][2] = 0;
							}
							if(plansze[((msg_t*)buf)->room_id][resignation[((msg_t*)buf)->client_id]] == 1 || plansze[((msg_t*)buf)->room_id][resignation[((msg_t*)buf)->client_id]] == 3){
								if(check_move(resignation[((msg_t*)buf)->client_id], ((msg_t*)buf)->client_id, plansze[((msg_t*)buf)->room_id]) == true){
									plansze[((msg_t*)buf)->room_id][resignation[((msg_t*)buf)->client_id]] = 0;
								}
							}
							else{
								if(check_king_move(resignation[((msg_t*)buf)->client_id], ((msg_t*)buf)->client_id) == true){
									plansze[((msg_t*)buf)->room_id][resignation[((msg_t*)buf)->client_id]] = 0;
								}
							}
						}
						else{
							response.cmd = CMD_ERR;
						}
						resignation[((msg_t*)buf)->client_id] = -1;
					}
					break;
				//odświeżenie szachownicy
				case CMD_REFRESH:
				{	
					char msg[64];
					for(int j = 0; j < 64; j++){
						msg[j] = (char)(plansze[((msg_t*)buf)->room_id][j]);
					}
					char x;
					if(rooms[((msg_t*)buf)->room_id][2] == 0){
						x = '0';
					}
					else{
						x = '1';
					}
					char msg2[19] = {'T', 'r', 'w', 'a', ' ', 't', 'u', 'r', 'a', ' ', 'g', 'r', 'a', 'c', 'z', 'a', ' ', x, '.'};
					write(tcp_socket, &msg, 64);
					write(tcp_socket, &msg2, 19);
				}
					break;
				default:
					response.cmd = CMD_ERR;
					break;
			}
		write(tcp_socket, &response, sizeof(msg_t));
        pthread_mutex_unlock(&mutex);
    }
}

int main(){

	//przygotowanie tablicy z pokojami i plansz
	for(int i = 0; i < ROOM_COUNT; i++){
		rooms[i][0] = 0xffff;
		rooms[i][1] = 0xffff;
		rooms[i][2] = 0;
		for(int j = 0; j < 64; j++){
			plansze[i][j] = 0;
		}
	}
	for(int i = 0; i < ROOM_COUNT*2; i++){
		resignation[i] = -1;
	}

	//przygotowanie połączenia
	int fd=socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in saddr, caddr;
	socklen_t l = sizeof(caddr);
	saddr.sin_family = AF_INET;
        saddr.sin_port = htons(TCP_PORT);
        saddr.sin_addr.s_addr = INADDR_ANY;
	bind(fd, (struct sockaddr*)&saddr, sizeof(struct sockaddr));
	listen(fd, 10);
	printf("start\r\n");
	uint8_t buf[sizeof(msg_t)];
	while(1){
		int cfd = accept(fd, (struct sockaddr*) &caddr, &l);	
		pthread_mutex_lock(&mutex);
		if(recv(cfd, &buf, sizeof(msg_t), 0) != sizeof(msg_t)) continue;
		msg_t response = {
            .cmd = 255,
            .room_id = ((msg_t*)buf)->room_id,
            .client_id = ((msg_t*)buf)->client_id,
            .move_from = {'-', '-'},
            .move_to =  {'-', '-'}
        };
		//znalezienie miejsca dla nowego gracza i inicjacja gry
		if(((msg_t*)buf)->cmd == CMD_SIGN_IN)
        {
            uint16_t client_id = find_next_client_id();
            if(client_id == -1)                     //nie można podłaczyć kolejnego klienta
            {
                response.cmd = 254;                 //zwracamy błąd
				write(cfd, &response, sizeof(msg_t));
            }
            else
            {
                response.client_id = client_id;
                response.room_id = client_id / 2;
                //tu uruchamiamy wątek dla nowego klienta
                client_t client = {
                    .client_id = client_id,
                    .tcp_socket = cfd
                };
                pthread_create(&threads[response.client_id], NULL, client_th, (void *)&client);
            }
        }
        else
        {
            response.cmd = 254;                     //spodziewamy sie tylko cmd = 1 czyli podłaczenie nowego klienta, w innym przypadku zwracamy błąd
			write(cfd, &response, sizeof(msg_t));
        }
		pthread_mutex_unlock(&mutex);
	}
	close(fd);
	return 0;
}
