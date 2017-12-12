#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <string>
#include <cstring>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
using namespace std;

#define MAX_CLIENTS 20
#define MAX_FILES 20
#define BUFLEN 1024

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

/*Functie care returneaza dimensiunea unui fisier,
localizat intr-un director ( in Bytes ) , sau -1,
daca fisierul nu exista*/
long dimFisier (char *director, char *nume_fis) {
	char *cale = (char *) malloc (30);
	strcpy(cale,"./");
	strcat(cale, director);
	strcat(cale, "/");
	strcat(cale, nume_fis);
	struct stat fis_stat;
	if ( stat ( cale , &fis_stat ) != 0 )
		return -1 ;
	return fis_stat.st_size;
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
        int sockfd, n, fdmax;
        struct sockaddr_in serv_addr;

        char buffer[BUFLEN];
        if (argc != 6) {
        	fprintf(stderr,"Numar insuficient de argumente!\n");
       		exit(0);
    	}

	//Socket file descripoti pentru citire, respectiv temporari
	fd_set read_fds, tmp_fds;

    	ofstream f;
    	char *numeFisLog = (char *)malloc(15);
    	sprintf(numeFisLog,"%s.log",argv[1]);

	//Golim multimea de descriptori de citire (read_fds) si multimea tmp_fds 
    	FD_ZERO(&read_fds);
    	FD_ZERO(&tmp_fds); 
    
   	sockfd = socket(AF_INET, SOCK_STREAM, 0);
    	if (sockfd < 0) {
        	f<<"-1: Eroare la conectare\n";
	 	cout<<"-1: Eroare la conectare\n";
		return -1;
    	}
    
	//Parametrii conexiunii intre client si server
    	serv_addr.sin_family = AF_INET;
    	serv_addr.sin_port = htons(atoi(argv[5]));
    	inet_aton(argv[4], &serv_addr.sin_addr);

    	if (connect(sockfd,(struct sockaddr*) &serv_addr,sizeof(serv_addr)) < 0) {
        	f<<"-1: Eroare la conectare\n";
	 	cout<<"-1: Eroare la conectare\n";
		return -1;
    	}

   	struct sockaddr_in client_addr;

    	int sockfd_client = socket(AF_INET,SOCK_STREAM, 0);
    	if (sockfd_client < 0)  {
        	f<<"-1: Eroare la conectare\n";
	 	cout<<"-1: Eroare la conectare\n";
		return -1;
    	}
	//Parametrii conexiunii pe care asculta clientul
    	client_addr.sin_family = AF_INET;
    	client_addr.sin_port = htons(atoi(argv[3]));
    	client_addr.sin_addr.s_addr=INADDR_ANY;    
    
    	if (bind(sockfd_client, (struct sockaddr *)&client_addr, sizeof(client_addr))< 0)   {
        	f<<"-1: Eroare la conectare\n";
	 	cout<<"-1: Eroare la conectare\n";
		return -1;
    	}

	//Clientul trimite serverului un mesaj prin care se identifica in mod unic
    	sprintf(buffer,"DateClient: %s %d",argv[1],atoi(argv[3]));

    	n = send(sockfd, buffer, strlen(buffer), 0);
    	if (n<0){
		f<<"-2: Eroare la citire/scriere pe socket\n";
		cout<<"-2: Eroare la citire/scriere pe socket\n";
    	}

    	FD_SET(sockfd,&read_fds);
    	FD_SET(sockfd_client,&read_fds);
    	FD_SET(0,&read_fds);
    	FD_SET(0,&tmp_fds);
    	fdmax = sockfd_client;

	/*com - sir de caractere auxiliar utilizat in cazul introducerii
	de comenzi de la tastatura*/
	char com[50];
	strcpy(com, "");
	char *token ;
	char dimFis[15];
	
	//Map pentru stocarea locala a fisierelor partajate
	map<string, string> fis_part;
	/*Vector ce contine informatiile despre clientii conectati.
	Este actualizat la fiecare rulare a comenzii infoclients.*/
	vector<string> date_clienti ;

    	while(1) {
		/*Tinem o copie a multimii de descriptori de citire la fiecare iteratie din
		bucla infinita, intrucat apelul select o modifica, aceasta ajungand sa ii contina
		doar pe aceia pe care s-au primit date*/
		tmp_fds = read_fds;
		if (select(fdmax+1, &tmp_fds, NULL, NULL, NULL)==-1) {
         		f<<"-1: Eroare la conectare\n";
	 		cout<<"-1: Eroare la conectare\n";
			return -1;
        	}
		//Se citeste o comanda de la tastatura
		if (FD_ISSET(0,&tmp_fds)) {
			memset(buffer,0,BUFLEN);
			cin.getline(buffer,BUFLEN);
			if (esteComanda (buffer) == 1) {
				strcpy (com,buffer);
				/*Pentru comanda quit, aceasta se trimite serverului ca mesaj
				si inchidem socketul corespunzator clientului*/
				if (strcmp (buffer, "quit") == 0) {
					    n = send(sockfd, buffer, strlen(buffer), 0);
					    if (n<0) {
					    	f<<"-2: Eroare la citire/scriere pe socket\n";
						cout<<"-2: Eroare la citire/scriere pe socket\n";
						break;
				    	    }
				    	    else	
						close(sockfd);
					    f<<argv[1]<<">quit\n";
					    f<<"Client inchis cu succes\n";
					    f.close();
				  	    cout<<"Client inchis cu succes\n";
				    	    return 0;
				}
				/*Pentru comanda share <nume_fisier> , se verifica daca fisierul exista
				in directorul specificat in linia de comanda*/
				/*Daca da, atunci dimensiunea lui se adauga la sfarsitul comenzii, urmand sa fie trimisa
				la server si se actualizeaza local lista fisierelor partajate.
				Daca nu, atunci se afiseaza cod de eroare, fara a se trimite mesaj.*/
				else if (strncmp (buffer, "share", 5) == 0) {
					token = strstr(buffer," ")+1;
					long dimensiune = dimFisier(argv[2],token);
					if (dimensiune == -1) {
						f<<"-5: Fisier inexistent\n";
						cout<<"-5: Fisier inexistent\n"; 
					}
					else {
						strcat(buffer," ");
						sprintf(dimFis,"%ld", dimensiune);
						strcat(buffer,dimFis);
						string aux = string(token).substr(0,string(token).find(' '));
						if ( (fis_part.size() < MAX_FILES) && (fis_part.find(aux) == fis_part.end()))
							fis_part.insert(pair<string,string>(aux,string(dimFis)));
						n = send(sockfd, buffer, strlen(buffer), 0);
						if (n<0){
							f<<"-2: Eroare la citire/scriere pe socket\n";
							cout<<"-2: Eroare la citire/scriere pe socket\n";
							break;
						}
					}
				}
				//Celelalte comenzi implementate vor fi trimise asa cum sunt introduse
				/*Insa pentru unshare, scoatem din lista de fisiere partajate fisierul specificat,
				daca acesta se regaseste in lista*/
				else if (strncmp (buffer, "unshare", 7)== 0) {
					token = strstr(buffer," ")+1;
					if ( fis_part.find (token) != fis_part.end() )
						fis_part.erase(string(token)); 
					n = send(sockfd, buffer, strlen(buffer), 0);
					if (n<0){
						f<<"-2: Eroare la citire/scriere pe socket\n";
						cout<<"-2: Eroare la citire/scriere pe socket\n";
						break;
					}
				}
				else {
					n = send(sockfd, buffer, strlen(buffer), 0);
					if (n<0){
						f<<"-2: Eroare la citire/scriere pe socket\n";
						cout<<"-2: Eroare la citire/scriere pe socket\n";
						break;
					}
				}
			}
			else 
				cout<<"Comanda nesuportata de client\n";
		}
		//Clientul primeste un mesaj
		else if (FD_ISSET(sockfd, &tmp_fds)) {
      			memset(buffer,0,BUFLEN);
			n = recv(sockfd, buffer, sizeof(buffer), 0);
			if (n == 0) { 
				//Conexiunea s-a inchis
				cout<<"Serverul a inchis conexiunea\n";
				break;
			} 
			if(n < 0){
				f<<"-2: Eroare la citire/scriere pe socket\n";
				cout<<"-2: Eroare la citire/scriere pe socket\n";
				break;
			}
			/*Daca mesajul primit reprezinta raspunsul unei comenzi
			introduse in prealabil, atunci se afiseaza in terminal
			si comanda si raspunsul.*/
			if ( strcmp (com, "") != 0) {
				f<<argv[1]<<">"<<com<<endl;
				cout<<buffer;
				f<<buffer;
				/*In cazul comenzii infoclients, este necesar sa reactualizam informatiile
				legate de clientii conectati la server, prin parsarea raspunsului.*/
				if (strncmp (com, "infoclients", 11) == 0) {
					date_clienti.clear();
					char *linie;
					linie = strtok(buffer, "\n");
					while (linie!=NULL) {
						date_clienti.push_back(string(linie));
						linie = strtok(NULL,"\n");
					}
				}
				memset(com,0,50);
			}
			/*Daca mesajul primit este de accept, clientul poate comunica cu serverul.
			In caz contrar, executia programului se termina.*/
			else {
				if (strcmp (buffer, "accept") == 0) {
					f.open(numeFisLog);
					cout<<"Client acceptat de server\n";
					f<<argv[1]<<">Client acceptat de server\n";
				}
				else if (strcmp (buffer, "reject") == 0) {
					cout<<"Client refuzat de server (nume deja existent)\n";
					return -1;
				}
				else 
					;	
			}
		}
		else ;
    }

    return 0;
}

