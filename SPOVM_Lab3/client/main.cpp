#include <ncurses.h>
#include <csignal>
#include <sys/types.h>
#include <sys/shm.h>
#include <cstdlib>
#include <unistd.h>

#define SHM_ID	2007            /* уникальный ключ разделяемой памяти */

#define WIDTH       38          /* ширина окна клиент-приложения */
#define HEIGHT      22          /* высота окна клиент-приложения */
#define OFFSET_X    41          /* смещение по оси X окна клиент-приложения*/
#define OFFSET_Y    1           /* смещение по оси Y окна клиент-приложения*/

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

int main()
{
    /* Идентификатор окна. */
    WINDOW *win;
    /* Идентификатор группы сигналов.*/
    sigset_t signalFromServerProgram;
    /* Идентификатор сегмента разделяемой памяти */
    int shmid;
    /* Идентификатор статуса.*/
    int status;
    /* Идентификатор стуктуры сообщений*/
    message_t *message;
    /*
     * Создание и инициализация группы сигналов,
     * используемых для общения с сервер-приложением.
     */
    sigemptyset(&signalFromServerProgram);
    sigaddset(&signalFromServerProgram, SIGUSR1);
    sigprocmask(SIG_BLOCK, &signalFromServerProgram, NULL);
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
    /* получение доступа к сегменту разделяемой памяти */
    if ((shmid = shmget (SHM_ID,
                         sizeof (message_t),
                         0)) < 0){
        mvwaddstr(win, 2, 1, "Can not get shared memory segment!");
        wrefresh(win);
    }
    /* получение адреса сегмента */
    if ((message = (message_t*) shmat(shmid, 0, 0)) == NULL){
        mvwaddstr(win, 3, 1, "Shared memory attach error!");
        wrefresh(win);
    }
    /*
     * Цикл обмена информацией с сервер-приложением:
     * подается сигнал сервер-приложению о том, что
     * клиент-приложение готово принимать сообщения,
     * ожидается ответный сигнал о том, что сообщение
     * было помещено в сегмент разделяемой памяти и в
     * зависимости от типа сообщения выводится информация
     * о его типе, а затем непосредственно текст сообщения;
     * и цикл снова повторяется.
     */
    while (1) {
        kill(getppid(), SIGUSR2);
        sigwait(&signalFromServerProgram, &status);
        deleteWindow(win);
        win = newWindow();

        switch (message->type){
            case MSG_TYPE_STRING:
                mvwaddstr(win, 2, 1, "Message:");
                break;
            case MSG_TYPE_CONTINUE:
                mvwaddstr(win, 2, 1, "Message(to be continued):");
                break;
            case MSG_TYPE_FINISH:
                mvwaddstr(win, 2, 1, "Shutdown...");
                wrefresh(win);
                sleep(5);
                deleteWindow(win);
                endwin();
                exit(0);
            default:
                break;
        }
        mvwaddstr(win, 3, 1, message->string);
        wrefresh(win);
        sleep(5);
    }
}

WINDOW* newWindow(){
    WINDOW *win = newwin(HEIGHT, WIDTH, OFFSET_Y, OFFSET_X);
    halfdelay(1);
    getch();
    box(win, 0, 0);
    mvwaddstr(win, 1, 1, "Client");
    wrefresh(win);
    return win;
}

void deleteWindow(WINDOW *win){
    wclear(win);
    wrefresh(win);
    delwin(win);
}
