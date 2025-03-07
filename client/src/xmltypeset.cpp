/*
 * =====================================================================================
 *
 *       Filename: xmltypeset.cpp
 *        Created: 12/11/2018 04:44:07
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

#include <cinttypes>
#include "log.hpp"
#include "lalign.hpp"
#include "totype.hpp"
#include "fflerror.hpp"
#include "xmltypeset.hpp"
#include "utf8f.hpp"
#include "mathf.hpp"
#include "fontexdb.hpp"
#include "sdldevice.hpp"
#include "emojidb.hpp"
#include "clientargparser.hpp"

extern Log *g_log;
extern FontexDB *g_fontexDB;
extern SDLDevice *g_sdlDevice;
extern EmojiDB *g_emojiDB;
extern ClientArgParser *g_clientArgParser;

void XMLTypeset::SetTokenBoxWordSpace(int nLine)
{
    if(!lineValid(nLine)){
        throw fflerror("invalid line: %d", nLine);
    }

    int nW1 = m_wordSpace / 2;
    int nW2 = m_wordSpace - nW1;

    for(auto &rstToken: m_lineList[nLine].content){
        rstToken.Box.State.W1 = nW1;
        rstToken.Box.State.W2 = nW2;
    }

    if(!m_lineList[nLine].content.empty()){
        m_lineList[nLine].content[0]    .Box.State.W1 = 0;
        m_lineList[nLine].content.back().Box.State.W2 = 0;
    }
}

// get line full width, count everything inside
// assume:
//      token W/W1/W2 has been set
// return:
//      full line width
int XMLTypeset::LineFullWidth(int nLine) const
{
    if(!lineValid(nLine)){
        throw fflerror("invalid line: %d", nLine);
    }

    int nWidth = 0;
    for(int nIndex = 0; nIndex < lineTokenCount(nLine); ++nIndex){
        auto pToken = getToken(nIndex, nLine);
        nWidth += (pToken->Box.State.W1 + pToken->Box.Info.W + pToken->Box.State.W2);
    }
    return nWidth;
}

// calculate line width without W1/W2
// assume:
//      tokens are in current line
//      tokens have W initialized
// return:
//      total token width, plus word space optionally
//      negative if error
// this function is used before we padding all tokens, to estimate how many pixels we need for current line
int XMLTypeset::LineRawWidth(int nLine, bool bWithWordSpace) const
{
    if(!lineValid(nLine)){
        throw fflerror("invalid line: %d", nLine);
    }

    switch(lineTokenCount(nLine)){
        case 0:
            {
                return 0;
            }
        case 1:
            {
                return getToken(0, nLine)->Box.Info.W;
            }
        default:
            {
                // for more than one tokens
                // we need to check word spacing

                int nWidth = 0;
                for(int nX = 0; nX < lineTokenCount(nLine); ++nX){
                    nWidth += getToken(nX, nLine)->Box.Info.W;
                    if(bWithWordSpace){
                        nWidth += GetTokenWordSpace(nX, nLine);
                    }
                }

                if(bWithWordSpace){
                    int nWordSpaceFirst = GetTokenWordSpace(0, nLine);
                    int nWordSpaceLast  = GetTokenWordSpace(lineTokenCount(nLine) - 1, nLine);

                    nWidth -= (nWordSpaceFirst / 2);
                    nWidth -= (nWordSpaceLast - nWordSpaceLast / 2);
                }

                return nWidth;
            }
    }
}

bool XMLTypeset::addRawTokenLine(int nLine, const std::vector<TOKEN> &tokenLine)
{
    if(!lineValid(nLine)){
        throw fflerror("invalid line: %d", nLine);
    }

    if(tokenLine.empty()){
        throw fflerror("empty token line");
    }

    if(m_lineWidth <= 0){
        m_lineList[nLine].content.insert(m_lineList[nLine].content.end(), tokenLine.begin(), tokenLine.end());
        return true;
    }

    // if we have a defined width but too small
    // need to accept but give warnings

    const auto rawExtraWidth = [&tokenLine]() -> int
    {
        int result = 0;
        for(const auto &token: tokenLine){
            result += token.Box.Info.W;
        }
        return result;
    }();

    if((lineTokenCount(nLine) == 0) && (m_lineWidth < to_d(rawExtraWidth + (tokenLine.size() - 1) * m_wordSpace))){
        g_log->addLog(LOGTYPE_WARNING, "XMLTypeset width is too small to hold the token line: lineWidth = %d", m_lineWidth);
        m_lineList[nLine].content.insert(m_lineList[nLine].content.end(), tokenLine.begin(), tokenLine.end());
        return true;
    }

    if(m_lineWidth < LineRawWidth(nLine, false) + rawExtraWidth + to_d(lineTokenCount(nLine) + tokenLine.size() - 1) * m_wordSpace){
        return false;
    }

    m_lineList[nLine].content.insert(m_lineList[nLine].content.end(), tokenLine.begin(), tokenLine.end());
    return true;
}

void XMLTypeset::LinePadding(int nLine)
{
    if(!lineValid(nLine)){
        throw fflerror("invalid line: %d", nLine);
    }
}

void XMLTypeset::LineDistributedPadding(int)
{
}

// do justify padding for current line
// assume:
//      1. alreayd put all tokens into current line
//      2. token has Box.W ready
void XMLTypeset::LineJustifyPadding(int nLine)
{
    if(!lineValid(nLine)){
        throw fflerror("invalid line: %d", nLine);
    }

    if(lineAlign() != LALIGN_JUSTIFY){
        throw fflerror("do line justify-padding while board align is configured as: %d", lineAlign());
    }

    switch(lineTokenCount(nLine)){
        case 0:
            {
                throw fflerror("empty line detected: %d", nLine);
            }
        case 1:
            {
                return;
            }
        default:
            {
                break;
            }
    }

    if(MaxLineWidth() == 0){
        throw fflerror("do line justify-padding while board is configured as infinite single line mode");
    }

    // we allow to exceeds the line limitation..
    // when there is a huge token inderted to current line, but only for this exception

    if((LineRawWidth(nLine, true) > MaxLineWidth()) && (lineTokenCount(nLine) > 1)){
        throw fflerror("line raw width exceeds the fixed max line width: %d", MaxLineWidth());
    }

    const auto fnLeafPadding = [this, y = nLine](const auto &fnCheckToken) -> int
    {
        while(LineFullWidth(y) < MaxLineWidth()){
            int nCurrDWidth = MaxLineWidth() - LineFullWidth(y);
            int nDoneDWidth = nCurrDWidth;
            for(int x = 0; x < lineTokenCount(y); ++x){
                if(!fnCheckToken(x, y)){
                    continue;
                }

                auto tokenPtr = getToken(x, y);
                if(tokenPtr->Box.State.W1 <= tokenPtr->Box.State.W2){
                    if(x != 0){
                        tokenPtr->Box.State.W1++;
                        nDoneDWidth--;

                        if(nDoneDWidth == 0){
                            return MaxLineWidth();
                        }
                    }
                }

                else{
                    if(x != lineTokenCount(y) - 1){
                        tokenPtr->Box.State.W2++;
                        nDoneDWidth--;

                        if(nDoneDWidth == 0){
                            return MaxLineWidth();
                        }
                    }
                }
            }

            // can't help
            // we go through the whole line but no width changed

            if(nCurrDWidth == nDoneDWidth){
                return LineFullWidth(y);
            }
        }
        return MaxLineWidth();
    };

    // first round
    // padding image and emoji only, limited to 10%

    if(fnLeafPadding([this](int x, int y) -> bool
    {
        const auto tokenPtr = getToken(x, y);
        switch(m_paragraph.leafRef(tokenPtr->leaf).type()){
            case LEAF_IMAGE:
            case LEAF_EMOJI: return tokenPtr->Box.State.W1 + tokenPtr->Box.State.W2 < tokenPtr->Box.Info.W / 5;
            default        : return false;
        }
    }) == MaxLineWidth()){
        return;
    }

    // second round
    // padding utf8 text but only do blank chars

    if(fnLeafPadding([this](int x, int y) -> bool
    {
        return blankToken(x, y);
    }) == MaxLineWidth()){
        return;
    }

    // last round
    // padding ut8 chars

    if(fnLeafPadding([this](int x, int y) -> bool
    {
        return m_paragraph.leafRef(getToken(x, y)->leaf).type() == LEAF_UTF8GROUP;
    }) == MaxLineWidth()){
        return;
    }

    throw fflerror("can't do padding to width: %d", MaxLineWidth());
}

void XMLTypeset::resetOneLine(int nLine, bool bCREnd)
{
    if(!lineValid(nLine)){
        throw fflerror("invalid line: %d", nLine);
    }

    // some tokens may have non-zero W1/W2 when reach here
    // need to reset them all

    const auto wordSpace = [this]() -> std::array<int, 2>
    {
        switch(lineAlign()){
            case LALIGN_LEFT:
            case LALIGN_JUSTIFY:
            case LALIGN_RIGHT:
            case LALIGN_CENTER:
                {
                    return
                    {
                        (m_wordSpace + 0) / 2,
                        (m_wordSpace + 1) / 2,
                    };
                }
            case LALIGN_DISTRIBUTED:
                {
                    return {0, 0};
                }
            default:
                {
                    throw fflerror("invalid line align: %d", lineAlign());
                }
        }
    }();

    for(int i = 0, tokenCnt = lineTokenCount(nLine); i < tokenCnt; ++i){
        getToken(i, nLine)->Box.State.W1 = (i     == 0       ) ? 0 : wordSpace[0];
        getToken(i, nLine)->Box.State.W2 = (i + 1 == tokenCnt) ? 0 : wordSpace[1];
    }

    switch(lineAlign()){
        case LALIGN_JUSTIFY:
            {
                if(!bCREnd){
                    LineJustifyPadding(nLine);
                }
                break;
            }
        case LALIGN_DISTRIBUTED:
            {
                LineDistributedPadding(nLine);
                break;
            }
        default:
            {
                break;
            }
    }

    SetLineTokenStartX(nLine);
    SetLineTokenStartY(nLine);
}

void XMLTypeset::SetLineTokenStartX(int nLine)
{
    if(!lineValid(nLine)){
        throw fflerror("invalid line: %d", nLine);
    }

    int nLineStartX = 0;
    switch(lineAlign()){
        case LALIGN_RIGHT:
            {
                nLineStartX = MaxLineWidth() - LineFullWidth(nLine);
                break;
            }
        case LALIGN_CENTER:
            {
                nLineStartX = (MaxLineWidth() - LineFullWidth(nLine)) / 2;
                break;
            }
    }

    int nCurrX = nLineStartX;
    for(auto &rstToken: m_lineList[nLine].content){
        nCurrX += rstToken.Box.State.W1;
        rstToken.Box.State.X = nCurrX;

        nCurrX += rstToken.Box.Info.W;
        nCurrX += rstToken.Box.State.W2;
    }
}

// get max H2 for an interval
// assume:
//      W1/W/W2/X of tokens in nLine is initialized
// return:
//      max H2, or negative if there is no tokens for that interval
int XMLTypeset::LineIntervalMaxH2(int nLine, int nIntervalStartX, int nIntervalWidth) const
{
    //    This function only take care of nLine
    //    has nothing to do with (n-1)th or (n+1)th Line
    //
    // ---------+----+----+-------------+-------  <-BaseLine
    // |        |    |    |             |
    // |        | W2 | W1 |             |
    // |        |    +----+-------------+
    // |        |    |
    // +--------+----+
    //             |    |
    //             |    |
    //             |    |
    //        +----+----+-----+
    //        | W1 |    | W2  |
    //        |  ->|    |<-   |
    //            Interval
    //
    //  Here we take the padding space as part of the token.
    //  If not, token in (n + 1)-th line may go through this space
    //
    //  but token in (n + 1)-th line only count for the interval
    //  W1/W2 won't count for here, as shown

    if(!lineValid(nLine)){
        throw fflerror("invalid line: %d", nLine);
    }

    if(nIntervalWidth == 0){
        throw fflerror("zero-length interval provided");
    }

    int nMaxH2 = -1;
    for(int nIndex = 0; nIndex < lineTokenCount(nLine); ++nIndex){
        auto pToken = getToken(nIndex, nLine);
        int nW  = pToken->Box.Info .W;
        int nX  = pToken->Box.State.X;
        int nW1 = pToken->Box.State.W1;
        int nW2 = pToken->Box.State.W2;
        int nH2 = pToken->Box.State.H2;

        if(mathf::intervalOverlap<int>(nX - nW1, nW1 + nW + nW2, nIntervalStartX, nIntervalWidth)){
            nMaxH2 = (std::max<int>)(nMaxH2, nH2);
        }
    }

    // maybe the line is not long enough to cover interval
    // in this case we return -1
    return nMaxH2;
}

// get the minmal (compact) possible start Y of a token in nth line
// assume:
//      (n-1)th line is valid if exists
// return:
//      possible minimal Y for current box
//
// for nth line we try to calculate possible minmal Y for each token then
// take the max-min as the line start Y, here we don't permit tokens in nth
// line go through upper than (n - 1)th baseLine
int XMLTypeset::LineTokenBestY(int nLine, int nTokenX, int nTokenWidth, int nTokenHeight) const
{
    if(!lineValid(nLine)){
        throw fflerror("invalid line: %d", nLine);
    }

    if(nTokenWidth <= 0 || nTokenHeight <= 0){
        throw fflerror("invalid token size: width = %d, height = %d", nTokenWidth, nTokenHeight);
    }

    if(nLine == 0){
        return nTokenHeight - 1;
    }

    if(int nMaxH2 = LineIntervalMaxH2(nLine - 1, nTokenX, nTokenWidth); nMaxH2 >= 0){
        return m_lineList[nLine - 1].startY + to_d(nMaxH2) + m_lineSpace + nTokenHeight;
    }

    // negative nMaxH2 means the (n - 1)th line is not long enough
    // directly let the token touch the (n - 1)th baseline
    //
    //           +----------+
    //           |          |
    // +-------+ |          |
    // | Words | |   Emoji  |  Words
    // +-------+-+----------+------------------
    //           |          |   +----------+
    //           +----------+   |          |
    //                          |   TB0    |
    //                +-------+ |          |
    //                | Words | |  Emoji   |  Words
    //                +-------+-+----------+-------
    //                          |          |
    //                          +----------+
    return m_lineList[nLine - 1].startY + m_lineSpace + nTokenHeight;
}

// get start Y of current line
// assume:
//      1. (nth - 1) line is valid
//      2. nLine is padded already, StartX, W/W1/W2 are OK now
int XMLTypeset::LineNewStartY(int nLine)
{
    //           +----------+
    //           |          |
    // +-------+ |          |
    // | Words | |   Emoji  |  Words
    // +-------+-+----------+------------------
    //           |          |   +----------+
    //           +----------+   |          |
    //                          |          |
    //                +-------+ |          |
    //                | Words | |   Emoji  |  Words
    //                +-------+-+----------+-------
    //                          |          |
    //                          +----------+
    //
    // for nLine we use W only, but for nLine - 1, we use W1 + W + W2
    // reason is clear since W1/2 of nLine won't show, then use them are
    // wasteful for space
    //
    //          +---+-----+--+---+-------+
    //          |   |     |  |   |       |
    //          | 1 |  W  |2 | 1 |   W   |...
    //          |   |     |  |   |       |
    //          +---+-----+--+---+-------+
    //
    //                   +--------+
    //                   |        |
    //                   |   W    |
    //                   |        |
    //

    if(!lineValid(nLine)){
        throw fflerror("invalid line: %d", nLine);
    }

    if(nLine == 0){
        return LineMaxHk(0, 1);
    }

    if(!CanThrough()){
        return LineReachMaxY(nLine - 1) + m_lineSpace + LineMaxHk(nLine, 1);
    }

    if(lineTokenCount(nLine) == 0){
        throw fflerror("empty line detected: %d", nLine);
    }

    int nCurrentY = -1;
    for(int nIndex = 0; nIndex < lineTokenCount(nLine); ++nIndex){
        auto pToken = getToken(nIndex, nLine);
        int nX  = pToken->Box.State.X;
        int nW  = pToken->Box.Info .W;
        int nH1 = pToken->Box.State.H1;

        // LineTokenBestY() already take m_lineSpace into consideration
        nCurrentY = (std::max<int>)(nCurrentY, LineTokenBestY(nLine, nX, nW, nH1));
    }

    // not done yet, think about the following situation
    // if we only check boxes in nth line we may make (n - 1)th line go cross
    //
    // +-----------+ +----+
    // |           | |    |
    // +-----------+-+    + ------ (n - 1)th
    //        +----+ |    |
    //        |    | |    |
    //  +--+  |    | |    |
    //  |  |  |    | |    |
    // -+--+--+----+ |    | ------ nth
    //               +----+

    return to_d((std::max<int>)(nCurrentY, LineReachMaxY(nLine - 1) + 1));
}

void XMLTypeset::SetLineTokenStartY(int nLine)
{
    if(!lineValid(nLine)){
        throw fflerror("invalid line: %d", nLine);
    }

    m_lineList[nLine].startY = to_d(LineNewStartY(nLine));
    for(int nIndex = 0; nIndex < lineTokenCount(nLine); ++nIndex){
        auto pToken = getToken(nIndex, nLine);
        pToken->Box.State.Y = m_lineList[nLine].startY - pToken->Box.State.H1;
    }
}

void XMLTypeset::checkDefaultFontEx() const
{
    const uint64_t u64key = utf8f::buildU64Key(m_font, m_fontSize, 0, utf8f::peekUTF8Code("0"));
    if(!g_fontexDB->retrieve(u64key)){
        throw fflerror("invalid default font: font = %d, fontsize = %d", to_d(m_font), to_d(m_fontSize));
    }
}

TOKEN XMLTypeset::buildUTF8Token(int leaf, uint8_t nFont, uint8_t nFontSize, uint8_t nFontStyle, uint32_t nUTF8Code) const
{
    TOKEN stToken;
    std::memset(&(stToken), 0, sizeof(stToken));
    auto nU64Key = utf8f::buildU64Key(nFont, nFontSize, nFontStyle, nUTF8Code);

    stToken.leaf = leaf;
    if(auto pTexture = g_fontexDB->retrieve(nU64Key)){
        int nBoxW = -1;
        int nBoxH = -1;
        if(!SDL_QueryTexture(pTexture, nullptr, nullptr, &nBoxW, &nBoxH)){
            stToken.Box.Info.W      = nBoxW;
            stToken.Box.Info.H      = nBoxH;
            stToken.Box.State.H1    = stToken.Box.Info.H;
            stToken.Box.State.H2    = 0;
            stToken.UTF8Char.U64Key = nU64Key;
            return stToken;
        }
        throw fflerror("SDL_QueryTexture(%p) failed", to_cvptr(pTexture));
    }

    nU64Key = utf8f::buildU64Key(m_font, m_fontSize, 0, nUTF8Code);
    if(g_fontexDB->retrieve(nU64Key)){
        throw fflerror("can't find texture for UTF8: %" PRIX32, nUTF8Code);
    }

    nU64Key = utf8f::buildU64Key(m_font, m_fontSize, nFontStyle, utf8f::peekUTF8Code("0"));
    if(g_fontexDB->retrieve(nU64Key)){
        throw fflerror("invalid font style: %" PRIX8, nFontStyle);
    }

    // invalid font given
    // use system default font, don't fail it

    nU64Key = utf8f::buildU64Key(m_font, m_fontSize, nFontStyle, nUTF8Code);
    if(auto pTexture = g_fontexDB->retrieve(nU64Key)){
        int nBoxW = -1;
        int nBoxH = -1;
        if(!SDL_QueryTexture(pTexture, nullptr, nullptr, &nBoxW, &nBoxH)){
            g_log->addLog(LOGTYPE_WARNING, "Fallback to default font: font: %d -> %d, fontsize: %d -> %d", to_d(nFont), to_d(m_font), to_d(nFontSize), to_d(m_fontSize));
            stToken.Box.Info.W      = nBoxW;
            stToken.Box.Info.H      = nBoxH;
            stToken.Box.State.H1    = stToken.Box.Info.H;
            stToken.Box.State.H2    = 0;
            stToken.UTF8Char.U64Key = nU64Key;
            return stToken;
        }
        throw fflerror("SDL_QueryTexture(%p) failed", to_cvptr(pTexture));
    }
    throw fflerror("fallback to default font failed: font: %d -> %d, fontsize: %d -> %d", to_d(nFont), to_d(m_font), to_d(nFontSize), to_d(m_fontSize));
}

TOKEN XMLTypeset::buildEmojiToken(int leaf, uint32_t emoji) const
{
    TOKEN token;
    std::memset(&(token), 0, sizeof(token));
    token.leaf = leaf;

    int tokenW     = -1;
    int tokenH     = -1;
    int h1         = -1;
    int fps        = -1;
    int frameCount = -1;

    if(((emoji << 8) >> 8) != emoji){
        throw fflerror("invalid emoji key: %llu", to_llu(emoji));
    }

    emoji <<= 8;
    if(g_emojiDB->retrieve(emoji, 0, 0, &tokenW, &tokenH, &h1, &fps, &frameCount)){
        token.Box.Info.W       = tokenW;
        token.Box.Info.H       = tokenH;
        token.Box.State.H1     = h1;
        token.Box.State.H2     = tokenH - h1;
        token.Emoji.U32Key     = emoji;
        token.Emoji.FPS        = fps;
        token.Emoji.FrameCount = frameCount;
    }
    return token;
}

TOKEN XMLTypeset::createToken(int leaf, int leafOff) const
{
    switch(auto &rstLeaf = m_paragraph.leafRef(leaf); rstLeaf.type()){
        case LEAF_UTF8GROUP:
            {
                auto nFont      = rstLeaf.font()     .value_or(m_font);
                auto nFontSize  = rstLeaf.fontSize() .value_or(m_fontSize);
                auto nFontStyle = rstLeaf.fontStyle().value_or(m_fontStyle);
                return buildUTF8Token(leaf, nFont, nFontSize, nFontStyle, rstLeaf.peekUTF8Code(leafOff));
            }
        case LEAF_EMOJI:
            {
                return buildEmojiToken(leaf, rstLeaf.emojiU32Key());
            }
        case LEAF_IMAGE:
            {
                throw fflerror("not supported yet");
            }
        default:
            {
                throw fflerror("invalid type: %d", rstLeaf.type());
            }
    }
}

std::vector<TOKEN> XMLTypeset::createTokenLine(int leaf, int leafOff, std::vector<TOKEN> *tokenListPtr) const
{
    std::vector<TOKEN> tokenList;
    if(tokenListPtr){
        std::swap(tokenList, *tokenListPtr);
    }

    tokenList.clear();
    if(m_paragraph.leafRef(leaf).wrap().value_or(true)){
        tokenList.push_back(createToken(leaf, leafOff));
    }
    else{
        if(leafOff != 0){
            throw fflerror("pick up token in middle of non-wrap leaf");
        }

        if(m_paragraph.leafRef(leaf).type() != LEAF_UTF8GROUP){
            throw fflerror("non-text leaf node has wrap attribute disabled");
        }

        for(int i = 0; i < m_paragraph.leafRef(leaf).length(); ++i){
            tokenList.push_back(createToken(leaf, i));
        }
    }
    return tokenList;
}

// rebuild the board from token (x, y)
// assumen:
//      (0, 0) ~ prevTokenLoc(x, y) are valid
// output:
//      valid board
// notice:
// this function may get called after any XML update, the (x, y) in XMLTypeset may be
// invalid but as long as it's valid in XMLParagraph it's well-defined
void XMLTypeset::buildTypeset(int x, int y)
{
    for(int line = 0; line < y; ++line){
        if(lineEmpty(line)){
            throw fflerror("line %d is empty", line);
        }
    }

    int leaf    = 0;
    int leafOff = 0;

    if(x || y){
        const auto [prevX, prevY] = prevTokenLoc(x, y);
        const auto [prevLeaf, prevLeafOff] = leafLocInXMLParagraph(prevX, prevY);

        int advanced = 0;
        std::tie(leaf, leafOff, advanced) = m_paragraph.nextLeafOff(prevLeaf, prevLeafOff, 1);

        if(advanced == 0){
            // only prev location is valid
            // from (x, y) all token should be removed
            m_leaf2TokenLoc.resize(prevLeaf + 1);

            m_lineList.resize(prevY + 1);
            m_lineList[prevY].startY = 0;
            m_lineList[prevY].content.resize(prevX + 1);

            resetOneLine(prevY, true);
            resetBoardPixelRegion();
            return;
        }
    }

    // we start to push token from (leaf, leafOff)
    // if current it's a utf8String and not start from the beginning, we should keep the leaf record
    m_leaf2TokenLoc.resize(leaf + to_d(leafOff > 0));

    m_lineList.resize(y + 1);
    m_lineList[y].startY = 0;
    m_lineList[y].content.resize(x);

    int currLine = y;
    int advanced = -1;

    std::vector<TOKEN> tokenList;
    for(; (advanced < 0) || (advanced == to_d(tokenList.size())); std::tie(leaf, leafOff, advanced) = m_paragraph.nextLeafOff(leaf, leafOff, tokenList.size())){
        tokenList = createTokenLine(leaf, leafOff, &tokenList); // TODO: well-defined ???
        if(addRawTokenLine(currLine, tokenList)){
            if(leafOff == 0){
                m_leaf2TokenLoc.push_back({m_lineList[currLine].content.size() - 1, currLine});
            }
            continue;
        }

        resetOneLine(currLine, false);

        currLine++;
        m_lineList.resize(currLine + 1);

        if(!addRawTokenLine(currLine, tokenList)){
            throw fflerror("insert token to a new line failed: line = %d", currLine);
        }

        if(leafOff == 0){
            m_leaf2TokenLoc.push_back({m_lineList[currLine].content.size() - 1, currLine});
        }
    }

    resetOneLine(currLine, true);
    resetBoardPixelRegion();
}

std::tuple<int, int> XMLTypeset::leafLocInXMLParagraph(int tokenX, int tokenY) const
{
    if(!tokenLocValid(tokenX, tokenY)){
        throw fflerror("invalid token location: (%d, %d)", tokenX, tokenY);
    }

    const int leaf = getToken(tokenX, tokenY)->leaf;
    const auto [startTokenX, startTokenY] = leafTokenLoc(leaf);

    if(getToken(startTokenX, startTokenY)->leaf != leaf){
        throw fflerror("invalid start token location");
    }

    if(tokenY == startTokenY){
        return {leaf, tokenX - startTokenX};
    }

    int tokenOff = lineTokenCount(startTokenY) - startTokenX;
    for(int line = startTokenY + 1; line < tokenY; ++line){
        tokenOff += lineTokenCount(line);
    }

    tokenOff += tokenX;
    return {leaf, tokenOff};
}

void XMLTypeset::resetBoardPixelRegion()
{
    if(lineCount() == 0){
        m_px = 0;
        m_py = 0;
        m_pw = 0;
        m_ph = 0;
        return;
    }

    int nMaxPX = 0;
    int nMaxPY = 0;
    int nMinPX = INT_MAX;
    int nMinPY = INT_MAX;

    for(int nLine = 0; lineValid(nLine); ++nLine){
        if(!lineTokenCount(nLine)){
            throw fflerror("found empty line in XMLTypeset: line = %d", nLine);
        }

        nMaxPX = (std::max<int>)(nMaxPX, LineReachMaxX(nLine));
        nMaxPY = (std::max<int>)(nMaxPY, LineReachMaxY(nLine));
        nMinPX = (std::min<int>)(nMinPX, LineReachMinX(nLine));
        nMinPY = (std::min<int>)(nMinPY, LineReachMinY(nLine));
    }

    m_px = nMinPX;
    m_py = nMinPY;
    m_pw = nMaxPX + 1 - nMinPX;
    m_ph = nMaxPY + 1 - nMinPY;
}

std::tuple<int, int> XMLTypeset::prevTokenLoc(int nX, int nY) const
{
    if(!tokenLocValid(nX, nY)){
        throw fflerror("invalid token location: (%d, %d)", nX, nY);
    }

    if(nX == 0 && nY == 0){
        throw fflerror("try find token before (0, 0)");
    }

    if(nX){
        return {nX - 1, nY};
    }else{
        return {lineTokenCount(nY - 1), nY - 1};
    }
}

std::tuple<int, int> XMLTypeset::NextTokenLoc(int nX, int nY) const
{
    if(!tokenLocValid(nX, nY)){
        throw fflerror("invalid token location: (%d, %d)", nX, nY);
    }

    if(nX == 0 && nY == 0){
        throw fflerror("try find token location before (0, 0)");
    }

    if(nX){
        return {nX - 1, nY};
    }else{
        return {lineTokenCount(nY - 1), nY - 1};
    }
}

void XMLTypeset::deleteToken(int x, int y, int tokenCount)
{
    if(!tokenLocValid(x, y)){
        throw fflerror("invalid token location: (%d, %d)", x, y);
    }

    if(tokenCount <= 0){
        return;
    }

    const auto [leaf, leafOff] = leafLocInXMLParagraph(x, y);
    m_paragraph.deleteToken(leaf, leafOff, tokenCount);

    if(m_paragraph.empty()){
        clear();
    }
    else{
        buildTypeset(x, y);
    }
}

void XMLTypeset::insertUTF8String(int x, int y, const char *text)
{
    if(!cursorLocValid(x, y)){
        throw fflerror("invalid location: (%d, %d)", x, y);
    }

    tinyxml2::XMLPrinter printer;
    const auto fnParXMLString = [&printer](const char *rawString) -> const char *
    {
        if(!rawString){
            throw fflerror("invalid null raw string pointer");
        }

        tinyxml2::XMLDocument xmlDoc;
        const char *xmlString = "<par></par>";

        if(xmlDoc.Parse(xmlString) != tinyxml2::XML_SUCCESS){
            throw fflerror("parse xml template failed: %s", xmlString);
        }

        // to support <, >, / in xml string
        // don't directly pass the raw string to addParXML
        xmlDoc.RootElement()->SetText(rawString);
        xmlDoc.Print(&printer);
        return printer.CStr();
    };

    if(m_paragraph.empty()){
        m_paragraph.loadXML(fnParXMLString(text));
        if(m_paragraph.leafCount() > 0){
            buildTypeset(0, 0);
        }
        return;
    }

    // XMLParagraph doesn't have cursor
    // need to parse here

    if(x == 0 && y == 0){
        if(m_paragraph.leafRef(0).type() != LEAF_UTF8GROUP){
            m_paragraph.insertLeafXML(0, fnParXMLString(text));
        }
        else{
            m_paragraph.insertUTF8String(0, 0, text);
        }
        buildTypeset(0, 0);
        return;
    }

    // we are appending at the last location
    // can't call prevTokenLoc() because this tokenLoc is invalid

    if((y == lineCount() - 1) && (x == lineTokenCount(y))){
        if(m_paragraph.backLeafRef().type() != LEAF_UTF8GROUP){
            m_paragraph.insertLeafXML(leafCount(), fnParXMLString(text));
        }
        else{
            m_paragraph.insertUTF8String(leafCount() - 1, m_paragraph.backLeafRef().utf8CharOffRef().size(), text);
        }
        buildTypeset(0, 0);
        return;
    }

    const auto [prevTX, prevTY] = prevTokenLoc(x, y);
    const auto currLeaf = getToken(x, y)->leaf;
    const auto prevLeaf = getToken(prevTX, prevTY)->leaf;

    if(prevLeaf == currLeaf){
        if(m_paragraph.leafRef(prevLeaf).type() != LEAF_UTF8GROUP){
            throw fflerror("only UTF8 leaf can have length great than 1");
        }

        const auto [leaf, leafOff] = leafLocInXMLParagraph(x, y);
        m_paragraph.insertUTF8String(currLeaf, leafOff, text);
    }
    else{
        if(m_paragraph.leafRef(prevLeaf).type() == LEAF_UTF8GROUP){
            m_paragraph.insertUTF8String(prevLeaf, m_paragraph.leafRef(prevLeaf).utf8CharOffRef().size(), text);
        }
        if(m_paragraph.leafRef(currLeaf).type() == LEAF_UTF8GROUP){
            m_paragraph.insertUTF8String(currLeaf, 0, text);
        }
        else{
            m_paragraph.insertLeafXML(currLeaf, fnParXMLString(text));
        }
    }

    buildTypeset(0, 0);
}

void XMLTypeset::drawEx(int dstX, int dstY, int srcX, int srcY, int srcW, int srcH) const
{
    if(!mathf::rectangleOverlap<int>(srcX, srcY, srcW, srcH, px(), py(), pw(), ph())){
        return;
    }

    const int dstDX = dstX - srcX;
    const int dstDY = dstY - srcY;

    uint32_t fgColorVal = 0;
    uint32_t bgColorVal = 0;

    int lastLeaf = -1;
    for(int line = 0; line < lineCount(); ++line){
        for(int token = 0; token < lineTokenCount(line); ++token){
            const auto tokenPtr = getToken(token, line);
            const auto &leaf = m_paragraph.leafRef(tokenPtr->leaf);

            if(lastLeaf != tokenPtr->leaf){
                fgColorVal  = leaf.  color().value_or(  color());
                bgColorVal  = leaf.bgColor().value_or(bgColor());
                lastLeaf    = tokenPtr->leaf;
            }

            // draw bgColor
            // background can be bigger than tokenbox by W1/W2

            if(colorf::A(bgColorVal)){
                int bgBoxX = tokenPtr->Box.State.X - tokenPtr->Box.State.W1;
                int bgBoxY = tokenPtr->Box.State.Y;
                int bgBoxW = tokenPtr->Box.Info.W + tokenPtr->Box.State.W1 + tokenPtr->Box.State.W2;
                int bgBoxH = tokenPtr->Box.Info.H;

                if(mathf::rectangleOverlapRegion(srcX, srcY, srcW, srcH, bgBoxX, bgBoxY, bgBoxW, bgBoxH)){
                    g_sdlDevice->fillRectangle(bgColorVal, bgBoxX + dstDX, bgBoxY + dstDY, bgBoxW, bgBoxH);
                }
            }

            int boxX = tokenPtr->Box.State.X;
            int boxY = tokenPtr->Box.State.Y;
            int boxW = tokenPtr->Box.Info.W;
            int boxH = tokenPtr->Box.Info.H;

            if(!mathf::rectangleOverlapRegion(srcX, srcY, srcW, srcH, boxX, boxY, boxW, boxH)){
                continue;
            }

            const int drawDstX = boxX + dstDX;
            const int drawDstY = boxY + dstDY;

            const int dx = boxX - tokenPtr->Box.State.X;
            const int dy = boxY - tokenPtr->Box.State.Y;

            switch(leaf.type()){
                case LEAF_UTF8GROUP:
                    {
                        if(auto texPtr = g_fontexDB->retrieve(tokenPtr->UTF8Char.U64Key)){
                            SDLDeviceHelper::EnableTextureModColor enableMod(texPtr, fgColorVal);
                            g_sdlDevice->drawTexture(texPtr, drawDstX, drawDstY, dx, dy, boxW, boxH);
                        }
                        else{
                            g_sdlDevice->drawRectangle(colorf::compColor(bgColorVal), drawDstX, drawDstY, boxW, boxH);
                        }

                        if(g_clientArgParser->drawTokenFrame){
                            g_sdlDevice->drawRectangle(colorf::MAGENTA + colorf::A_SHF(255), drawDstX, drawDstY, boxW, boxH);
                        }
                        break;
                    }
                case LEAF_IMAGE:
                    {
                        break;
                    }
                case LEAF_EMOJI:
                    {
                        const auto emojiKey = [tokenPtr]() -> uint32_t
                        {
                            if(tokenPtr->Emoji.FrameCount && tokenPtr->Emoji.FPS){
                                return (tokenPtr->Emoji.U32Key & 0XFFFFFF00) + tokenPtr->Emoji.Frame % tokenPtr->Emoji.FrameCount;
                            }
                            return tokenPtr->Emoji.U32Key & 0XFFFFFF00;
                        }();

                        int xOnTex = 0;
                        int yOnTex = 0;

                        if(auto texPtr = g_emojiDB->retrieve(emojiKey, &xOnTex, &yOnTex, 0, 0, 0, 0, 0)){
                            SDLDeviceHelper::EnableTextureModColor enableMod(texPtr, m_imageMaskColor);
                            g_sdlDevice->drawTexture(texPtr, drawDstX, drawDstY, xOnTex + dx, yOnTex + dy, boxW, boxH);
                        }
                        else{
                            g_sdlDevice->drawRectangle(colorf::compColor(bgColorVal), drawDstX, drawDstY, boxW, boxH);
                        }
                        break;
                    }
            }

        }
    }

    if(g_clientArgParser->drawBoardFrame){
        g_sdlDevice->drawRectangle(colorf::YELLOW + colorf::A_SHF(255), dstX, dstY, srcW, srcH);
    }
}

void XMLTypeset::update(double fMS)
{
    for(int leaf = 0; leaf < m_paragraph.leafCount(); ++leaf){
        if(m_paragraph.leafRef(leaf).type() == LEAF_EMOJI){
            const auto [x, y] = leafTokenLoc(leaf);
            if(auto pToken= getToken(x, y); pToken->Emoji.FPS != 0){
                double fPeroidMS = 1000.0 / pToken->Emoji.FPS;
                double fCurrTick = fMS + pToken->Emoji.Tick;

                auto nAdvancedFrame  = to_u8(fCurrTick / fPeroidMS);
                pToken->Emoji.Tick   = to_u8(fCurrTick - nAdvancedFrame * fPeroidMS);
                pToken->Emoji.Frame += nAdvancedFrame;
            }
        }
    }
}

std::string XMLTypeset::getText(bool textOnly) const
{
    std::string plainString;
    for(int i = 0; i < m_paragraph.leafCount(); ++i){
        switch(auto leafType = m_paragraph.leafRef(i).type()){
            case LEAF_UTF8GROUP:
                {
                    plainString += m_paragraph.leafRef(i).UTF8Text();
                    break;
                }
            case LEAF_IMAGE:
                {
                    if(!textOnly){
                        plainString += str_printf("\\image{0x%016" PRIx64 "}", m_paragraph.leafRef(i).ImageU64Key());
                    }
                    break;
                }
            case LEAF_EMOJI:
                {
                    if(!textOnly){
                        plainString += str_printf("\\emoji{0x%016" PRIu64 "}", to_u64(m_paragraph.leafRef(i).emojiU32Key()));
                    }
                    break;
                }
            default:
                {
                    throw fflerror("invalid leaf type: %d", leafType);
                }
        }
    }
    return plainString;
}

int XMLTypeset::GetTokenWordSpace(int nX, int nY) const
{
    if(!tokenLocValid(nX, nY)){
        throw fflerror("invalid token location: (%d, %d)", nX, nY);
    }
    return m_wordSpace;
}

int XMLTypeset::LineReachMaxX(int nLine) const
{
    if(!lineValid(nLine)){
        throw fflerror("invalid line: %d", nLine);
    }

    if(lineTokenCount(nLine) == 0){
        throw fflerror("invalie empty line: %d", nLine);
    }

    auto pToken = GetLineBackToken(nLine);
    return pToken->Box.State.X + pToken->Box.Info.W;
}

int XMLTypeset::LineReachMaxY(int nLine) const
{
    if(!lineValid(nLine)){
        throw fflerror("invalid line: %d", nLine);
    }
    return m_lineList[nLine].startY + LineMaxHk(nLine, 2);
}

int XMLTypeset::LineReachMinX(int nLine) const
{
    if(!lineValid(nLine)){
        throw fflerror("invalid line: %d", nLine);
    }

    if(lineTokenCount(nLine) == 0){
        throw fflerror("invalie empty line: %d", nLine);
    }

    return getToken(0, nLine)->Box.State.X;
}

int XMLTypeset::LineReachMinY(int nLine) const
{
    if(!lineValid(nLine)){
        throw fflerror("invalid line: %d", nLine);
    }
    return m_lineList[nLine].startY - LineMaxHk(nLine, 1) + 1;
}

int XMLTypeset::LineMaxHk(int nLine, int k) const
{
    // nHk = 1: get MaxH1
    // nHk = 2: get MaxH2

    if(!lineValid(nLine)){
        throw fflerror("invalid line: %d", nLine);
    }

    if(k != 1 && k != 2){
        throw fflerror("invalid argument k: %d", k);
    }

    int nCurrMaxHk = 0;
    for(int nIndex = 0; nIndex < lineTokenCount(nLine); ++nIndex){
        nCurrMaxHk = (std::max<int>)(nCurrMaxHk, (k == 1) ? getToken(nIndex, nLine)->Box.State.H1 : getToken(nIndex, nLine)->Box.State.H2);
    }
    return nCurrMaxHk;
}

int XMLTypeset::lineAlign() const
{
    if(MaxLineWidth() == 0){
        return LALIGN_LEFT;
    }
    return m_lineAlign;
}

std::tuple<int, int> XMLTypeset::locCursor(int xOffPixel, int yOffPixel) const
{
    if(empty()){
        return {0, 0};
    }

    const auto cursorY = [this, yOffPixel]() -> int
    {
        const auto pLine = std::lower_bound(m_lineList.begin(), m_lineList.end(), yOffPixel, [](const auto &parmX, const auto &parmY)
        {
            return parmX.startY < parmY;
        });

        if(pLine == m_lineList.end()){
            return lineCount() - 1;
        }
        return to_d(std::distance(m_lineList.begin(), pLine));
    }();

    if(lineEmpty(cursorY)){
        return {0, cursorY};
    }

    const auto cursorX = [cursorY, xOffPixel, this]() -> int
    {
        const auto iter_begin = m_lineList.at(cursorY).content.begin();
        const auto iter_end   = m_lineList.at(cursorY).content.end();

        const auto pBox = std::lower_bound(iter_begin, iter_end, xOffPixel, [](const auto &parmX, const auto &parmY)
        {
            return parmX.Box.State.X < parmY;
        });
        return std::distance(m_lineList.at(cursorY).content.begin(), pBox);
    }();

    return {cursorX, cursorY};
}

bool XMLTypeset::locInToken(int xOffPixel, int yOffPixel, const TOKEN *pToken, bool withPadding)
{
    if(!pToken){
        throw fflerror("null token pointer");
    }

    const int startX = pToken->Box.State.X - (withPadding ? pToken->Box.State.W1 : 0);
    const int startY = pToken->Box.State.Y;
    const int boxW   = pToken->Box.Info.W + (withPadding ? (pToken->Box.State.W1 + pToken->Box.State.W2) : 0);
    const int boxH   = pToken->Box.Info.H;

    return mathf::pointInRectangle(xOffPixel, yOffPixel, startX, startY, boxW, boxH);
}

std::tuple<int, int> XMLTypeset::locToken(int xOffPixel, int yOffPixel, bool withPadding) const
{
    if(yOffPixel < 0 || yOffPixel >= ph()){
        return {-1, -1};
    }

    const auto [cursorX, cursorY] = locCursor(xOffPixel, yOffPixel);
    if(!cursorLocValid(cursorX, cursorY)){
        throw fflerror("invalid cursor location: (%d, %d)", cursorX, cursorY);
    }

    for(const int tokenX: {cursorX - 1, cursorX, cursorX + 1}){
        if(!tokenLocValid(tokenX, cursorY)){
            continue;
        }

        if(locInToken(xOffPixel, yOffPixel, getToken(tokenX, cursorY), withPadding)){
            return {tokenX, cursorY};
        }
    }
    return {-1, -1};
}

bool XMLTypeset::blankToken(int x, int y) const
{
    if(!tokenLocValid(x, y)){
        throw fflerror("invalid token location: x = %d, y = %d", x, y);
    }

    const auto tokenPtr = getToken(x, y);
    const auto &leaf = m_paragraph.leafRef(tokenPtr->leaf);

    const auto fnCheckBlank = [](uint64_t u64Key)
    {
        return (u64Key & 0X00000000FFFFFFFFULL) == ' ';
    };

    return (leaf.type() == LEAF_UTF8GROUP) && fnCheckBlank(tokenPtr->UTF8Char.U64Key);
}

void XMLTypeset::setLineWidth(int lineWidth)
{
    m_lineWidth = lineWidth;
    buildTypeset(0, 0);
}

int XMLTypeset::getDefaultFontHeight() const
{
    if(auto texPtr = g_fontexDB->retrieve(utf8f::buildU64Key(m_font, m_fontSize, m_fontStyle, utf8f::peekUTF8Code(" ")))){
        return SDLDeviceHelper::getTextureHeight(texPtr);
    }
    return 20;
}
