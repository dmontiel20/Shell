# Custom UNIX Shell in C (`myshell`)

[![Language: C](https://img.shields.io/badge/Language-C99-00599C.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![POSIX Compliant](https://img.shields.io/badge/API-POSIX%20Syscalls-blue.svg)](https://pubs.opengroup.org/onlinepubs/9699919799/)
[![Memory Safety: Valgrind](https://img.shields.io/badge/Tested%20with-Valgrind-brightgreen.svg)](https://valgrind.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

Implementación en lenguaje **C** de un intérprete de comandos (**Shell estilo UNIX**) desarrollado a bajo nivel mediante llamadas al sistema POSIX (`fork`, `execv`, `pipe`, `dup2`, `waitpid`).

El proyecto aborda los principios fundamentales de los sistemas operativos: gestión de procesos concurrentes, sincronización padre-hijo, resolución de rutas en el entorno, tuberías encadenadas (*pipelines* multiproceso), redirecciones de I/O y control de ejecución en segundo plano (*background*).

---

## 📌 Tabla de Contenidos
- [Características Principales](#-características-principales)
- [Arquitectura Interna](#-arquitectura-interna)
- [Llamadas al Sistema y Mecanismos Clave](#-llamadas-al-sistema-y-mecanismos-clave)
- [Estructura del Proyecto](#-estructura-del-proyecto)
- [Compilación y Ejecución](#-compilación-y-ejecución)
- [Ejemplos de Uso](#-ejemplos-de-uso)
- [Límites del Sistema](#-límites-del-sistema)

---

## 🚀 Características Principales

- **Ejecución de binarios externos:** Búsqueda dinámica de ejecutables en el directorio de trabajo local (`PWD`) y a través de los directorios definidos en la variable de entorno `PATH`.
- **Comandos internos (Built-in commands):**
  - `cd [dir]`: Cambio de directorio de trabajo soportando rutas absolutas, relativas, expansión de variables (p. ej. `cd $HOME`) y retorno por defecto a `$HOME`.
  - `VAR=valor`: Definición y sobreescritura de variables de entorno mediante `setenv()`.
  - `$VAR`: Expansión de variables de entorno dentro de argumentos y rutas.
- **Tuberías anónimas concurrentes (`|`):** Creación de pipelines de múltiples etapas comunicando la salida estándar (`STDOUT`) de cada comando con la entrada estándar (`STDIN`) del siguiente.
- **Redirecciones de I/O:**
  - Redirección de entrada estándar (`< archivo`): Lectura desde fichero descriptor vía `open()` y `dup2()`.
  - Redirección de salida estándar (`> archivo`): Escritura y truncado seguro de ficheros (`O_WRONLY | O_CREAT | O_TRUNC`).
- **Ejecución en segundo plano (`&`):** Desacoplamiento de procesos para ejecución asíncrona redirigiendo descriptores a `/dev/null` para evitar el bloqueo del prompt interactivo.

---

## 📐 Arquitectura Interna

```
                     ┌───────────────────────────────┐
                     │         Bucle REPL            │
                     │  (Prompt con getcwd() / fgets)│
                     └───────────────┬───────────────┘
                                     │
                        save_buffer / buffer
                                     │
           ┌─────────────────────────┼─────────────────────────┐
           ▼                         ▼                         ▼
   Detección de '&'          Detección de '<' / '>'     Tokenización de args
   (Flag de Background)      (get_redirectons())        (token_line / strtok_r)
           │                         │                         │
           └─────────────────────────┼─────────────────────────┘
                                     │
                                     ▼
                        ┌─────────────────────────┐
                        │     control_line()      │
                        └────────────┬────────────┘
                                     │
               ┌─────────────────────┴─────────────────────┐
               ▼                                           ▼
      ¿Contiene pipes '|'?                         Comando simple
      ┌───────────────────────────┐               ┌───────────────────────────┐
      │      pipes_maker()        │               │   builtin_command()?      │
      │  - Creación de N procesos │               │   - Sí: cd, VAR=val       │
      │  - Encadenamiento pipe()  │               │   - No: fork()            │
      │  - Redirección con dup2() │               │         dup2() (I/O)      │
      │  - execv() en cada hijo   │               │         execute_command() │
      └───────────────────────────┘               └───────────────────────────┘
```

---

## ⚙️ Llamadas al Sistema y Mecanismos Clave

| Syscall / Función C | Propósito en el Proyecto |
| :--- | :--- |
| `fork()` | Creación de procesos hijos aislados para evitar que `execv` sobrescriba el proceso de la shell. |
| `execv(path, argv)` | Sustitución de la imagen del proceso hijo por el binario a ejecutar. |
| `pipe(fd[2])` | Creación de canales unidireccionales de comunicación IPC entre procesos del pipeline. |
| `dup2(oldfd, newfd)` | Clonación y reasignación de descriptores de fichero para redirigir `STDIN_FILENO` y `STDOUT_FILENO`. |
| `wait()` / `waitpid()` | Sincronización del proceso padre para esperar la finalización de los comandos en primer plano (*foreground*). |
| `chdir(path)` | Cambio del directorio de trabajo actual del proceso padre para el comando `cd`. |
| `setenv()` / `getenv()` | Modificación y lectura directa de la tabla de variables de entorno del proceso. |

---

## 📂 Estructura del Proyecto

```text
.
├── shell.c       # Código fuente completo de la shell
├── Makefile      # (Opcional) Reglas de compilación y limpieza
└── README.md     # Documentación técnica del proyecto
```

---

## 🛠️ Compilación y Ejecución

### Requisitos
- Compilador de C compatible con C99 o superior (`gcc` o `clang`).
- Sistema operativo basado en UNIX (Linux, macOS, WSL).

### Compilación Manual
Se recomienda compilar con flags estrictos para advertencias y depuración:

```bash
# Compilar código objeto
gcc -Wall -Wshadow -Wvla -g -c shell.c

# Enlazar y generar el ejecutable
gcc -g -o shell shell.o
```

### Ejecución
```bash
./shell
```

### Análisis de Fugas de Memoria (Valgrind)
Para auditar la gestión dinámica de memoria (`malloc`, `strdup`, `free`):

```bash
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./shell
```

---

## 💻 Ejemplos de Uso

Una vez dentro de la shell (`user:/ruta/actual$ `):

### 1. Comandos Básicos y Variables de Entorno
```bash
user:/home/user$ MI_VARIABLE=hola_mundo
user:/home/user$ echo $MI_VARIABLE
hola_mundo
user:/home/user$ cd /tmp
user:/tmp$ cd $HOME
user:/home/user$ 
```

### 2. Redirecciones de Entrada y Salida
```bash
# Redirigir la salida a un archivo
user:/home/user$ ls -l > salida.txt

# Redirigir la entrada desde un archivo
user:/home/user$ grep shell < salida.txt
```

### 3. Tuberías Encadenadas (Pipes)
```bash
# Pipeline de dos comandos
user:/home/user$ cat /etc/passwd | grep bash

# Pipeline múltiple
user:/home/user$ ls -la | grep txt | wc -l
```

### 4. Ejecución en Segundo Plano
```bash
# Ejecutar sin bloquear el prompt interactivo
user:/home/user$ sleep 5 &
user:/home/user$ ls
```

---

## ⚠️ Límites del Sistema

- **Argumentos máximos (`MAX_ARGS`):** `11` argumentos por comando.
- **Tamaño máximo de buffer (`MAX_BUFFER_SIZE`):** `1024` bytes por línea de entrada.
- **Tamaño base de rutas (`MAX_SIZE`):** `60` caracteres base para concatenación de directorios.
