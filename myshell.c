#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <pthread.h>
#include <errno.h>
#include "parser.h"
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

//Registro que contiene la información de un proceso en segundo plano
typedef struct Info {
	pid_t *hijos;
        char *mandatos;
        int estado;
	int numCom;
	int **pipes;
}TInfo;

void changeDirectory (int argc, char *argv[]);
int foreground (TInfo *bgProcess,int numBg, int argc, char *argv[]);
int changePermissions(int argc, char *argv[]);
void manejador1();
void manejador2();

int main (int argc, char *argv[]) {
	char linea[1024];
	pid_t  pid;
	tline *line;
	int entrada;
	int salida;
	int error;
	int n;
	int i;
	int j;
	int x;
	int **pipes;
    	TInfo *bgProcess;
	int numBg;
	int state2;
	char *state;

//Si se ejecuta ./myshell +argumentos da error. 
	if (argc != 1) {
		fprintf(stderr, "Error. Uso %s\n", argv[0]);
		return 1;
	}

	else {
		//signal(SIGINT, manejador2);
		printf("msh> ");
		signal(SIGINT,manejador2); 
		while(fgets(linea, 1024, stdin)){
			line = tokenize(linea);
			n = line->ncommands;
			//Caso en el que solo se pulsa enter
			if (n == 0) {
				//pasa a la siguiente iteración
			}
			//Caso cd
			else if (n == 1 && strcmp(line->commands[0].argv[0], "cd") == 0){
				changeDirectory(line->commands[0].argc, line->commands[0].argv);
			}
			//Caso jobs
			else if (n == 1 && strcmp(line->commands[0].argv[0], "jobs") == 0){
				for(j = 0; j<numBg; j++){
					//Comprobamos los procesos terminados.
					for(i = 0; i<bgProcess[j].numCom; i++){
						bgProcess[j].estado = 0;
                                                if(bgProcess[j].estado == 0 && waitpid(bgProcess[j].hijos[i], &state2, WNOHANG) == 0){
                                                        bgProcess[j].estado = 1;
                                                }
                                        }

					if(bgProcess[j].estado == 0){
        			                state = "Terminado";
                			}
                			else {
                        			state = "Ejecutando";
                			}

               			 	printf("[%d]+   %s   %s", j+1, state, bgProcess[j].mandatos);
					//Después de mostrarlo si el proceso ha terminado se borra y se libera memoria.
					if(bgProcess[j].estado == 0){
						free(bgProcess[j].hijos);
                                                free(bgProcess[j].mandatos);

						for(x = 0; x<bgProcess[j].numCom-1; x++){
                                                        free(bgProcess[j].pipes[x]);
                                                }
						free(bgProcess[j].pipes);
						for(i = j; i<numBg-1; i++){
							bgProcess[i] = bgProcess[i+1];
						}

						bgProcess = realloc(bgProcess, (numBg-1)*sizeof(struct Info));
						j--;
						numBg--;
                                        }
				}//for
                        }
			//Caso fg
                        else if (n == 1 && strcmp(line->commands[0].argv[0], "fg") == 0){
                        	if(foreground(bgProcess, numBg, line->commands[0].argc, line->commands[0].argv)==0){
					numBg--;
				}
                        }
			//Caso umask
			else if (n == 1 && strcmp(line->commands[0].argv[0], "umask") == 0){
                               changePermissions(line->commands[0].argc, line->commands[0].argv);
                        }
			//Caso exit
			else if (n == 1 && strcmp(line->commands[0].argv[0], "exit") == 0){
				//Terminamos con todos los procesos y liberamos memoria
                               for(j = 0; j<numBg; j++){
                                        for(i = 0; i<bgProcess[j].numCom; i++){
                                                kill(bgProcess[j].hijos[i], SIGTERM);
						waitpid(bgProcess[j].hijos[i], NULL, 0);
                                        }
					for(x = 0; x<bgProcess[j].numCom-1;x++){
                                                free(bgProcess[j].pipes[x]);
                                        }
                                        free(bgProcess[j].pipes);
                                        free(bgProcess[j].hijos);
                                        free(bgProcess[j].mandatos);
				}
				if(numBg>0){
					free(bgProcess);
				}
				exit(0);
                        }

			else {
				//Caso de más de un mandato (conectados por pipes)
	                        if(n>1){
					pipes = malloc((n-1)*sizeof(int*));
					for(i = 0; i<n-1; i++){
						pipes[i] = malloc(2*sizeof(int));
					}
					for(j = 0; j < n-1; j++){
						pipe(pipes[j]);
					}
				}
				//Caso procesos en background
                                if(line->background){
						//Si no hay procesos en bg se crea el array
                                                if(numBg == 0){
                                                        bgProcess = malloc(sizeof(struct Info));
                                                        numBg++;
                                                }
						//En caso de que haya procesos en bg se redimensiona
                                                else{
                                                        numBg++;
                                                        bgProcess = realloc(bgProcess, numBg*sizeof(struct Info));
                                                }
                                                if(n>1){
                                                        bgProcess[numBg-1].pipes = pipes;
                                                }
                                                bgProcess[numBg-1].hijos =malloc(n*sizeof(pid_t));
                                                bgProcess[numBg-1].estado = 0;
                                                bgProcess[numBg-1].numCom = n;
                                                bgProcess[numBg-1].mandatos = strdup(linea);
				}

				for(i = 0; i<n; i++){
					pid = fork();
					if (pid == 0){
						//Establecemos como van a actuar ante el Ctrl+C
						if(line->background){
							signal(SIGINT, SIG_IGN);
						}
						else {
							signal(SIGINT, SIG_DFL);
						}
						break;
					}
					else if (pid < 0){
                        	        	fprintf(stderr, "Error en el fork");
                       		 	        continue;
					}
					else {
						if(line->background){
                                                        bgProcess[numBg-1].hijos[i] = pid;
                                                }
					}
				}//fin del for

				if (pid == 0){ //Código de los procesos hijos.
					if (i == 0 && line->redirect_input != NULL) { //Redireccionamiento de entrada estándar
		        	                entrada = open(line->redirect_input, O_RDONLY);
						if (entrada == -1){
							//Error al abrir el fichero
                                        	        fprintf(stderr, "%s: Error. %s \n", line->redirect_input,strerror(errno));
							exit(1);
	                                        }
						dup2(entrada, 0);
                			}

			                if (i == n-1 && line->redirect_output != NULL) { //Redireccionamiento de salida estándar
						//Si existe se abre en modo escritura, en caso contrario lo crea .
        	        		        salida = open(line->redirect_output, O_CREAT|O_WRONLY, 0664);
						if (salida == -1){
                                                        //Error al abrir el fichero
                                                        fprintf(stderr, "%s: Error. %s \n", line->redirect_output,strerror(errno));
                                                        exit(1);
                                                }

						dup2(salida, 1);
                			}

		                	if (i == n-1 && line->redirect_error != NULL) { //Redireccionamiento de sálida estándar de error.
						//Si existe se abre en modo escritura, en caso contrario lo crea.
                		        	error = open(line->redirect_error, O_CREAT|O_WRONLY, 0664);
						if (error == -1){
                                                        //Error al abrir el fichero
                                                        fprintf(stderr, "%s: Error. %s \n", line->redirect_error,strerror(errno));
                                                        exit(1);
                                                }

						dup2(error, 2);
                			}
					//Cerrar los  extremos de los pipes que no se van a usar
					//Redireccionamiento de entrada/salida estándar a los extremos de los pipes correspondiente
					if (n>1){
						if (i==0){
							close(pipes[0][0]);
							dup2(pipes[0][1], 1);
							for(j = 1; j<n-1; j++){
								close(pipes[j][0]);
								close(pipes[j][1]);
							}
						}

						else if (i == n-1){
							close(pipes[n-2][1]);
							dup2(pipes[n-2][0],0);
							for (j = 0; j<n-2; j++){
								close(pipes[j][0]);
								close(pipes[j][1]);
							}
						}

						else {
							for(j = 0; j<n-1; j++){
								if(j == i-1){
									close(pipes[j][1]);
									dup2(pipes[j][0], 0);
								}
								else if(j == i){
									close(pipes[j][0]);
	                                                        	dup2(pipes[j][1], 1);
								}
								else {
									close(pipes[j][0]);
									close(pipes[j][1]);
								}
							}
						}
					}//if

					execv(line->commands[i].filename, line->commands[i].argv);
					fprintf(stderr, "%s: no se encuentra el mandato\n", line->commands[i].argv[0]);
					exit(0);
				} //if hijo

				else { //Código de proceso padre
					if(n>1){
						//Cierra los pipes
						for (j = 0; j < n-1; j++){
	                	                	close(pipes[j][0]);
                	        	                close(pipes[j][1]);
                        	        	}
					}
					//El proceso padre espera de forma no bloqueante por los procesos que se encuentran en segundo plano
                                        for(j=0; j<numBg; j++){
                                                for(i = 0; i<bgProcess[j].numCom; i++){
                                                         waitpid(bgProcess[j].hijos[i], &state2, WNOHANG);
                                                }
                                        }
					//En el caso de proceso en primer plano, el proceso espera de forma bloqueante a que terminen su ejecución
                                        if(!line->background){
						signal(SIGINT,manejador1);
                                                for(j = 0; j < n; j++){
                                                        wait(NULL);
                                                }
                                        }
				} //else padre

				if(n > 1){
					//Liberación de memoria en caso de procesos en segundo plano
					if(!line->background){
						//signal(SIGINT,manejador1);
                                        	for(j=0; j<n-1;j++){
                                                	free(pipes[j]);
                                        	}
                                        	free(pipes);
                                        }
				}
			}//else
			printf("msh> ");
			signal(SIGINT,manejador2);
		}//while
	}//else
	return 0;
}

//Función que ejecuta el mandato cd
void changeDirectory (int argc, char *argv[]) {
        char *dir;
        char aux [200];
	//Comprobación de argumentos
	//Caso en el que te pasan el directorio
        if (argc == 2){
                dir=argv[1];
        }
	//Caso sin directorio, para cambiar al directorio HOME
        else if (argc == 1){
                dir = getenv("HOME");
                if (dir == NULL) {
                        fprintf(stderr, "Error, no existe la variable $HOME\n");
                }
        }
	//Caso de error
        else {
                fprintf(stderr,"Error. Uso: %s directorio\n", argv[0]);
		exit(0);
        }

        if (chdir(dir) != 0){
                fprintf(stderr, "Error al cambiar de directorio\n");
        }

        else {
                printf("El directorio actual es: %s\n", getcwd(aux, -1));
        }
}
//Función que ejecuta el mandato umask
int changePermissions(int argc, char *argv[]){
	int num;
	int n;
	int cons = 777;
	char *p;
        char texto[20];
	mode_t num2;
	//Caso en el que se cambian los permisos
	if(argc == 2){
		if(strlen(argv[1]) == 4){
			strcpy(texto, argv[1]);
			p = texto;
			//Comprobación de que el parámetro es un número octal
			if(*p != '0'){
				fprintf(stderr, "Error. Uso: umask + [número octal]\n");
			}
			else {
				p++;
				while(*p != '\0'){
					n = atoi(p);
					if(n >= cons){
						fprintf(stderr, "Error. Uso: umask + [número octal]\n");
						return 1;
					}
					cons = cons/10;
					p++;
				}
				num = atoi(argv[1]);
                        	if(num != 0){
                                	umask(num);
	                        }
        	                else{
                	                fprintf(stderr, "Parámetros incorrectos. Uso: umask + [número octal]\n");
					return 1;
                        	}
			}
		}

		else{
			fprintf(stderr, "Parámetros incorrectos. Uso: umask + [número octal]\n");
			return 1;
		}
	}
	//Caso sin parámetros
	else if (argc == 1){
		num2 = umask(0);
		umask(num2);
		printf("%04o\n", num2);
	}

	else {
		fprintf(stderr, "Número de parámetros incorrecto. Uso: umask + [número octal]\n");
		return 1;
	}
	return 0;
}
//Función que ejecuta el mandato fg
int foreground (TInfo* bgProcess, int numBg, int argc, char *argv[]){
	int pos;
	int i;
	//Comprobación de parámetro
	if(argc >= 3){
		fprintf(stderr,"Número de parámetros incorrecto. Uso: fg + [número]");
		return 1;
	}
	//Caso sin parámetros
	else if(argc == 1){
		pos = numBg;
	}
	//Caso con un parámetro
	else if(argc == 2){
		pos = atoi(argv[1]);
		if(pos == 0){
			printf("Parámetros incorrectos. Uso: fg + [número]");
			return 1;
		}
		else{
			//Número de proceso en segundo plano incorrecto
			if(pos > numBg){
				fprintf(stderr,"Error. El número no coincide con ningún proceso");
				return 1;
			}
		}
	}
	//El proceso padre espera de forma bloqueante por el proceso que se acaba de pasar a primer plano
	for(i = 0; i < bgProcess[pos-1].numCom; i++){
		waitpid(bgProcess[pos-1].hijos[i], NULL, 0);
	}
	//Liberación de memoria
	free(bgProcess[pos-1].hijos);
        free(bgProcess[pos-1].mandatos);
        for(i = 0; i<bgProcess[pos-1].numCom-1; i++){
      		  free(bgProcess[pos-1].pipes[i]);
        }
        free(bgProcess[pos-1].pipes);
	for(i = pos-1; i < numBg-1; i++){
		 bgProcess[i] = bgProcess[i+1];
        }

        bgProcess = realloc(bgProcess, (numBg-1)*sizeof(struct Info));
	return 0;
}
//Manejador de la señal SIGINT en el caso de estar esperando a la ejecución de un proceso en primer plano
void manejador1(){
	fprintf(stdout,"\n");
	fflush(stdout);
}
//Manejador de la señal SIGINT para controlar el salto de línea y que se muestre el prompt
void manejador2(){
	fprintf(stdout,"\n");
	fprintf(stdout,"msh> ");
	fflush(stdout);
}

