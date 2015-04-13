/*************************************************************
 *                     _ooOoo_
 *                    o8888888o
 *                    88" . "88
 *                    (| -_- |)
 *                    O\  =  /O
 *                 ____/`---'\____
 *               .'  \\|     |//  `.
 *              /  \\|||  :  |||//  \
 *             /  _||||| -:- |||||-  \
 *             |   | \\\  -  /// |   |
 *             | \_|  ''\---/''  |   |
 *             \  .-\__  `-`  ___/-. /
 *           ___`. .'  /--.--\  `. . __
 *        ."" '<  `.___\_<|>_/___.'  >'"".
 *       | | :  `- \`.;`\ _ /`;.`/ - ` : | |
 *       \  \ `-.   \_ __\ /__ _/   .-` /  /
 *  ======`-.____`-.___\_____/___.-`____.-'======
 *                     `=---='
 *  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 *           佛祖保佑       永无BUG
 *
 * ==========================================================================
 *
 * this software or lib may be copied only under the terms of the gnu general
 * public license v3, which may be found in the source kit.
 *
 *       Filename:  IdcreatorTcpAccept.c
 *        Created:  2015年03月03日 11时27分35秒
 *         Author:  Seapeak.Xu (www.94geek.com), xvhfeng@gmail.com
 *        Company:  Tencent Literature
 *         Remark:
 *
 ***********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ev.h>

#include "spx_types.h"
#include "spx_string.h"
#include "spx_defs.h"
#include "spx_socket.h"
#include "spx_module.h"
#include "spx_job.h"
#include "spx_time.h"
#include "spx_alloc.h"
#include "spx_io.h"

#include "IdcreatorTcpAccept.h"
#include "IdcreatorNetworkModule.h"
#include "IdcreatorConfig.h"
#include "IdcreatorServerContext.h"

struct mainsocket_thread_arg{
    SpxLogDelegate *log;
    struct IdcreatorConfig *c;
};

spx_private struct ev_loop *main_socket_loop = NULL;
spx_private ev_io main_watcher;

spx_private void *idcreatorTcpCreate(void *arg);
spx_private void idcreatorTcpAcceptHandler(struct ev_loop *loop,
        ev_io *watcher,int revents);
spx_private void idcreatorTcpAccept(SpxLogDelegate *log,struct ev_loop *loop,int fd);

spx_private void idcreatorTcpAccept(SpxLogDelegate *log,struct ev_loop *loop,int fd){/*{{{*/
    SpxZero(main_watcher);
    ev_io_init(&main_watcher,idcreatorTcpAcceptHandler,fd,EV_READ);
    main_watcher.data = log;
     ev_io_start(loop,&(main_watcher));
    ev_run(loop,0);
}/*}}}*/

spx_private void idcreatorTcpAcceptHandler(struct ev_loop *loop,ev_io *watcher,int revents){/*{{{*/
    ev_io_stop(loop,watcher);
    SpxLogDelegate *log = (SpxLogDelegate *) watcher->data;
    err_t err = 0;
    while(true){
        struct sockaddr_in client_addr;
        unsigned int socket_len = 0;
        int client_sock = 0;
        socket_len = sizeof(struct sockaddr_in);
        client_sock = accept(watcher->fd, (struct sockaddr *) &client_addr,
                &socket_len);
        if (0 > client_sock) {
            if (EWOULDBLOCK == errno || EAGAIN == errno) {
                break;
            }
            break;
        }

        if (0 == client_sock) {
            break;
        }

        struct IdcreatorServerContext *isc = idcreatorServerContextPoolPop(gIdcreatorServerContextPool,&err);
        if(!isc) {
            SpxClose(client_sock);
            SpxLog1(log,SpxLogError,\
                    "pop isc context is fail.");
            break;
        }

        size_t idx = client_sock % gIdcreatorNetworkModule->threadpool->size;
        struct spx_thread_context * tc = spx_get_thread(gIdcreatorNetworkModule,idx);
        isc->fd = client_sock;
        isc->tc = tc;
        isc->moore = IdcreatorMooreIn;

        SpxLogFmt1(log,SpxLogDebug,"recv socket conntect.wakeup notifier module idx:%d.jc idx:%d."
                ,idx,isc->idx);
        SpxModuleDispatch(idcreatorNetworkModuleWakeupHandler,isc);
    }
    ev_io_start(loop,watcher);
}/*}}}*/

spx_private void *idcreatorTcpCreate(void *arg){/*{{{*/
    struct mainsocket_thread_arg *mainsocket_arg = (struct mainsocket_thread_arg *) arg;
    SpxLogDelegate *log = mainsocket_arg->log;
    struct IdcreatorConfig *c= mainsocket_arg->c;
    SpxFree(mainsocket_arg);
    err_t err = 0;
    main_socket_loop = ev_loop_new(0);
    if(NULL == main_socket_loop){
        SpxLog2(log,SpxLogError,err,"create main socket loop is fail.");
        return NULL;
    }
    int mainsocket =  spx_socket_new(&err);
    if(0 == mainsocket){
        SpxLog2(log,SpxLogError,err,"create main socket is fail.");
        return NULL;
    }

    if(0!= (err = spx_set_nb(mainsocket))){
        SpxLog2(log,SpxLogError,err,"set main socket nonblock is fail.");
        goto r1;
    }

    if(0 != (err =  spx_socket_start(mainsocket,c->ip,c->port,\
                    true,c->timeout,\
                    3,c->timeout,\
                    false,0,\
                    true,\
                    true,c->timeout,
                    1024))){
        SpxLog2(log,SpxLogError,err,"start main socket is fail.");
        goto r1;
    }

    SpxLogFmt1(log,SpxLogMark,
            "main socket fd:%d."
            "and accepting...",
            mainsocket);

    idcreatorTcpAccept(c->log,main_socket_loop,mainsocket);
r1:
    SpxClose(mainsocket);
    return NULL;
}/*}}}*/


pthread_t idcreatorMainTcpThreadNew(SpxLogDelegate *log,struct IdcreatorConfig *c,err_t *err) {/*{{{*/
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    size_t ostack_size = 0;
    pthread_attr_getstacksize(&attr, &ostack_size);
    if (ostack_size != c->stackSize
            && (0 != (*err = pthread_attr_setstacksize(&attr,c->stackSize)))){
        return 0;
    }
    struct mainsocket_thread_arg *arg =(struct mainsocket_thread_arg *) spx_alloc_alone(sizeof(*arg),err);
    if(NULL == arg){
        pthread_attr_destroy(&attr);
        return 0;
    }
    arg->log = log;
    arg->c = c;

    pthread_t tid = 0;
    if (0 !=(*err =  pthread_create(&tid, &attr, idcreatorTcpCreate,
                    arg))){
        pthread_attr_destroy(&attr);
        SpxFree(arg);
        return 0;
    }
    pthread_attr_destroy(&attr);
    return tid;
}/*}}}*/

