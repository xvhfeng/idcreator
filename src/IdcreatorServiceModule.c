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
 *       Filename:  IdcreatorServiceModule.c
 *        Created:  2015年03月03日 15时30分29秒
 *         Author:  Seapeak.Xu (www.94geek.com), xvhfeng@gmail.com
 *        Company:  Tencent Literature
 *         Remark:
 *
 ***********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <ev.h>

#include "spx_types.h"
#include "spx_io.h"
#include "spx_defs.h"
#include "spx_module.h"
#include "spx_job.h"
#include "spx_socket.h"

#include "IdcreatorNetworkModule.h"
#include "IdcreatorConfig.h"
#include "IdcreatorServerContext.h"
#include "IdcreatorServiceModule.h"

struct spx_module_context *gIdcreatorServiceModule = NULL;

void idcreatorServiceModuleListeningHandler(struct ev_loop *loop,ev_io *w,int revents){
    struct IdcreatorServerContext *tsc = NULL;
    size_t len = 0;
    struct spx_receive_context *rc = (struct spx_receive_context *) w;//magic,yeah
    err_t err= 0;
    err = spx_read_nb(w->fd,(byte_t *) &tsc,sizeof(tsc),&len);
    if(0 != err){
        SpxLog2(rc->log,SpxLogError,err,\
                "read the server context is fail."\
                "forced push tsc to pool.");
        return;
    }

    if (NULL == tsc) {
        SpxLog1(rc->log,SpxLogError,
                "read the server context is fail,"\
                "tne server context is null."\
                "forced push tsc to pool.");
        return;
    }

    tsc->dispatcher(loop,tsc->tc->idx,tsc);
    return ;
}


void idcreatorServiceModuleWakeupHandler(int revents,void *arg) {
    err_t err = 0;
    struct IdcreatorServerContext *tsc = (struct IdcreatorServerContext *) arg;
    if((revents & EV_TIMEOUT) || (revents & EV_ERROR)){
        SpxLog1(tsc->log,SpxLogError,\
                "wake up network module is fail.");
        idcreatorServerContextPoolPush(gIdcreatorServerContextPool,tsc);
        return;
    }
    if(revents & EV_WRITE){
        size_t len = 0;
        SpxLogFmt1(tsc->log,SpxLogDebug,"wakeup thread:%d.",tsc->tc->idx );
        err = spx_write_nb(tsc->tc->pipe[1],(byte_t *) &tsc,sizeof(tsc),&len);
        if (0 != err) {
            SpxLog2(tsc->log,SpxLogError,err,
                    "wake up network module is fail.");
            idcreatorServerContextPoolPush(gIdcreatorServerContextPool,tsc);
        }
    }
    return;
}

