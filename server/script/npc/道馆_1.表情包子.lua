-- =====================================================================================
--
--       Filename: 道馆.散财童子.lua
--        Created: 11/22/2020 08:52:57 PM
--    Description: lua 5.3
--
--        Version: 1.0
--       Revision: none
--       Compiler: lua
--
--         Author: ANHONG
--          Email: anhonghe@gmail.com
--   Organization: USTC
--
-- =====================================================================================

-- NPC script
-- provides the table: processNPCEvent for event processing

addLog(LOGTYPE_INFO, string.format('NPC %s sources %s', getNPCFullName(), getFileName()))
setNPCLook(13)
setNPCGLoc(407, 127)

processNPCEvent =
{
    [SYS_NPCINIT] = function(uid, value)
        uidPostXML(uid,
        [[
            <layout>
                <par>客官%s你好我是%s，我展示所有表情包子！<emoji id="0"/></par>
                <par></par>
                <par><emoji id="0"/> <emoji id="1"/> <emoji id="2"/> <emoji id="3"/> <emoji id="4"/> <emoji id="5"/> <emoji id="6"/> <emoji id="7"/> <emoji id="8"/> <emoji id="9"/> <emoji id="10"/> <emoji id="11"/> <emoji id="12"/> <emoji id="13"/> <emoji id="14"/> <emoji id="15"/> <emoji id="16"/> <emoji id="17"/> <emoji id="18"/> <emoji id="19"/> <emoji id="20"/> <emoji id="21"/> <emoji id="22"/> <emoji id="23"/> <emoji id="24"/> <emoji id="25"/> <emoji id="26"/> <emoji id="27"/> <emoji id="28"/> <emoji id="29"/> <emoji id="30"/> <emoji id="31"/> <emoji id="32"/> <emoji id="33"/> <emoji id="34"/> <emoji id="35"/> <emoji id="36"/> <emoji id="37"/> <emoji id="38"/> <emoji id="39"/> <emoji id="40"/> <emoji id="41"/> <emoji id="42"/> <emoji id="43"/> <emoji id="44"/> <emoji id="45"/> <emoji id="46"/> <emoji id="47"/> <emoji id="48"/> <emoji id="49"/> <emoji id="50"/> <emoji id="51"/> <emoji id="52"/> <emoji id="53"/> <emoji id="54"/> <emoji id="55"/> <emoji id="56"/> <emoji id="57"/> <emoji id="58"/> <emoji id="59"/> <emoji id="60"/> <emoji id="61"/> <emoji id="62"/> <emoji id="63"/> <emoji id="64"/> <emoji id="65"/> <emoji id="66"/> <emoji id="67"/> <emoji id="68"/> <emoji id="69"/> <emoji id="70"/> <emoji id="71"/> <emoji id="72"/> <emoji id="73"/> <emoji id="74"/> <emoji id="75"/> <emoji id="76"/> <emoji id="77"/> <emoji id="78"/> <emoji id="79"/> <emoji id="80"/> <emoji id="81"/> <emoji id="82"/> <emoji id="83"/> <emoji id="84"/> <emoji id="85"/> <emoji id="86"/> <emoji id="87"/> <emoji id="88"/> <emoji id="89"/> <emoji id="90"/> <emoji id="91"/> <emoji id="92"/> <emoji id="93"/> <emoji id="94"/> <emoji id="95"/> <emoji id="96"/> <emoji id="97"/> <emoji id="98"/> <emoji id="99"/> <emoji id="100"/> <emoji id="101"/> <emoji id="102"/> <emoji id="103"/> <emoji id="104"/> <emoji id="105"/> <emoji id="106"/> <emoji id="107"/> <emoji id="108"/> <emoji id="109"/> <emoji id="110"/> <emoji id="111"/> <emoji id="112"/> <emoji id="113"/> <emoji id="114"/> <emoji id="115"/> <emoji id="116"/> <emoji id="117"/> <emoji id="118"/> <emoji id="119"/> <emoji id="120"/> <emoji id="121"/> <emoji id="122"/> <emoji id="123"/> <emoji id="124"/> <emoji id="125"/> <emoji id="126"/> <emoji id="127"/> <emoji id="128"/> <emoji id="129"/> <emoji id="130"/> <emoji id="131"/> <emoji id="132"/> <emoji id="133"/> <emoji id="134"/> <emoji id="135"/> <emoji id="136"/> <emoji id="137"/> <emoji id="138"/> <emoji id="139"/> <emoji id="140"/> <emoji id="141"/> <emoji id="142"/> <emoji id="143"/> <emoji id="144"/> <emoji id="145"/> <emoji id="146"/> <emoji id="147"/> <emoji id="148"/> <emoji id="149"/> <emoji id="150"/> <emoji id="151"/> <emoji id="152"/> <emoji id="153"/> <emoji id="154"/> <emoji id="155"/> <emoji id="156"/> <emoji id="157"/> <emoji id="158"/> <emoji id="159"/> <emoji id="160"/> <emoji id="161"/> <emoji id="162"/> <emoji id="163"/> <emoji id="164"/> <emoji id="165"/> <emoji id="166"/> <emoji id="167"/> <emoji id="168"/> <emoji id="169"/></par>
                <par><event id="%s">关闭</event></par>
            </layout>
        ]], uidQueryName(uid), getNPCName(), SYS_NPCDONE)
    end,
}
