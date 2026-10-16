/*
gcc -Wall -Wshadow -Wvla -g -c shell.c

gcc -g -o shell shell.o

valgrind --leak-check=full  --show-leak-kinds=all ./shell
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_BUFFER_SIZE 1024
#define MAX_SIZE 60
#define PATH_BASE "/bin/"
#define MAX_ARGS 11

char *get_var(char *var);
void set_var(char *var);
char *get_path(void);
int look_for_char(char *line, char caracter);
int builtin_command(char **arguments, int num);
int execute_command(char **args, int num);
char **token_line(char *line);
void pipes_maker(char **pipes, int bg, char **redirections);
void control_line(char *line, char **arguments, int args_counter, int bg,
		  char **redirections);
int array_counter(char **array);
char *file_path(char *name);

char *
get_path(void)
{

	char *path = (char *)malloc(MAX_BUFFER_SIZE * sizeof(char));

	if (path == NULL) {

		warnx("No se pudo asignar memoria para la ruta");
		return NULL;

	}

	if (getcwd(path, MAX_BUFFER_SIZE) == NULL) {

		warnx("No se pudo obtener el directorio actual");
		free(path);
		return NULL;
	}
	return path;
}

int
look_for_char(char *line, char caracter)
{

	int i;
	int control;

	control = -1;

	for (i = 0; i < strlen(line); i++) {
		if (line[i] == caracter) {

			control = i;
			break;
		}
	}
	return control;
}

int
builtin_command(char **arguments, int num)
{

	int control = 0;
	char *tmp;
	int i;

	if (arguments[0] == NULL) {

		control = 1;
		return control;
	}

	if (look_for_char(arguments[0], '=') != -1) {

		set_var(arguments[0]);
		control = 1;
		return control;
	}

	for (i = 0; i < num; i++) {

		if (look_for_char(arguments[i], '$') != -1) {

			if ((tmp = get_var(arguments[i])) == NULL) {

				return 0;
			}
			strcpy(arguments[i], tmp);
		}
	}

	if (strcmp(arguments[0], "cd") == 0) {
		if (arguments[1] == NULL) {

			const char *home_path = getenv("HOME");

			if (chdir(home_path) != 0) {

				fprintf(stderr,
					"Error: no se ha podido acceder al directorio\n");
			}
		} else {

			if (look_for_char(arguments[1], '$') != -1) {

				if ((tmp = get_var(arguments[1])) == NULL) {

					control = 1;
					return control;
				}
				strcpy(arguments[1], tmp);
			}

			if (chdir(arguments[1]) != 0) {

				fprintf(stderr,
					"Error: no se ha podido acceder al directorio\n");
			}
		}
		control = 1;
	}
	return control;
}

int
execute_command(char **args, int num)
{

	char *path;
	char *tmp;
	int i;

	for (i = 0; i < num; i++) {

		if (look_for_char(args[i], '$') != -1) {

			if ((tmp = get_var(args[i])) == NULL) {

				return 0;
			}
			strcpy(args[i], tmp);
		}
		warn("%s\n", args[i]);
	}

	if ((path = file_path(args[0])) != NULL) {

		if (execv(path, args) == -1) {

			perror("Comando erroneo");
			return 0;
		}
	} else {

		err(EXIT_FAILURE, "erro obteniendo direccion");
	}
	return 1;
}

void
set_var(char *var)
{

	char *saveptr;
	char *token;
	char *variable;

	token = strtok_r(var, "=", &saveptr);

	variable = malloc(strlen(token) + 1);

	strcpy(variable, token);
	token = strtok_r(NULL, "=", &saveptr);

	if (setenv(variable, token, 1) != 0) {

		perror("Error estableciendo la variable");
	}
	free(variable);
}

char *
get_var(char *var)
{

	var++;
	char *value = getenv(var);

	if (value == NULL) {

		fprintf(stderr, "Variable de entorno no encontrada: %s\n", var);
		return NULL;
	}
	return value;
}

int
array_counter(char **array)
{

	int i = 0;

	while (1) {

		if (array[i] == NULL) {

			break;
		}
		i++;
	}
	return i;
}

char **
token_line(char *line)
{

	char **tokens = malloc((MAX_ARGS + 3) * sizeof(char *));
	char *token;
	char *tmp;
	int index = 0;
	int error = 1;
	int i, x;

	if ((token = strtok(line, " \n\t")) != NULL) {

		tokens[index] = token;
		index++;
	}

	while ((token = strtok(NULL, " \n\t")) != NULL) {

		if (index >= MAX_ARGS) {

			warnx("Error: sobrepasado el numero de argumentos(11)");
			error = 0;
			break;
		}
		if (strcmp(token, "<") == 0 || strcmp(token, ">") == 0) {

			break;
		}
		if ((x = look_for_char(token, '<')) != -1
		    || (x = look_for_char(token, '>') != -1)) {

			token[x] = '\0';
		}

		tokens[index] = token;
		index++;
	}

	tokens[index] = NULL;

	for (i = 0; i < index; i++) {

		if (look_for_char(tokens[i], '$') != -1) {

			if ((tmp = get_var(tokens[i])) == NULL) {

				error = 0;
			}
			strcpy(tokens[i], tmp);
		}
	}

	if (error == 0) {

		free(tokens);
		return NULL;
	}
	return tokens;
}

void
pipes_maker(char **pipes, int bg, char **redirections)
{

	char **command;
	int index, i;
	int pipe1[2];
	int pipe2[2];
	pid_t pid;

	//char buffer[MAX_BUFFER_SIZE];

	if (pipe(pipe1) == -1) {

		perror("pipe fallo");
		return;
	}

	for (index = 0; index < array_counter(pipes); index++) {

		if (pipe(pipe2) == -1) {

			perror("pipe fallo");
			return;
		}
		if ((pid = fork()) == -1) {

			perror("fork fallo");
			return;
		}
		warn("1");
		if (pid < 0) {

			perror("Error al crear el proceso hijo");
			return;
		}
		if (pid == 0) {	//proceso hijo

			if (index < array_counter(pipes) - 1) {

				close(pipe2[0]);
				dup2(pipe2[1], STDOUT_FILENO);
				close(pipe2[1]);
			}
			if (index > 0) {

				close(pipe1[1]);	// Cerramos la escritura de la tubería anterior
				dup2(pipe1[0], STDIN_FILENO);	// Redirigir entrada estándar desde la tubería anterior
				close(pipe1[0]);
			}

			if (redirections[0] != NULL) {

				int fd =
				    open(file_path(redirections[0]), O_RDONLY);

				if (fd < 0) {

					perror
					    ("Error al abrir el archivo de entrada");
					exit(EXIT_FAILURE);
				}
				dup2(fd, STDIN_FILENO);
				close(fd);
			} else if (bg == 1 && index == 0) {

				int fd = open("/dev/null", O_RDONLY);

				if (fd == -1) {

					perror("open fallo");
					exit(EXIT_FAILURE);
				} else {

					dup2(fd, STDOUT_FILENO);
					dup2(fd, STDIN_FILENO);

					close(fd);
				}
			}
			//salida
			if (redirections[1] != NULL
			    && index >= array_counter(pipes) - 1) {

				int fd = open(file_path(redirections[1]),
					      O_WRONLY | O_CREAT | O_TRUNC,
					      0644);

				if (fd < 0) {

					perror
					    ("Error al abrir el archivo de salida");
					exit(EXIT_FAILURE);
				}
				dup2(fd, STDOUT_FILENO);
				close(fd);
			} else if (bg == 1 && index >= array_counter(pipes) - 1) {

				int fd = open("/dev/null", O_WRONLY);

				if (fd < 0) {

					perror("Error al abrir /dev/null");
					exit(EXIT_FAILURE);
				}
				dup2(fd, STDOUT_FILENO);
				close(fd);
			}

			command = token_line(pipes[index]);

			if (command != NULL) {

				if (builtin_command
				    (command, array_counter(command)) == 0) {

					if (execute_command
					    (command,
					     array_counter(command)) == 0) {

						warnx
						    ("Error: comando erroneo en pipe");
						exit(EXIT_FAILURE);
					}
				}
			}
		} else {
			if (index > 0) {

				close(pipe1[0]);
				close(pipe1[1]);
			}
			if (index < array_counter(pipes) - 1) {

				pipe1[0] = pipe2[0];
				pipe1[1] = pipe2[1];
			}
		}
	}
	close(pipe1[0]);
	close(pipe1[1]);

	if (bg == 0) {

		for (i = 0; i < array_counter(pipes); i++) {
			int status;

			if (wait(&status) == -1) {
				perror("Error esperando a los procesos hijos");
				return;
			}
		}
	}

}

void
control_line(char *line, char **arguments, int args_counter, int bg,
	     char **redirections)
{

	int error = 1;
	char *extr_pipe;
	char *saver;
	char *pipes[MAX_ARGS];
	int pipe_counter = 0;

	if (look_for_char(line, '|') != -1) {

		if ((extr_pipe = strtok_r(line, "|", &saver)) != NULL) {

			pipes[pipe_counter] = extr_pipe;
			pipe_counter++;
		}
		while ((extr_pipe = strtok_r(NULL, "|", &saver)) != NULL) {

			if (pipe_counter > MAX_ARGS) {

				warnx("Error: sobrepasado el numero de pipes");
				error = 0;
				break;
			}

			pipes[pipe_counter] = extr_pipe;
			pipe_counter++;
		}

		pipes[pipe_counter] = NULL;

		if (error == 1) {

			pipes_maker(pipes, bg, redirections);
		}
	} else {

		if (builtin_command(arguments, args_counter) == 0) {

			pid_t pid = fork();

			switch (pid) {

			case -1:

				perror("fork fallo");
				return;

			case 0:	// proceso hijo
				//salida en caso de redireccion
				if (redirections[1] != NULL) {

					int fd =
					    open(file_path(redirections[1]),
						 O_WRONLY | O_CREAT | O_TRUNC,
						 0644);

					if (fd < 0) {

						perror
						    ("Error al abrir el archivo de salida");
						exit(EXIT_FAILURE);
					}
					dup2(fd, STDOUT_FILENO);
					close(fd);
				} else if (bg == 1) {

					int fd = open("/dev/null", O_RDWR);

					if (fd == -1) {

						perror("open fallo");
						exit(EXIT_FAILURE);
					} else {

						dup2(fd, STDOUT_FILENO);
						dup2(fd, STDIN_FILENO);

						close(fd);
					}
				}
				//entrada en caso de redireccion
				if (redirections[0] != NULL) {

					int fd =
					    open(file_path(redirections[0]),
						 O_RDONLY);

					if (fd < 0) {

						perror
						    ("Error al abrir el archivo de entrada");
						exit(EXIT_FAILURE);
					}
					dup2(fd, STDIN_FILENO);
					close(fd);
				}

				execute_command(arguments, args_counter);

			default:	// proceso padre

				if (bg == 0) {

					int status;

					if (waitpid(pid, &status, 0) == -1) {

						perror("waitpid");
					}
				}
			}
		}
	}
}

//funcion para obtener la redirecciones
char **
get_redirectons(char *line, char *save_buffer)
{

	char **redirections = malloc(2 * sizeof(char *));;
	char *saveptr;
	char *token;
	char *redic;
	char *tmp;

	redirections[0] = NULL;
	redirections[1] = NULL;

	//me devuelve un puntero al parametro indicado
	if ((token = strrchr(save_buffer, '<')) != NULL) {

		//cojo el primer token despues de <
		redic = strtok_r(token + 1, " \n\t", &saveptr);

		if (look_for_char(redic, '$') != -1) {

			if ((tmp = get_var(redic)) == NULL) {

				redirections[0] = NULL;
			}
			strcpy(redic, tmp);
		}
		redirections[0] = redic;
	}

	if ((token = strrchr(line, '>')) != NULL) {

		//cojo el primer token despues de >
		redic = strtok_r(token + 1, " \n\t", &saveptr);

		if (look_for_char(redic, '$') != -1) {

			if ((tmp = get_var(redic)) == NULL) {

				redirections[1] = NULL;
			}
			strcpy(redic, tmp);
		}
		redirections[1] = redic;
	}

	return redirections;
}

char *
file_path(char *name)
{

	char *path;
	char *global_path;
	char *dir_path;
	char *saveptr;
	int size;

	size = MAX_SIZE + strlen(name);
	path = (char *)malloc(sizeof(char) * size);

	if (path == NULL) {

		err(EXIT_FAILURE, "malloc");
	}
	//si el nombre es la ruta completa
	if (access(name, F_OK) != -1) {

		free(path);
		path = strdup(name);
		return path;
	}

	strcpy(path, getenv("PWD"));
	strcat(path, "/");
	strcat(path, name);

	if (access(path, F_OK) != -1) {

		return path;
	}

	global_path = strdup(getenv("PATH"));
	dir_path = strtok_r(global_path, ":", &saveptr);

	while (dir_path != NULL) {

		if (strlen(dir_path) + strlen(name) + 2 <= size) {

			strcpy(path, dir_path);
			strcat(path, "/");
			strcat(path, name);

			if (access(path, F_OK) != -1) {

				free(global_path);
				return path;
			}
		} else {

			warnx("path demasiado larga");
		}

		dir_path = strtok_r(NULL, ":", &saveptr);
	}

	free(path);
	free(global_path);
	return NULL;
}

int
main(void)
{

	char *path;
	char *args[MAX_ARGS + 5];
	char **redirections;
	char *arg;
	char *ptr;
	int counter, error, x, bg;

	char buffer[MAX_BUFFER_SIZE];
	char *save_buffer;

	path = get_path();
	printf("user:%s$ ", path);

	free(path);

	while (fgets(buffer, sizeof(buffer), stdin) != NULL) {

		//reinicio el contador y el array
		counter = 0;
		error = 0;
		bg = 0;
		memset(args, '\0', sizeof(args));

		save_buffer = strdup(buffer);

		if ((x = look_for_char(save_buffer, '&')) != -1) {

			bg = 1;
			//elimino el &
			save_buffer[x] = '\0';
			buffer[x] = '\0';
		}

		redirections = get_redirectons(save_buffer, buffer);

		if ((arg = strtok_r(buffer, " \n", &ptr)) != NULL) {

			args[counter] = arg;
			counter++;
		}

		while ((arg = strtok_r(NULL, " \n", &ptr)) != NULL) {

			if (counter >= MAX_ARGS) {

				warnx
				    ("Error: sobrepasado el numero de argumentos(11)");
				error = 1;
				free(save_buffer);
				break;
			}
			if (strcmp(arg, "<") == 0 || strcmp(arg, ">") == 0) {

				break;
			}
			if ((x = look_for_char(arg, '<')) != -1
			    || (x = look_for_char(arg, '>') != -1)) {

				arg[x] = '\0';
			}

			args[counter] = arg;
			counter++;
		}

		args[counter] = NULL;

		if (error == 0) {

			control_line(save_buffer, args, counter, bg,
				     redirections);
		}

		path = get_path();
		printf("user:%s$ ", path);

		free(path);
		free(save_buffer);
		free(redirections);
	}

	exit(EXIT_SUCCESS);
}
