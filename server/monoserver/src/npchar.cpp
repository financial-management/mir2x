/*
 * =====================================================================================
 *
 *       Filename: npchar.cpp
 *        Created: 04/12/2020 16:01:51
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

#include <cstdint>
#include "uidf.hpp"
#include "npchar.hpp"
#include "totype.hpp"
#include "filesys.hpp"
#include "dbcomid.hpp"
#include "cerealf.hpp"
#include "fflerror.hpp"
#include "serdesmsg.hpp"
#include "friendtype.hpp"
#include "monoserver.hpp"
#include "dbcomrecord.hpp"
#include "serverconfigurewindow.hpp"

extern MonoServer *g_monoServer;
extern ServerConfigureWindow *g_serverConfigureWindow;

NPChar::LuaNPCModule::LuaNPCModule(NPChar *npc)
    : ServerLuaModule()
{
    m_luaState.set_function("getUID", [npc]() -> std::string
    {
        return std::to_string(npc->rawUID());
    });

    m_luaState.set_function("getUIDString", [](std::string uidString) -> std::string
    {
        return uidf::getUIDString(uidf::toUID(uidString));
    });

    m_luaState.set_function("getNPCName", [npc]() -> std::string
    {
        return std::string(to_cstr(DBCOM_NPCRECORD(uidf::getNPCID(npc->rawUID())).name));
    });

    m_luaState.set_function("getNPCFullName", [npc]() -> std::string
    {
        return std::string(to_cstr(DBCOM_MAPRECORD(uidf::getMapID(npc->m_map->UID())).name)) + "." + std::string(to_cstr(DBCOM_NPCRECORD(uidf::getNPCID(npc->rawUID())).name));
    });

    m_luaState.set_function("sendSell", [npc, this](std::string uidString, std::string listName)
    {
        if(const sol::object checkObj = m_luaState[listName]; checkObj == sol::nil){
            throw fflerror("lua script has no variable defined: %s", to_cstr(listName));
        }

        std::vector<std::string> itemList;
        sol::table luaTable = m_luaState[listName];

        for(const auto &p: luaTable){
            itemList.push_back(p.second.as<std::string>());
        }
        npc->sendSell(uidf::toUIDEx(uidString), itemList);
    });

    m_luaState.set_function("sendCallStackQuery", [npc](std::string callStackUID, std::string uidString, std::string query)
    {
        npc->sendQuery(uidf::toUIDEx(callStackUID), uidf::toUIDEx(uidString), query);
    });

    m_luaState.set_function("getSellItemList", [npc](sol::this_state luaPtr)
    {
        std::vector<std::string> itemNameList;
        for(const auto itemID: npc->getSellItemIDList()){
            if(const auto &ir = DBCOM_ITEMRECORD(itemID); ir && (std::u8string_view(ir.name) != u8"未知")){
                itemNameList.push_back(to_cstr(ir.name));
            }
        }
        return sol::make_object(sol::state_view(luaPtr), itemNameList);
    });

    m_luaState.set_function("sayXML", [npc, this](std::string uidString, std::string xmlString)
    {
        const uint64_t uid = [&uidString]() -> uint64_t
        {
            try{
                return std::stoull(uidString);
            }
            catch(...){
                //
            }
            return 0;
        }();

        if(uid){
            npc->sendXMLLayout(uid, std::move(xmlString));
        }

        else{
            addLog(0, u8"invalid UID: 0");
        }
    });

    m_luaState.set_function("pollCallStackEvent", [this](std::string callStackUID)
    {
        const uint64_t uid = [&callStackUID]() -> uint64_t
        {
            try{
                return std::stoull(callStackUID);
            }
            catch(...){
                //
            }
            return 0;
        }();

        return sol::as_returns([uid, this]() -> std::vector<std::string>
        {
            if(auto p = m_callStackList.find(uid); p != m_callStackList.end()){
                const auto fromUID = p->second.from;
                p->second.from = 0;

                if(!fromUID){
                    return {};
                }

                if(p->second.event.empty()){
                    throw fflerror("detected uid %llu with empty event", to_llu(fromUID));
                }


                if(p->second.value.empty()){
                    return {std::to_string(fromUID), std::move(p->second.event)};
                }
                return {std::to_string(fromUID), std::move(p->second.event), std::move(p->second.value)};
            }
            throw fflerror("can't find call stack UID = %llu", to_llu(uid));
        }());
    });

    m_luaState.script(INCLUA_BEGIN(char)
#include "npchar.lua"
    INCLUA_END());

    m_luaState.script_file([npc]() -> std::string
    {
        const auto scriptPath = []() -> std::string
        {
            if(const auto cfgScriptPath = g_serverConfigureWindow->getScriptPath(); !cfgScriptPath.empty()){
                return cfgScriptPath + "/npc";
            }
            return std::string("script/npc");
        }();

        const auto mapName = to_cstr(DBCOM_MAPRECORD(uidf::getMapID(npc->m_map->UID())).name);
        const auto npcName = to_cstr(DBCOM_NPCRECORD(uidf::getNPCID(npc->rawUID())).name);

        for(const auto &fileName:
        {
            str_printf("%s/%s.%s.lua",           scriptPath.c_str(), mapName, npcName),
            str_printf("%s/default/%s.lua",      scriptPath.c_str(), npcName),
            str_printf("%s/default/default.lua", scriptPath.c_str()),
        }){
            if(filesys::hasFile(fileName.c_str())){
                return fileName;
            }
        }
        throw fflerror("no available script for NPC %s.%s", mapName, npcName);
    }());

    // NPC script has no state
    // after source it finishes the script, not yield

    m_luaState.script
    (
        R"###( -- do the first sanity check here                               )###""\n"
        R"###( -- last in main call we also check it but with verbose disabled )###""\n"
        R"###( has_processNPCEvent(true, SYS_NPCINIT)                          )###""\n");
}

void NPChar::LuaNPCModule::setEvent(uint64_t callStackUID, uint64_t from, std::string event, std::string value)
{
    if(!(callStackUID && from && !event.empty())){
        throw fflerror("invalid argument: callStackUID = %llu, from = %llu, event = %s, value = %s", to_llu(callStackUID), to_llu(from), to_cstr(event), to_cstr(value));
    }

    if(event == SYS_NPCDONE){
        m_callStackList.erase(callStackUID);
        return;
    }

    const auto fnCheckCOResult = [this](const auto &result)
    {
        if(result.valid()){
            return;
        }

        const sol::error err = result;
        std::stringstream errStream(err.what());

        std::string errLine;
        while(std::getline(errStream, errLine, '\n')){
            addLog(1, to_u8cstr(errLine));
        }
    };

    auto p = m_callStackList.find(callStackUID);
    if(p == m_callStackList.end()){
        p = m_callStackList.insert({callStackUID, LuaNPCModule::LuaCallStack(this)}).first;

        // initial call to make main reaches its event polling point
        // need to assign event to let it advance
        const auto result = p->second.co_callback(std::to_string(callStackUID));
        fnCheckCOResult(result);
    }

    if(!p->second.co_callback){
        throw fflerror("lua coroutine is not callable");
    }

    // clear the event
    // call the coroutine to make it stuck at pollEvent()

    p->second.from  = from;
    p->second.event = std::move(event);
    p->second.value = std::move(value);

    const auto result = p->second.co_callback();
    fnCheckCOResult(result);

    if(!p->second.co_callback){
        // not invocable anymore after the event-driven call
        // the event handling coroutine is done
        //
        // remove the call stack when an event sequence is done, i.e.
        // 1. get event SYS_NPCINIT from player, init main()
        // 2. in processNPCEvent(SYS_NPCINIT), script calls uidQueryName(), this sends QUERY_NAME
        // 3. get event NAME
        // 4. in processNPCEvent(SYS_NPCINIT), script calls uidQueryLevel(), this sends QUERY_LEVEL
        // 5. get event LEVEL
        // 6. send sayXML() to player, done main()
        m_callStackList.erase(p);
    }
}

NPChar::NPChar(uint16_t npcId, ServiceCore *core, ServerMap *serverMap, int mapX, int mapY)
    : CharObject(core, serverMap, uidf::buildNPCUID(npcId), mapX, mapY, DIR_NONE)
{
    // LuaNPCModule(this) can access ``this"
    // when constructing LuaNPCModule we need to confirm ``this" is ready
    m_luaModulePtr = std::make_unique<NPChar::LuaNPCModule>(this);
    fillSellItemList();
}

bool NPChar::update()
{
    return true;
}

bool NPChar::InRange(int, int, int)
{
    return true;
}

void NPChar::reportCO(uint64_t)
{
}

bool NPChar::DCValid(int, bool)
{
    return true;
}

DamageNode NPChar::GetAttackDamage(int)
{
    return {};
}

bool NPChar::StruckDamage(const DamageNode &)
{
    return true;
}

void NPChar::checkFriend(uint64_t, std::function<void(int)> fnOP)
{
    fnOP(FT_NEUTRAL);
}

bool NPChar::goDie()
{
    return true;
}

bool NPChar::goGhost()
{
    return true;
}

void NPChar::sendSell(uint64_t uid, const std::vector<std::string> &itemList)
{
    sendNetPackage(uid, SM_NPCSELL, cerealf::serialize(SDNPCSell
    {
        .npcUID = UID(),
        .itemList = [&itemList]()
        {
            std::vector<uint32_t> itemIDList;
            for(const auto &itemName: itemList){
                if(const uint32_t itemID = DBCOM_ITEMID(to_u8cstr(itemName))){
                    itemIDList.push_back(itemID);
                }
                else{
                    g_monoServer->addLog(LOGTYPE_WARNING, "invalid NPC selling item: %s", to_cstr(itemName));
                }
            }
            return itemIDList;
        }()
    }, true));
}

void NPChar::sendQuery(uint64_t callStackUID, uint64_t uid, const std::string &query)
{
    AMNPCQuery amNPCQ;
    std::memset(&amNPCQ, 0, sizeof(amNPCQ));

    const auto seqID = m_luaModulePtr->getCallStackSeqID(callStackUID);
    if(!seqID){
        throw fflerror("calling sendQuery(%llu, %llu, %s) outside of LuaCallStack", to_llu(callStackUID), to_llu(uid), to_cstr(query));
    }

    if(query.size() >= sizeof(amNPCQ.query)){
        throw fflerror("query name is too long: %s", query.c_str());
    }

    std::strcpy(amNPCQ.query, query.c_str());
    m_actorPod->forward(uid, {AM_NPCQUERY, amNPCQ}, [callStackUID, uid, seqID, this](const ActorMsgPack &mpk)
    {
        if(uid != mpk.from()){
            throw fflerror("query sent to uid %llu but get response from %llu", to_llu(uid), to_llu(mpk.from()));
        }

        if(mpk.seqID()){
            throw fflerror("query result expects response");
        }

        if(m_luaModulePtr->getCallStackSeqID(callStackUID) != seqID){
            return;
        }

        switch(mpk.type()){
            case AM_NPCEVENT:
                {
                    const auto amNPCE = mpk.conv<AMNPCEvent>();
                    m_luaModulePtr->setEvent(callStackUID, uid, amNPCE.event, amNPCE.value);
                    return;
                }
            default:
                {
                    m_luaModulePtr->close(callStackUID);
                    return;
                }
        }
    });
}

void NPChar::sendXMLLayout(uint64_t uid, std::string xmlString)
{
    sendNetPackage(uid, SM_NPCXMLLAYOUT, cerealf::serialize(SDNPCXMLLayout
    {
        .npcUID = UID(),
        .xmlLayout = std::move(xmlString),
    }, true));
}

void NPChar::operateAM(const ActorMsgPack &mpk)
{
    switch(mpk.type()){
        case AM_OFFLINE:
        case AM_METRONOME:
            {
                break;
            }
        case AM_BUY:
            {
                on_AM_BUY(mpk);
                break;
            }
        case AM_ACTION:
            {
                on_AM_ACTION(mpk);
                break;
            }
        case AM_NPCEVENT:
            {
                on_AM_NPCEVENT(mpk);
                break;
            }
        case AM_NOTIFYNEWCO:
            {
                on_AM_NOTIFYNEWCO(mpk);
                break;
            }
        case AM_QUERYCORECORD:
            {
                on_AM_QUERYCORECORD(mpk);
                break;
            }
        case AM_QUERYLOCATION:
            {
                on_AM_QUERYLOCATION(mpk);
                break;
            }
        case AM_QUERYSELLITEMLIST:
            {
                on_AM_QUERYSELLITEMLIST(mpk);
                break;
            }
        case AM_BADACTORPOD:
            {
                on_AM_BADACTORPOD(mpk);
                break;
            }
        default:
            {
                throw fflerror("unsupported message: %s", mpkName(mpk.type()));
            }
    }
}

std::vector<SDCostItem> NPChar::getCostItemList(const SDItem &) const
{
    SDCostItem cost;
    std::vector<SDCostItem> result;

    cost.itemID = DBCOM_ITEMID(u8"金币");
    cost.count  = 100 + std::rand() % 10;

    result.push_back(cost);
    return result;
}

std::set<uint32_t> NPChar::getSellItemIDList() const
{
    std::set<uint32_t> itemIDList;
    for(uint32_t itemID = 1; itemID < 1000; ++itemID){
        if(const auto &ir = DBCOM_ITEMRECORD(itemID); ir && (std::u8string_view(ir.name) != u8"未知")){
            itemIDList.insert(itemID);
        }
    }
    return itemIDList;
}

void NPChar::fillSellItemList()
{
    for(const uint32_t itemID: getSellItemIDList()){
        const auto &ir = DBCOM_ITEMRECORD(itemID);
        if(!ir){
            throw fflerror("selling invalid item: itemID = %llu", to_llu(itemID));
        }

        auto &itemListRef = m_sellItemList[itemID];
        if(ir.packable()){
            // refresh the price for packable item
            // always use seqID = 0 for packable item in the list

            const auto item = createSellItem(itemID, 0);
            const auto cost = getCostItemList(item);

            itemListRef[0] = NPChar::SellItem
            {
                .item = item,
                .locked = false,
                .costList = cost,
            };

            if(itemListRef.size() != 1){
                throw fflerror("failed to reset packable item");
            }
        }
        else{
            const auto fillSize = std::max<size_t>(itemListRef.size(), 20 + std::rand() % 5);
            while(itemListRef.size() < fillSize){
                const uint32_t seqID = itemListRef.empty() ? 1 : (itemListRef.rbegin()->first + 1);
                const auto item = createSellItem(itemID, seqID);
                const auto cost = getCostItemList(item);
                itemListRef[seqID] = NPChar::SellItem
                {
                    .item = item,
                    .locked = false,
                    .costList = cost,
                };
            }
        }
    }
}

SDItem NPChar::createSellItem(uint32_t itemID, uint32_t seqID) const
{
    return SDItem
    {
        .itemID = itemID,
        .seqID =  seqID,
        .count = 1,
        .duration = 0,
        .extAttrList = {},
    };
}
