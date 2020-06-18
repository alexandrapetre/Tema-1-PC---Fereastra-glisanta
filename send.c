#include <string.h>

#include <stdio.h>

#include <stdlib.h>

#include <unistd.h>

#include <fcntl.h>

#include <sys/types.h>

#include <sys/stat.h>

#include "link_emulator/lib.h"

#define TIMEOUT 1000
#define HOST "127.0.0.1"
#define PORT 10000

struct stat file_status;

//----strictura pachetelor---//
typedef struct {
  int number;
  int checksum;
  int bytes;
  int count;
  char payload[MSGSIZE - 4 * sizeof(int)];
}
pkt;

//---functie care calculeaza minimul---//
int min(int a, int b) {
  if (a < b) {
    return a;
  }
  return b;
}

//----functie care calculeaza checksum pentru fiecare pachet---//
int string_parity(pkt pachet) {

  char* string = pachet.payload;
  int res = string[0] ^ string[1];
  int len = pachet.bytes;
  int i = 2;

  while(i < len){
    res = res ^ string[i];
    i++;
  }

  res = res ^ pachet.number;
  res = res ^ pachet.bytes;
  res = res ^ pachet.count;

  return res;
}


int main(int argc, char ** argv) {

  init(HOST, PORT);
  msg t, r;
  pkt pk;

  char * file_name;
  file_name = argv[1];
  int speed, delay;
  int i;
  speed = atoi(argv[2]);
  delay = atoi(argv[3]);
  int BDP = speed * delay;
  int window = (BDP * 1000) / (sizeof(msg) * 8);//calculez fereastra


//---creez numele fisierului de iesire----//
  char new_filename[11];
  char recv[11];
  strcpy(recv, "recv_");
  strcpy(new_filename, file_name);
  strcat(recv, new_filename);

  int file, size_file;
  file = open(file_name, O_RDONLY);
  fstat(file, & file_status);
  size_file = (int) file_status.st_size;
  int number_seq = size_file / (MSGSIZE - (4 * sizeof(int))) + 1;//numarul de cadre

  window = min(window, number_seq);//marimea ferestrei

  // tasks

  char BUFF[MSGSIZE - (4 * sizeof(int))];
  int nr_bytes;
  msg msg_sent[number_seq + 1]; //un vector de mesaje trimise

  int * ack = (int * ) calloc(1, sizeof(int) * (number_seq + 1));

  for (i = 0; i < window; i++) {

    if (i == 0) {
      memset(t.payload, 0, sizeof(t.payload));//trimit in prima fereastra
                                              //numele fisierului
      memset(pk.payload, 0, sizeof(pk.payload));
      pk.number = 0;
      pk.count = number_seq;
      memcpy(pk.payload, & recv, sizeof(recv));
      pk.checksum = string_parity(pk);
      t.len = 4 * sizeof(int) + strlen(recv) + 1;
      memcpy(t.payload, & pk, sizeof(t.payload));
      send_message( &t);
      msg_sent[i] = t;//se salveaza mesajul

    } else {

      //se trimit primele pachete dimensiune(window - 1)
      memset(t.payload, 0, sizeof(t.payload));
      memset(pk.payload, 0, sizeof(pk.payload));
      nr_bytes = read(file, BUFF, MSGSIZE - (4 * sizeof(int)));
      memcpy(pk.payload, BUFF, nr_bytes);
      pk.number = i;
      pk.count = number_seq;
      pk.bytes = nr_bytes;
      pk.checksum = string_parity(pk);
      t.len = 4 * sizeof(int) + nr_bytes;
      memcpy(t.payload, & pk, t.len);
      msg_sent[i] = t;//se salveaza mesajul

      if (send_message( &t) < 0) {
        perror("Send error.\n");
        return -1;
      }
    }
  }

  int j = 0, k = 0;
  int count = 0;

  for (;; i++) {
    recv_message_timeout( &r, delay / 4);

    pkt p_rcv = * ((pkt *) r.payload);
    ack[p_rcv.number] = 1;//s-a primit in recv pachetul bun, nu trebuie retrimis

    //cat timp pot citi din fisier trimit pachete
    nr_bytes = read(file, BUFF, MSGSIZE - 4 * sizeof(int));

      if (nr_bytes > 0) {

        memset(t.payload, 0, sizeof(t.payload));
        memset(pk.payload, 0, sizeof(pk.payload));
        memcpy(pk.payload, BUFF, nr_bytes);
        pk.number = i;
        pk.count = number_seq;
        pk.bytes = nr_bytes;
        pk.checksum = string_parity(pk);
        t.len = 4 * sizeof(int) + nr_bytes;
        memcpy(t.payload, &pk, t.len);
        msg_sent[i] = t;//se pastreaza in vector mesajul
        count = i;

        if ( send_message( &t) < 0) {
          perror("send 2");
          return -1;
        }

      } else if (nr_bytes <= 0) { //cazul in care se pierd pachete
        if (j == count) {
          for (k = 0; k < count; k++) {
            if (ack[k] == 0) { //ultimul mesaj trimis corect
              j = k;
              break;
            }
          }
          if (k >= count) //conditie de oprire
            break;       //toate pachetele au fost trimise
        }

        for (; j < count; j++) {//se retrimit toate mesajele pierdute
          if (ack[j] == 0) {
            if ( send_message( &msg_sent[j]) < 0) {
              perror("send 3");
              return -1;
            }
            j++;
            break;
          }
        }
      }
    }

  //trimit la final 2 mesaje in plus
  //send se termina mai repede si nu se scrie tot in fisier
  sprintf(t.payload,"Last message sent");
  t.len = strlen(t.payload+1);
  send_message(&t);
  if (recv_message(&r)<0){
      perror("receive error");
  }

  sprintf(t.payload,"Last message sent");
  t.len = strlen(t.payload+1);
  send_message(&t);
  if (recv_message(&r)<0){
    perror("receive error");
  }

  return 0;
}
