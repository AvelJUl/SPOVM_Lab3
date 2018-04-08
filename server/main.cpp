#include <ncurses.h>
#include <csignal>
#include <sys/types.h>
#include <sys/shm.h>
#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <wait.h>
#include <cmath>

#define SHM_ID	2007        /* уникальный ключ разделяемой памяти */
#define PERMS	0666        /* права доступа */

#define WIDTH       38      /* ширина окна клиент-приложения */
#define HEIGHT      22      /* высота окна клиент-приложения */
#define OFFSET_X    1       /* смещение по оси X окна клиент-приложения*/
#define OFFSET_Y    1       /* смещение по оси Y окна клиент-приложения*/

/* коды сообщений */

#define MSG_TYPE_STRING     0   /* тип сообщения о том, что переданная строка не имеет продолжения */
#define MSG_TYPE_CONTINUE   1   /* тип сообщения о том, что переданная строка имеет продолжение */
#define MSG_TYPE_FINISH     2   /* тип сообщения о том, что обмен строками завершен */

#define MAX_SIZE	30          /* максимальная длина сообщения */

/* Функция создания окна.*/
WINDOW *newWindow();

/* Функция удаления окна. */
void deleteWindow(WINDOW *win);

/* структура сообщения, помещаемого в разделяемую память */
typedef struct {
    int type;
    char string [MAX_SIZE];
} message_t;

int main() {
    char *const argv[] = {NULL};
    /* Идентификатор окна. */
    WINDOW *win;
    /* Идентификатор клиент-процесса.*/
    pid_t client;
    /* Идентификатор сегмента разделяемой памяти */
    int shmid;
    /* Идентификатор группы сигналов.*/
    sigset_t signalFromClientProgram;
    /* Идентификатор статуса.*/
    int status;
    /* Идентификатор структуры сообщений.*/
    message_t *message;
    /* Буферная строка, в которую производится ввод сообщений*/
    char buffer[3 * MAX_SIZE];
    /*
     * Переход в ncurses-режим с очисткой
     * и обновлением экрана.
     */
    initscr();
    clear();
    refresh();
    /* Создание нового окна с сответствующими аттрибутами.*/
    win = newWindow();
    wrefresh(win);
    /*
     * Создание и инициализация группы сигналов,
     * используемых для общения с клиент-приложением.
     */
    sigemptyset(&signalFromClientProgram);
    sigaddset(&signalFromClientProgram, SIGUSR2);
    sigprocmask(SIG_BLOCK, &signalFromClientProgram, NULL);

    /*
     * Создание сегмента разделяемой памяти
     * с уникальным ключем SHM_ID, максимальным
     * размером выделяемой области sizeof(message_t)
     * байт, и флагом доступа PERMS | IPC_CREAT.
     */
    shmid = shmget(SHM_ID,
                   sizeof (message_t),
                   PERMS | IPC_CREAT);
    if (shmid < 0) {
        mvwaddstr(win, 2, 1, "Can not create shared memory segment!");
        wrefresh(win);
    }
    /*
     * Подключение сегмента c  к адресному пространству
     * процесса, система сама выбирает подходящий
     * (неиспользуемый) адрес, над сегментом возможно
     * выполнение операций чтения и записи.
     */
    if ((message = (message_t *) shmat(shmid,
                                      NULL,
                                      0)) == NULL){
        mvwaddstr(win, 3, 1, "Shared memory attach error!");
        wrefresh(win);
    }
    /* Запуск клиент-приложения. */
    client = fork();
    /*
     * В случае успешного создания и открытия клиент-приложения
     * запускается цикл обмена сообщениями с клиент-приложением:
     * сперва ожидается сигнал от клиент-приложения о готовности
     * принимать сообщения, после чего выводится приглашение к
     * вводу сообщения, вводится само сообщения, после чего
     * анализируется его тип и размещается в сегменте разделяемой
     * памяти, далее передается сигнал клиент-приложению, что сообщение
     * готово к прочтению; цикл повторяется снова.
     */
    switch(client){
        case 0:
            if (execv("./Client", argv) < 0){
                mvwaddstr(win, 4, 1,"Launching the client application error");
                wrefresh(win);
                sleep(7);
                deleteWindow(win);
                refresh();
                endwin();
                exit(1);
            }
            break;
        case -1:
            wclear(win);
            mvwaddstr(win, 1, 1, "Error!");
            break;
        default:
            while (1) {
                sigwait(&signalFromClientProgram, &status);
                deleteWindow(win);
                win = newWindow();
                mvwaddstr(win, 2, 1, "Please, input message: (q - quit)");
                mvwgetstr(win, 3, 1, buffer);
                wrefresh(win);

                if ((strcmp(buffer, "q")) == 0){
                    message->type = MSG_TYPE_FINISH;
                    kill(client, SIGUSR1);
                    waitpid(client, &status, 0);
                    break;
                } else {
                    while(1) {
                        message->type = MSG_TYPE_STRING;
                        strncpy(message->string, buffer, MAX_SIZE);
                        if ((int)strlen(buffer) <= 30){
                            kill(client, SIGUSR1);
                            break;
                        } else {
                            message->type = MSG_TYPE_CONTINUE;
                            kill(client, SIGUSR1);
                            sigwait(&signalFromClientProgram, &status);
                            strcpy(buffer, buffer + MAX_SIZE);
                        }
                    }
                }
            }
            break;
    }

    /*
     * Удаление сегмента разделяемой памяти: отсоединение
     * сегмента, удаление из системы идентификатор shmid,
     * ликвидация разделяемого сегмента памяти и ассоции-
     * рованной с ним структуры данных.
     */
    shmdt (message);
    if (shmctl(shmid,
               IPC_RMID,
               (shmid_ds*) 0) < 0){
        deleteWindow(win);
        win = newWindow();
        mvwaddstr(win, 2, 1, "Shared memory remove error!");
        wrefresh(win);
        sleep(5);
        deleteWindow(win);
    }

    /*
     * Очистка содержимого терминала и выход из
     * ncurses-режима.
     */
    clear();
    refresh();
    endwin();
    exit (0);
}

WINDOW* newWindow(){
    WINDOW *win = newwin(HEIGHT, WIDTH, OFFSET_Y, OFFSET_X);
    halfdelay(1);
    getch();
    box(win, 0, 0);
    mvwaddstr(win, 1, 1, "Server");
    wrefresh(win);
    return win;
}

void deleteWindow(WINDOW *win){
    wclear(win);
    wrefresh(win);
    delwin(win);
}
