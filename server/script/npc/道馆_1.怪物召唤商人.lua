addLog(LOGTYPE_INFO, 'NPC %s sources %s', getNPCFullName(), getFileName())
setNPCLook(12)
setNPCGLoc(406, 126)

processNPCEvent = {}
do
    local monsterID = 1
    local monsterNameString = ''

    while(true) do
        local monsterName = getMonsterName(monsterID)
        if monsterName == '' then
            break
        end

        local suffixDigits = string.match(monsterName, '^.-(%d+)$')
        if suffixDigits == nil then
            if monsterName ~= '未知' then
                local tagName = string.format('goto_tag_%d', monsterID)
                monsterNameString = monsterNameString .. string.format([[<event id="%s" wrap="false">%s，</event>]], tagName, monsterName)

                processNPCEvent[tagName] = function(uid, value)
                    addMonster(monsterName)
                end
            end
        end
        monsterID = monsterID + 1
    end

    processNPCEvent[SYS_NPCINIT] = function(uid, value)
        uidPostXML(uid,
        [[
            <layout>
                <par>客官%s你好我是%s，我可以召唤所有的怪物哦！<emoji id="0"/></par>
                <par></par>
                <par align="justify">%s</par>
                <par></par>
                <par><event id="%s">关闭</event></par>
            </layout>
        ]], uidQueryName(uid), getNPCName(), monsterNameString, SYS_NPCDONE)
    end
end
