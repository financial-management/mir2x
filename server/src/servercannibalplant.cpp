/*
 * =====================================================================================
 *
 *       Filename: servercannibalplant.cpp
 *        Created: 04/07/2016 03:48:41 AM
 *    Description:
 *
 *        Version: 1.0
 *       Revision: none
 *       Compiler: gcc
 *
 *         Author: ANHONG
 *          Email: anhonghe@gmail.com
 *   Organization: USTC
 *
 * =====================================================================================
 */

#include <optional>
#include "mathf.hpp"
#include "dbcomid.hpp"
#include "raiitimer.hpp"
#include "servercannibalplant.hpp"

corof::eval_poller ServerCannibalPlant::updateCoroFunc()
{
    uint64_t targetUID = 0;
    std::optional<uint64_t> idleTime;

    while(m_sdHealth.hp > 0){
        if(targetUID && !(co_await coro_validTarget(targetUID))){
            targetUID = 0;
        }

        if(!targetUID){
            targetUID = co_await coro_pickTarget();
        }

        if(targetUID){
            idleTime.reset();
            setStandMode(true);
            co_await coro_attackUID(targetUID, DBCOM_MAGICID(u8"物理攻击"));
        }
        else if(!idleTime.has_value()){
            idleTime = hres_tstamp().to_sec();
        }
        else if(hres_tstamp().to_sec() - idleTime.value() > 30ULL){
            setStandMode(false);
        }
        co_await corof::async_wait(200);
    }

    goDie();
    co_return true;
}
