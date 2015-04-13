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
 *       Filename:  idcreatorDispatcher.c
 *        Created:  2015年03月03日 15时38分56秒
 *         Author:  Seapeak.Xu (www.94geek.com), xvhfeng@gmail.com
 *        Company:  Tencent Literature
 *         Remark:
 *
 ***********************************************************************/

#include <stdlib.h>
#include <stdio.h>

#include "spx_types.h"
#include "spx_string.h"
#include "spx_defs.h"
#include "spx_log.h"
#include "spx_module.h"
#include "spx_alloc.h"
#include "spx_periodic.h"
#include "spx_atomic.h"
#include "spx_time.h"

#include "IdcreatorConfig.h"
#include "IdcreatorServerContext.h"
#include "IdcreatorNetworkModule.h"
#include "IdcreatorProtocol.h"


static u32_t seed = 0;
static u64_t sum = 0;

void idcreatorDispatcherHandler(struct ev_loop *loop,int tidx,struct IdcreatorServerContext *isc) {
    err_t err = 0;
    u64_t id = 0l;
    int type = 0;
    int next = 0;
    time_t token = spx_get_token();
    do{
        if(0x3FF <= seed)
            SpxAtomicVCas(seed,0x3ff,0);
        next = SpxAtomicLazyVIncr(seed);
    } while(0x3FF <= seed);

    type = spx_msg_b2i((uchar_t *) isc->inbuf + SpxMsgHeaderSize);

    SpxTypeConvert2(struct IdcreatorConfig,c,isc->c);

    int mid = c->mid;
    id = (u64_t) (((u64_t) (0xFFFFFFFF & token) << 32)
        | (((u64_t) (0x3FF & next)) << 22)
        | (((u64_t) (0x3FFF & type)) << 8)
        | ((u64_t) (0xFF & mid)));

//    printf("%lu \n",id);

    SpxAtomicVIncr(sum);
    printf("%lu \n",sum);
    isc->outheader.version = IdcreatorVersion;
    isc->outheader.protocol = IdcreatorMakeId;
    isc->outheader.bodylen = sizeof(u64_t);

    spx_header_pack(isc->outbuf,&(isc->outheader));
    spx_msg_ul2b((uchar_t *) isc->outbuf + SpxMsgHeaderSize,id);

    size_t idx = isc->fd % gIdcreatorNetworkModule->threadpool->size;
    struct spx_thread_context * tc = spx_get_thread(gIdcreatorNetworkModule,idx);
    isc->tc = tc;
    isc->moore = IdcreatorMooreOut;
    SpxModuleDispatch(idcreatorNetworkModuleWakeupHandler,isc);
    return;
}




