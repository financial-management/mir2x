/*
 * =====================================================================================
 *
 *       Filename: servicecoreop.cpp
 *        Created: 05/03/2016 21:29:58
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
#include <string>
#include <type_traits>

#include "player.hpp"
#include "actorpod.hpp"
#include "netdriver.hpp"
#include "monoserver.hpp"
#include "servicecore.hpp"

extern NetDriver *g_netDriver;
extern MonoServer *g_monoServer;

// ServiceCore accepts net packages from *many* sessions and based on it to create
// the player object for a one to one map
//
// So servicecore <-> session is 1 to N, means we have to put put pointer of session
// in the net package otherwise we can't find the session even we have session's
// address, session is a sync-driver, even we have it's address we can't find it
//
void ServiceCore::on_AM_RECVPACKAGE(const ActorMsgPack &mpk)
{
    /* const */ auto amRP = mpk.conv<AMRecvPackage>();
    operateNet(amRP.channID, amRP.package.type, amRP.package.buf(), amRP.package.size);
    freeActorDataPackage(&(amRP.package));
}

void ServiceCore::on_AM_METRONOME(const ActorMsgPack &)
{
}

void ServiceCore::on_AM_ADDCO(const ActorMsgPack &rstMPK)
{
    const auto amACO = rstMPK.conv<AMAddCharObject>();
    if(!amACO.mapID){
        m_actorPod->forward(rstMPK.from(), AM_ERROR, rstMPK.seqID());
        return;
    }

    auto pMap = retrieveMap(amACO.mapID);

    if(!pMap){
        m_actorPod->forward(rstMPK.from(), AM_ERROR, rstMPK.seqID());
        return;
    }

    if(!pMap->in(amACO.mapID, amACO.x, amACO.y) && amACO.strictLoc){
        m_actorPod->forward(rstMPK.from(), AM_ERROR, rstMPK.seqID());
        return;
    }

    m_actorPod->forward(pMap->UID(), {AM_ADDCO, amACO}, [this, amACO, rstMPK](const ActorMsgPack &rstRMPK)
    {
        m_actorPod->forward(rstMPK.from(), {rstRMPK.type(), rstRMPK.data()}, rstMPK.seqID());
    });
}

void ServiceCore::on_AM_QUERYMAPLIST(const ActorMsgPack &rstMPK)
{
    AMMapList amML;
    std::memset(&amML, 0, sizeof(amML));

    size_t nIndex = 0;
    for(auto pMap: m_mapList){
        if(pMap.second && pMap.second->ID()){
            if(nIndex < std::extent_v<decltype(amML.MapList)>){
                amML.MapList[nIndex++] = pMap.second->ID();
            }else{
                throw fflerror("Need larger map list size in AMMapList");
            }
        }
    }
    m_actorPod->forward(rstMPK.from(), {AM_MAPLIST, amML}, rstMPK.seqID());
}

void ServiceCore::on_AM_LOADMAP(const ActorMsgPack &mpk)
{
    const auto amLM = mpk.conv<AMLoadMap>();
    const auto mapPtr = retrieveMap(amLM.mapID);

    if(mapPtr){
        m_actorPod->forward(mpk.from(), AM_LOADMAPOK, mpk.seqID());
    }
    else{
        m_actorPod->forward(mpk.from(), AM_LOADMAPERROR, mpk.seqID());
    }
}

void ServiceCore::on_AM_QUERYCOCOUNT(const ActorMsgPack &rstMPK)
{
    AMQueryCOCount amQCOC;
    std::memcpy(&amQCOC, rstMPK.data(), sizeof(amQCOC));

    int nCheckCount = 0;
    if(amQCOC.mapID){
        if(m_mapList.find(amQCOC.mapID) == m_mapList.end()){
            nCheckCount = 0;
        }else{
            nCheckCount = 1;
        }
    }else{
        nCheckCount = to_d(m_mapList.size());
    }

    switch(nCheckCount){
        case 0:
            {
                m_actorPod->forward(rstMPK.from(), AM_ERROR, rstMPK.seqID());
                return;
            }
        case 1:
            {
                if(auto pMap = (amQCOC.mapID ? m_mapList[amQCOC.mapID] : m_mapList.begin()->second)){
                    m_actorPod->forward(pMap->UID(), {AM_QUERYCOCOUNT, amQCOC}, [this, rstMPK](const ActorMsgPack &rstRMPK)
                    {
                        switch(rstRMPK.type()){
                            case AM_COCOUNT:
                                {
                                    m_actorPod->forward(rstMPK.from(), {AM_COCOUNT, rstRMPK.data(), rstRMPK.size()}, rstMPK.seqID());
                                    return;
                                }
                            case AM_ERROR:
                            default:
                                {
                                    m_actorPod->forward(rstMPK.from(), AM_ERROR, rstMPK.seqID());
                                    return;
                                }
                        }
                    });
                    return;
                }else{
                    m_mapList.erase(amQCOC.mapID);
                    m_actorPod->forward(rstMPK.from(), AM_ERROR, rstMPK.seqID());
                    return;
                }
            }
        default:
            {
                // difficult part
                // need send multiple query message and collect them
                // after all collected we need to return the sum, problem:
                // 1. share state
                // 2. error handle

                struct SharedState
                {
                    bool Done;
                    int  CheckCount;
                    int  COCount;

                    SharedState(int nCheckCount)
                        : Done(false)
                        , CheckCount(nCheckCount)
                        , COCount(0)
                    {}
                };

                // current I don't have error handling
                // means if one query didn't get responded it will wait forever
                // to solve this issue, we can install an state hook but for simplity not now

                auto pSharedState = std::make_shared<SharedState>(nCheckCount);
                auto fnOnResp = [pSharedState, this, rstMPK](const ActorMsgPack &rstRMPK)
                {
                    switch(rstRMPK.type()){
                        case AM_COCOUNT:
                            {
                                if(pSharedState->Done){
                                    // we get response but shared state shows ``done"
                                    // means more than one error has alreay happened before
                                    // do nothing
                                }else{
                                    // get one more valid response
                                    // need to check if we need to response to sender
                                    AMCOCount amCOC;
                                    std::memcpy(&amCOC, rstRMPK.data(), sizeof(amCOC));

                                    if(pSharedState->CheckCount == 1){
                                        amCOC.Count += pSharedState->COCount;
                                        m_actorPod->forward(rstMPK.from(), {AM_COCOUNT, amCOC}, rstMPK.seqID());
                                    }else{
                                        pSharedState->CheckCount--;
                                        pSharedState->COCount += to_d(amCOC.Count);
                                    }
                                }
                                return;
                            }
                        case AM_ERROR:
                        default:
                            {
                                if(pSharedState->Done){
                                    // we get response but shared state shows ``done"
                                    // means more than one error has alreay happened before
                                    // do nothing
                                }else{
                                    // get first error
                                    m_actorPod->forward(rstMPK.from(), AM_ERROR, rstMPK.seqID());
                                }
                                return;
                            }
                    }
                };

                for(auto p: m_mapList){
                    m_actorPod->forward(p.second->UID(), {AM_QUERYCOCOUNT, amQCOC}, fnOnResp);
                }
                return;
            }
    }
}

void ServiceCore::on_AM_BADCHANNEL(const ActorMsgPack &mpk)
{
    const auto amBC = mpk.conv<AMBadChannel>();
    m_dbidList.erase(amBC.channID);
}
