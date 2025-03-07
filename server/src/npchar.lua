--, u8R"###(
--

function sendQuery(uid, query)
    sendCallStackQuery(getCallStackUID(), uid, query)
end

function waitEvent()
    local callStackUID = getCallStackUID()
    while true do
        local uid, event, value = pollCallStackEvent(callStackUID)
        if uid then
            return uid, event, value
        end
        coroutine.yield()
    end
end

function uidQueryString(uid, query)
    sendQuery(uid, query)
    local from, event, value = waitEvent()

    if from ~= uid then
        fatalPrintf('Send query to uid %s but get response from %s', uid, from)
    end

    if event ~= SYS_NPCQUERY then
        fatalPrintf('Wait event as SYS_NPCQUERY but get %s', tostring(event))
    end
    return value
end

function uidQuery(uid, query, ...)
    return uidQueryString(uid, query:format(...))
end

function uidQueryName(uid)
    return uidQuery(uid, 'NAME')
end

function uidQueryRedName(uid)
    return false
end

function uidQueryLevel(uid)
    return tonumber(uidQuery(uid, 'LEVEL'))
end

function uidQueryGold(uid)
    return tonumber(uidQuery(uid, 'GOLD'))
end

function convItemSeqID(item)
    if math.type(item) == 'integer' then
        return item, 0
    elseif type(item) == 'string' then
        return getItemID(item), 0
    elseif type(item) == 'table' then
        if item.itemID ~= nil then
            local itemID = convItemSeqID(item.itemID)
            if item.seqID == nil then
                return itemID, 0
            elseif math.type(item.seqID) == 'integer' then
                return itemID, item.seqID
            elseif type(item.seqID) == 'string' then
                return itemID, tonumber(item.seqID)
            else
                fatalPrintf("invalid augument: unexpected item.seqID type: %s", type(item.seqID))
            end
        else
            fatalPrintf("invalid augument: table has no itemID")
        end
    else
        fatalPrintf('invalid argument: item = %s', tostring(item))
    end
end

function uidRemove(uid, item, count)
    local itemID, seqID = convItemSeqID(item)
    if itemID == 0 then
        fatalPrintf('invalid item: %s', tostring(item))
    end

    local value = uidQuery(uid, 'REMOVE %d %d %d', itemID, seqID, argDef(count, 1))
    if value == '1' then
        return true
    elseif value == '0' then
        return false
    else
        fatalPrintf('invalid query result: %s', value)
    end
end

-- always use 金币（小）to represent the gold item
-- when convert to a SDItem the real 小中大 will get figured out by the count
function uidRemoveGold(uid, count)
    return uidRemove(uid, '金币（小）', count)
end

function uidSecureItem(uid, itemID, seqID)
    local result = uidQuery(uid, 'SECURE %d %d', itemID, seqID)
    if result == '1' then
        return true
    elseif result == '0' then
        return false
    else
        fatalPrintf('invalid query result: %s', result)
    end
end

function uidShowSecuredItemList(uid)
    local result = uidQuery(uid, 'SHOWSECURED')
    if result == '1' then
        return true
    elseif result == '0' then
        return false
    else
        fatalPrintf('invalid query result: %s', result)
    end
end

function uidGrant(uid, item, count)
    local itemID = convItemSeqID(item)
    if itemID == 0 then
        fatalPrintf('invalid item: %s', tostring(item))
    end

    local value = uidQuery(uid, 'GRANT %d %d', itemID, argDef(count, 1))
    if value == '1' then
        return true
    elseif value == '0' then
        return false
    else
        fatalPrintf('invalid query result: %s', value)
    end
end

-- setup call stack table for thread-based parameters
-- we spawn call stack by sol::thread which still access global table
-- so we can't have tls per call stack, have to save call stack related globals into this table
--
-- TODO: take a look at this code for tls implementation
-- https://stackoverflow.com/a/24358483
g_callStackTableList = {}

function getCallStackTable()
    local id, main_thread = coroutine.running()
    if main_thread then
        fatalPrintf('calling getCallStackTable() in main thead')
    end

    if not g_callStackTableList[id] then
        g_callStackTableList[id] = {}
    end
    return g_callStackTableList[id]
end

function clearCallStackTable()
    local id, main_thread = coroutine.running()
    if main_thread then
        fatalPrintf('calling clearCallStackTable() in main thead')
    end
    g_callStackTableList[id] = nil
end

function setCallStackUID(uid)
    if not isUID(uid) then
        fatalPrintf("invalid call stack uid: nil")
    end

    local callStackTable = getCallStackTable()
    if callStackTable['CS_UID'] then
        fatalPrintf('calling setCallStackUID() in same thread twice')
    end
    callStackTable['CS_UID'] = uid
end

function getCallStackUID()
    local callStackTable = getCallStackTable()
    if not callStackTable['CS_UID'] then
        fatalPrintf('call stack has no uid setup, missed to call setCallStackUID(uid) in main()')
    end
    return callStackTable['CS_UID']
end

function uidGrantGold(uid, count)
    uidGrant(uid, '金币（小）', count)
end

function uidPostXML(uid, xmlFormat, ...)
    if type(uid) ~= 'string' or type(xmlFormat) ~= 'string' then
        fatalPrintf("invalid argument type: uid: %s, xmlFormat: %s", type(uid), type(xmlFormat))
    end

    if not isUID(uid) then
        fatalPrintf("not a valid uid string: %s", uid)
    end
    uidPostXMLString(uid, xmlFormat:format(...))
end

function has_processNPCEvent(verbose, event)
    verbose = verbose or false
    if type(verbose) ~= 'boolean' then
        verbose = false
        addLog(LOGTYPE_WARNING, 'parmeter verbose is not boolean type, assumed false')
    end

    if type(event) ~= 'string' then
        event = nil
        addLog(LOGTYPE_WARNING, 'parmeter event is not string type, ignored')
    end

    if not processNPCEvent then
        if verbose then
            addLog(LOGTYPE_WARNING, "NPC %s: processNPCEvent is not defined", getNPCFullName())
        end
        return false
    elseif type(processNPCEvent) ~= 'table' then
        if verbose then
            addLog(LOGTYPE_WARNING, "NPC %s: processNPCEvent is not a function table", getNPCFullName())
        end
        return false
    else
        local count = 0
        for _ in pairs(processNPCEvent) do
            -- here for each entry we can check if the key is string and value is function type
            -- but can possibly be OK if the event is not triggered
            count = count + 1
        end

        if count == 0 then
            if verbose then
                addLog(LOGTYPE_WARNING, "NPC %s: processNPCEvent is empty", getNPCFullName())
            end
            return false
        end

        if not event then
            return true
        end

        if type(processNPCEvent[event]) ~= 'function' then
            if verbose then
                addLog(LOGTYPE_WARNING, "NPC %s: processNPCEvent[%s] is not a function", getNPCFullName(), event)
            end
            return false
        end
        return true
    end
end

-- entry coroutine for event handling
-- it's event driven, i.e. if the event sink has no event, this coroutine won't get scheduled

function main(uid)
    -- setup current call stack uid
    -- all functions in current call stack can use this implicit argument as *this*
    setCallStackUID(uid)

    -- poll the event sink
    -- current call stack only process 1 event and then clean itself
    local from, event, value = waitEvent()
    if event == SYS_NPCDONE then
        clearCallStackTable()
    elseif has_processNPCEvent(false, event) then
        processNPCEvent[event](from, value)
    else
        -- don't exit this loop
        -- always consume the event no matter if the NPC can handle it
        uidPostXML(uid,
        [[
            <layout>
                <par>我听不懂你在说什么。。。</par>
                <par></par>
                <par><event id="%s">关闭</event></par>
            </layout>
        ]], SYS_NPCDONE)
    end

    -- event process done
    -- clean the call stack itself, next event needs another call stack
    clearCallStackTable()
end

--
-- )###"
