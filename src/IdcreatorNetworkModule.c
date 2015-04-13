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
 *       Filename:  idcreatorNetworkModule.c
 *        Created:  2015年03月03日 11时39分20秒
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
#include "spx_io.h"
#include "spx_alloc.h"

#include "IdcreatorServiceModule.h"
#include "IdcreatorServerContext.h"
#include "IdcreatorNetworkModule.h"
#include "IdcreatorConfig.h"

struct spx_module_context *gIdcreatorNetworkModule = NULL;

void idcreatorNetworkModuleListeningHandler(struct ev_loop *loop,ev_io *w,int revents){/*{{{*/
    struct IdcreatorServerContext *isc = NULL;
    size_t len = 0;
    struct spx_receive_context *rc = (struct spx_receive_context *) w;//magic,yeah
    err_t err= 0;
    err = spx_read_nb(w->fd,(byte_t *) &isc,sizeof(isc),&len);
    if(0 != err){
        SpxLog2(rc->log,SpxLogError,err,\
                "read server context is fail.");
        return;
    }

    if (NULL == isc) {
        SpxLog1(rc->log,SpxLogError,
                "read the service context is fail,"
                "tne nio context is null.");
        return;
    }

    SpxTypeConvert2(struct IdcreatorConfig,c,isc->c);

    switch(isc->moore){
        case IdcreatorMooreIn :
            {
                ev_once(loop,isc->fd,EV_READ,(ev_tstamp) c->waitting,isc->inHandler,(void *) isc);
                break;
            }
        case IdcreatorMooreOut :
            {
                idcreatorNetworkSenderHandler(EV_WRITE,isc);
                break;
            }
        case IdcreatorMooreNormal:
        default:
            {
                SpxLog1(isc->log,SpxLogError,\
                        "the isc moore is normal,and no the handler."\
                        "forced push isc to pool.");
                goto r1;
            }
    }
    return ;
r1:
    idcreatorServerContextPoolPush(gIdcreatorServerContextPool,isc);
    return ;
}/*}}}*/

void idcreatorNetworkModuleWakeupHandler(int revents,void *arg) {/*{{{*/
    err_t err = 0;
    struct IdcreatorServerContext *isc = (struct IdcreatorServerContext *) arg;
    if((revents & EV_TIMEOUT) || (revents & EV_ERROR)){
        SpxLog1(isc->log,SpxLogError,\
                "wake up network module is fail.");
        idcreatorServerContextPoolPush(gIdcreatorServerContextPool,isc);
        return;
    }
    if(revents & EV_WRITE){
        size_t len = 0;
        SpxLogFmt1(isc->log,SpxLogDebug,"wakeup thread:%d.",isc->tc->idx );
        err = spx_write_nb(isc->tc->pipe[1],(byte_t *) &isc,sizeof(isc),&len);
        if (0 != err) {
            SpxLog2(isc->log,SpxLogError,err,
                    "wake up network module is fail.");
            idcreatorServerContextPoolPush(gIdcreatorServerContextPool,isc);
        }
    }

}/*}}}*/

void idcreatorNetworkReceiverHandler(int revents,void *arg) {/*{{{*/
    SpxTypeConvert2(struct IdcreatorServerContext,isc,arg);
    if(NULL == isc){
        return;
    }
    SpxTypeConvert2(struct IdcreatorConfig,c,isc->c);
    if(EV_TIMEOUT & revents){
        SpxLog1(isc->log,SpxLogError,\
                "waitting read from client is timeout."
                "and then push the server context to pool.");
        idcreatorServerContextPoolPush(gIdcreatorServerContextPool,isc);
        return;
    }
    err_t err = 0;

    if(EV_READ & revents){
        size_t len = 0;
        err = spx_read_ack(isc->fd,
                (byte_t *) isc->inbuf + isc->offset, isc->inlen,&len);
        if(err) {
            if(EAGAIN == err || EWOULDBLOCK == err || EINTR == err){
                isc->offset += len;
                ev_once(isc->tc->loop,isc->fd,EV_READ,c->waitting,isc->inHandler,(void *) isc);
                return;
            } else {
                SpxLogFmt2(isc->log,SpxLogError,err,
                        "read body is fail.bodylen:%d,recvlen:%d."
                        "and then push the server context to pool.",
                        isc->inlen,isc->offset);
                idcreatorServerContextPoolPush(gIdcreatorServerContextPool,isc);
                return;
            }
        }

        size_t idx = isc->fd % gIdcreatorServiceModule->threadpool->size;
        struct spx_thread_context * tc = spx_get_thread(gIdcreatorServiceModule,idx);

        isc->tc = tc;
        SpxModuleDispatch(idcreatorServiceModuleWakeupHandler,isc);
        return;
    }
    return;
}/*}}}*/

void idcreatorNetworkSenderHandler(int revents,void *arg) {/*{{{*/
    SpxTypeConvert2(struct IdcreatorServerContext,isc,arg);
    if(NULL == isc){
        return;
    }
    SpxTypeConvert2(struct IdcreatorConfig,c,isc->c);
    if(EV_TIMEOUT & revents){
        SpxLogFmt1(isc->log,SpxLogError,
                "wait for writing to client is fail."
                "bodylen:%d,writed len:%d."
                "and then push the server context to pool.",
                isc->outlen,isc->offset);
        idcreatorServerContextPoolPush(gIdcreatorServerContextPool,isc);
        return;
    }
    err_t err = 0;

    if(EV_WRITE & revents){
        size_t len = 0;
        err = spx_write_ack(isc->fd,
                (byte_t *) isc->outbuf + isc->offset, isc->outlen,&len);
        if(err) {
            if(EAGAIN == err || EWOULDBLOCK == err || EINTR == err){
                isc->offset += len;
                ev_once(isc->tc->loop,isc->fd,EV_WRITE,c->waitting,isc->outHandler,(void *) isc);
                return;
            } else {
                SpxLogFmt2(isc->log,SpxLogError,err,
                        "wait for writing to client is fail."
                        "bodylen:%d,writed len:%d."
                        "and then push the server context to pool.",
                        isc->outlen,isc->offset);
                idcreatorServerContextPoolPush(gIdcreatorServerContextPool,isc);
                return;
            }
        }
        idcreatorServerContextPoolPush(gIdcreatorServerContextPool,isc);
    }
    return;
}/*}}}*/


