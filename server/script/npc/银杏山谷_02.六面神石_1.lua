setNPCLook(56)
setNPCGLoc(249, 144)

local tp = require('npc.include.teleport')
tp.setTeleport('<par>你想去哪里？</par>',
{
    {name = '比奇城', map = '比奇县_0', x = 499, y = 462, gold = 500},
    {},

    {name = '边境村庄', map = '边境城市_01', x = 457, y = 215, gold = 1000},
    {name = '道馆村', map = '道馆_1', x = 417, y = 178, gold = 1000},
    {name = '毒蛇山村', map = '毒蛇山谷_2', x = 307, y = 243, gold = 1000},
    {},

    {name = '潘夜岛村', map = '潘夜岛_8', x = 289, y = 240, gold = 1500},
    {name = '盟重土城', map = '盟重县_74', x = 350, y = 328, gold = 1500},
    {},

    {map = '沙漠土城_5', x = 205, y = 288, gold = 2000},
    {},

    {map = '绿洲_4', x = 436, y = 82, gold = 2500},
    {},
})
