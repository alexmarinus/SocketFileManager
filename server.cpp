#include <iostream>
#include <fstream>
#include <map>
#include <cstring>
#include <vector>
#include <algorithm>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_CLIENTS 20
#define MAX_FILES 20
#define BUFLEN 1024
using namespace std;

/*Structura ce caracterizeaza un client conectat la server*/
/*Campurile sunt : socket file descriptor, numele trimis la identificare,
adresa IP, portul pe care pe asculta, timpul de conectare si un map
ce contine fisierele distribuite, sub forma de perechi <fisier dimensiune>*/
typedef struct Client {
	int sock;
	string nume;
	string adr_IP;
	int port;
	long timp;
	map<string, string> fis_distrib;
}Client;

/*Comparator utilizat la sortarea clientilor conectati in functie de timp*/
bool comparator (const Client& client1, const Client& client2) {
	return (client1.timp < client2.timp);
}

/*Functie care returneaza 1 daca un sir de caractere
constituie o comanda dintre cele implementate*/
int esteComanda (char *buffer) {
	if (strncmp (buffer, "infoclients", 11) == 0)
		return 1;
	if (strncmp (buffer, "getshare", 8) == 0)
		return 1;
	if (strncmp (buffer, "share", 5) == 0)
		return 1;
	if (strncmp (buffer, "unshare", 7) == 0)
		return 1;
	if (strncmp (buffer, "getfile", 8) == 0)
		return 1;
	if (strncmp (buffer, "history", 7) == 0)
		return 1;
	if (strncmp (buffer, "quit", 4) == 0)
		return 1;
	return 0;
}

/*Functie pentru conversia unui numar de tip long
( numar de Bytes ) in format user-friendly */
char *dimFormat (long dimensiune) {
	long unitati ;
	char *format = (char *)malloc(8);
	strcpy (format, "-1");
	if (dimensiune >= 1<<30) {
		unitati = dimensiune / (1<<30);
		sprintf (format, "%ld",unitati);
		strcat (format, "GiB");
	}
	else if (dimensiune >= 1<<20) {
		unitati = dimensiune / (1<<20);
		sprintf (format, "%ld",unitati);
		strcat (format, "MB");
	}
	else if (dimensiune >= 1<<10) {
		unitati = dimensiune / (1<<10);
		sprintf (format, "%ld",unitati);
		strcat (format, "KiB");
	}
	else {
		unitati = dimensiune ;
		sprintf (format, "%ld",unitati);
		strcat (format, "B");
	}
	return format;
}

int main(int argc, char *argv[])
{

	//Map cu perechi <socket Client>
	map<int, Client> clienti_sock;
	//Map cu perechi <nume Client>
	map<string, Client> clienti_nume;
	map<int, Client>::iterator it1;
	map<string, Client>::iterator it2;
	map<string, string>::iterator it3;
     	int sockfd, newsockfd, portno; 
	unsigned int clilen;
     	char buffer[BUFLEN], deTrimis[BUFLEN], infoClient[BUFLEN];
     	struct sockaddr_in serv_addr, cli_addr;
     	int n, i, j, k;
     	
	//Multimea de citire folosita in select(), respectiv o multime temporara
	fd_set read_fds , tmp_fds;
	//Valoare maxima file descriptor din multimea read_fds	 
     	int fdmax;		

     	if (argc < 2) {
        	cout<<"Numar insuficient de argumente!\n";
         	return -1;
     	}

     	ofstream f("logfile");
	f<<"Server deschis\n";

     	//Golim multimea de descriptori de citire (read_fds) si multimea tmp_fds 
     	FD_ZERO(&read_fds);
     	FD_ZERO(&tmp_fds);
     
     	sockfd = socket(AF_INET, SOCK_STREAM, 0);
     	if (sockfd < 0){
         	f<<"-1: Eroare la conectare\n";
	 	cout<<"-1: Eroare la conectare\n";
		return -1;
    	}
     
     	portno = atoi(argv[1]);

     	memset((char *) &serv_addr, 0, sizeof(serv_addr));
     	serv_addr.sin_family = AF_INET;
     	serv_addr.sin_addr.s_addr = INADDR_ANY;	// Adresa IP a masinii
     	serv_addr.sin_port = htons(portno);
     
     	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0){
         	f<<"-1: Eroare la conectare\n";
	 	cout<<"-1: Eroare la conectare\n";
		return -1;
    	}
     
     	listen(sockfd, MAX_CLIENTS);

     	FD_SET(sockfd, &read_fds);
     	FD_SET(0, &read_fds);
     	fdmax = sockfd;

	Client cl;

	while (1) {
		tmp_fds = read_fds; 
		if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1) {
		 	f<<"-1: Eroare la conectare\n";
		 	cout<<"-1: Eroare la conectare\n";
			return -1;
	    	}	
		for(i = 0; i <= fdmax; i++) {
			//Se citeste de la tastatura. Singura comanda suportata este quit.
			if (FD_ISSET(i, &tmp_fds)) {
				if(i == 0) {
			  	  memset(buffer, 0, BUFLEN);
				  cin.getline(buffer,BUFLEN);
				  if(strcmp(buffer, "quit") == 0) {
					//Sunt inchisi toti socketii clientilor
					for (it1 = clienti_sock.begin() ; it1!=clienti_sock.end() ; it1++)
						close(it1->first);
				  	close(sockfd);
					cout<<"Conexiune inchisa\n";
					f<<"Conexiune inchisa\n";
					f.close();
				  	return 0;
				  }
				  else {
					cout<<"Comanda nesuportata de server\n";
					f<<buffer<<endl;
					f<<"Comanda nesuportata de server\n";
				  }
			  }
			  else
				//S-a primit o cerere de conexiune
				if (i == sockfd) {
					clilen = sizeof(cli_addr);
					if ((newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen)) == -1) {
					 	f<<"-1: Eroare la conectare\n";
					 	cout<<"-1: Eroare la conectare\n";
						return -1;
				    	}
					else {
						FD_SET(newsockfd, &read_fds);
						if (newsockfd > fdmax) { 
							fdmax = newsockfd;
						}
						/*Tinem minte datele despre noul client, cu exceptia numelui,
						care este accesibil in urmatorii pasi ais programului*/
						string adr_IP(inet_ntoa(cli_addr.sin_addr));
						cl.adr_IP = adr_IP;
						cl.port = ntohs (cli_addr.sin_port);
						cl.sock = newsockfd;
						cl.timp = time(NULL);
						clienti_sock.insert(pair<int, Client>(newsockfd, cl));
					}
				}
					
				else {
					//S-au primit date pe unul din socketii cu care vorbim cu clientii
					memset(buffer, 0, BUFLEN);
					if ( (n = recv (i, buffer, sizeof(buffer), 0) ) <= 0 ) {
						if (n == 0) {
							//Conexiunea s-a inchis
							cout<<"Clientul cu numele "<<clienti_sock[i].nume<<" a inchis conexiunea\n";
							clienti_nume.erase(clienti_sock[i].nume);
							clienti_sock.erase(i);
							
						} 
						else {
							f<<"-2: Eroare la citire/scriere pe socket\n";
							cout<<"-2: Eroare la citire/scriere pe socket\n";
						}
						close(i);
						//Scoatem din multimea de citire socketul 
						FD_CLR(i, &read_fds); 
					} 
					
					else { //recv intoarce >0
						char token[20], token1[20], token2[20];
						if (esteComanda (buffer) == 1)
							f<<clienti_sock[i].nume<<">"<<buffer<<endl;
						else
							f<<buffer<<endl;
						strcpy (token, strtok(buffer," "));
						
						//Daca primim un mesaj de identificare
						//Atunci verificam unicitatea numelui sau
						if (strcmp (token, "DateClient:") == 0) {
							
							strcpy(token1, strtok(NULL," "));
							string name(token1);
							strcpy(token2, strtok(NULL," "));
							string numar_port(token2);
							//Daca numele este unic, atunci clientul este acceptat
							//Ambele map-uri sunt actualizate
     				if (clienti_nume.find(name) == clienti_nume.end() && clienti_sock.find(i) != clienti_sock.end()) {
								clienti_sock[i].nume = name;
								clienti_sock[i].port = atoi(numar_port.c_str());
								clienti_nume[name] = clienti_sock[i];
								strcpy(buffer,"accept");
								send(i, buffer, strlen(buffer), 0);
							}
							//Altfel, este respins
							else {
								clienti_sock.erase(i);
								strcpy(buffer,"reject");
								send(i, buffer, strlen(buffer), 0);
							}
							memset(buffer,0,BUFLEN);

						}
						/*Pentru o comanda infoclients trimisa de un client, serverul ii furnizeaza
						lista clientilor, in ordinea conectarii. Pentru aceasta, am stocat clientii intr-un vector
						auxiliar, sortat dupa comparatorul pe structuri Client*/
						else if (strcmp(token, "infoclients") == 0) {
							memset(deTrimis,0,strlen(deTrimis));
							vector<Client> listaClienti;
							vector<Client>::iterator itCl;
							for (it1=clienti_sock.begin() ; it1!=clienti_sock.end(); it1++)
								listaClienti.push_back(it1->second);
							sort(listaClienti.begin(),listaClienti.end(),comparator);
							for (itCl=listaClienti.begin() ; itCl != listaClienti.end(); itCl++) {
							
							  sprintf(infoClient,"%s %s %d\n",itCl->nume.c_str(),itCl->adr_IP.c_str(),itCl->port);
							  strcat(deTrimis,infoClient);
							}
							f<<deTrimis;
							if (strlen(deTrimis) == 0)
								;
							else 
								send(i,deTrimis,strlen(deTrimis),0);
						}
						//Se primeste o comanda share, ambele map-uri pentru clienti sunt modificate
						else if (strcmp(token, "share") == 0) {
							memset(deTrimis,0,strlen(deTrimis));
							char *fisier = strdup(strtok(NULL," "));
							char *dim = strdup(strtok(NULL, " "));
							string file(fisier);
							string size(dimFormat(atol(dim)));
							if (clienti_sock[i].fis_distrib.size() == MAX_FILES)
								strcpy(deTrimis,"Limita superioara de fisiere partajate depasita\n");
							else if (clienti_sock[i].fis_distrib.find(fisier) != clienti_sock[i].fis_distrib.end())
								strcpy(deTrimis,"-6: Fisier partajat cu acelasi nume\n");
							else {
							clienti_sock[i].fis_distrib.insert(pair<string, string>(file,size));
							clienti_nume[clienti_sock[i].nume].fis_distrib.insert(pair<string, string>(file,size));
							strcpy(deTrimis,"Succes\n");
							}
							f<<deTrimis;
							send(i, deTrimis, strlen(deTrimis), 0);
						}
						//Se primeste o comanda unshare
						//Daca exista deja in lista de partajari pentru un client, fisierul dorit este scos din lista
						else if (strcmp(token, "unshare") == 0) {
							memset(deTrimis,0,strlen(deTrimis));
						  	char *fisier = strdup(strtok(NULL," "));
						  	string file(fisier);
						  	if (clienti_sock[i].fis_distrib.size() == 0)
						     		strcpy(deTrimis,"-5: Fisier inexistent\n");
					  	  	else if (clienti_sock[i].fis_distrib.find(file) != clienti_sock[i].fis_distrib.end()) {
								clienti_sock[i].fis_distrib.erase(file);
								clienti_nume[clienti_sock[i].nume].fis_distrib=clienti_sock[i].fis_distrib;
								strcpy(deTrimis,"Succes\n");
						  	}
							else 
								strcpy(deTrimis,"-5: Fisier inexistent\n");
							f<<deTrimis;
						  	send(i, deTrimis, strlen(deTrimis), 0);
						}
						/*Raspunsul serverului la comanda share este dimensiunea listei de partajari, urmata
						de inregistrarile ei.*/
						else if (strcmp (token, "getshare") == 0) {
							memset(deTrimis,0,strlen(deTrimis));
							char *nume_client = strdup(strtok(NULL, " "));
							string namecl(nume_client);
							if (clienti_nume.find(namecl) == clienti_nume.end())
								strcpy(deTrimis, "-3: Client inexistent\n");
							else if (clienti_nume[namecl].fis_distrib.size() == 0)
								strcpy(deTrimis,"0\n");
							else {
							    char *infoShare = (char *)malloc(40);
							    map<string, string> listaShare = clienti_nume[namecl].fis_distrib;
							    sprintf(deTrimis,"%d\n",listaShare.size());
							    for (it3 = listaShare.begin(); it3 != listaShare.end(); it3++) {
							    	sprintf(infoShare,"%s %s\n",it3->first.c_str(),it3->second.c_str());
							        strcat(deTrimis,infoShare);
							    }
							}
							f<<deTrimis;
							send(i,deTrimis,strlen(deTrimis),0);
						}
						else 
							;						
					} 
				} 
			}
		}
     }


     close(sockfd);
   
     return 0; 
}



