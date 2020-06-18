#include <string.h>

#include <stdio.h>

#include <stdlib.h>

#include <unistd.h>

#include <fcntl.h>

#include "link_emulator/lib.h"

#define HOST "127.0.0.1"
#define PORT 10001
#define MAXVALUE 9000


//-----structura pachetului------//

typedef struct {
  int number;
  int checksum;
  int bytes;
  int count;
  char payload[MSGSIZE - 4 * sizeof(int)];
}
pkt;


//----- functie cu care calculez checksum-ul------//

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

  msg r, t;
  pkt pk;
  init(HOST, PORT);
  char file_name_out[50];


  //--------- crearea fisierului ---------//
  //creez un fisier temporar caruia ii schimb numele

  int file_out;
  file_out = creat("recv_temp", S_IRWXU | S_IRWXG | S_IRWXO); //creerea fisierului cu permisiuni
  file_out = open("recv_temp", O_WRONLY); // fisier de scriere



  //------- primire pachete trimitere ack -------- //


  msg * msg_rcv; //vector alocat dinamic de mesaje primite
  int * ack;// vector de ack-uri tine minte ce pachete s-au primit
  int check;
  ack = calloc(1, sizeof(int) * 1000);
  msg_rcv = (msg * ) calloc(1, sizeof(msg) * 1000);

  int contor = 0;
  int number_seq = MAXVALUE;
  int verif = 0;

  while (1) {

    while (ack[contor] == 1 && contor != 0 && verif == 0) {

      pkt pachet = * ((pkt * ) msg_rcv[contor].payload);
      write(file_out, pachet.payload, pachet.bytes);
      contor++;

    }

    if (contor == number_seq + 1) {
      break;
    }

    memset(r.payload, 0, sizeof(r.payload));
    memset(t.payload, 0, sizeof(t.payload));

    if (recv_message( & r) < 0) {
      perror("Receive message");
      return -1;
    }

    pk = *((pkt *) r.payload);
    verif = 0;
    check = string_parity(pk);//calculez checksum-ul pachetului primit

    if (check != pk.checksum) {//daca este corupt consider ca s-a pierdut
      verif = 1;

    } else {

      if (pk.number == 0 && verif == 0) {

        //daca s-a primit numele corect
        //se schimba numele fisierului in care se scrie
        //trimit ack, pentru ca am primit pachetul

        memcpy(file_name_out, pk.payload, r.len - sizeof(int));
        number_seq = pk.count;
        contor++;
        memset(pk.payload, 0, sizeof(pk.payload));
        memcpy(t.payload, & pk, pk.bytes);
        send_message( &t);
        rename("recv_temp", file_name_out);

      } else {
        if (pk.number == contor && verif == 0) {

          //primesc pachetele corect, in ordine
          //se scriu in fisier

          write(file_out, pk.payload, pk.bytes);
          contor++;
          memset(pk.payload, 0, sizeof(pk.payload));
          memcpy(t.payload, & pk, pk.bytes);
          t.len = 4 * sizeof(int) + pk.bytes;

          if ( send_message( &t) < 0) {
            perror("recv");
            return -1;
         }
        } else {
          if (ack[pk.number] == 0 && verif == 0) {
            //daca s-au primit cele pierdute
            //si nu sunt in ordine se trimite ack pentru ca le-am primit
            //se actualizeaza vectorul de ack-uri

            memset(pk.payload, 0, sizeof(pk.payload));
            memcpy(t.payload, & pk, pk.bytes);
            msg_rcv[pk.number] = r;
            ack[pk.number] = 1;
            t.len = 4 * sizeof(int) + pk.bytes;
            if ( send_message( &t) < 0) {
              perror("recv");
              return -1;
            }
          }
        }
      }
    }
  }


//cele doua mesaje trimise la final
  if (recv_message(&r)<0){
    perror("Receive message");
    return -1;
  }
  sprintf(t.payload, "ACK\n");
  t.len = strlen(r.payload) + 1;
  send_message(&t);

  if (recv_message(&r)<0){
    perror("Receive message");
    return -1;
  }
  sprintf(t.payload, "ACK\n");
  t.len = strlen(r.payload) + 1;
  send_message(&t);

  return 0;
}
