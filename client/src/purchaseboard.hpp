/*
 * =====================================================================================
 *
 *       Filename: purchaseboard.hpp
 *        Created: 10/08/2017 19:06:52
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

#pragma once
#include <vector>
#include <cstdint>
#include "widget.hpp"
#include "serdesmsg.hpp"
#include "texvslider.hpp"
#include "tritexbutton.hpp"

class ProcessRun;
class PurchaseBoard: public Widget
{
    // NPC sell items in the small box
    // +----------------------------
    // | (19, 15)                (252, 15)
    // |  *-----+----------------*
    // |  |     |                |
    // |  |     |(57, 53)        |
    // |  +-----*----------------+
    // | (19, 57)
    // |  *-----+-----------
    // |  |     |
    // |  |     |
    // |  +-----+-----------
    // |
    // +--------------------

    private:
        constexpr static int m_startX = 19;
        constexpr static int m_startY = 15;

        constexpr static int m_boxW  = 57 - 19;
        constexpr static int m_boxH  = 53 - 15;
        constexpr static int m_lineH = 57 - 15;

    private:
        uint64_t m_npcUID = 0;

    private:
        int m_ext1Page = 0;
        int m_ext1PageGridSelected = -1;

    private:
        SDSellItemList m_sdSellItemList;

    private:
        int m_selected = 0;
        std::vector<uint32_t> m_itemList;

    private:
        TritexButton m_closeButton;
        TritexButton m_selectButton;

    private:
        TritexButton m_closeExt1Button;
        TritexButton m_leftExt1Button;
        TritexButton m_selectExt1Button;
        TritexButton m_rightExt1Button;

    private:
        TritexButton m_closeExt2Button;
        TritexButton m_selectExt2Button;

    private:
        TexVSlider m_slider;

    private:
        ProcessRun *m_processRun;

    public:
        PurchaseBoard(ProcessRun *, Widget * = nullptr, bool = false);

    public:
        void update(double) override;

    public:
        void drawEx(int, int, int, int, int, int) const override;

    public:
        bool processEvent(const SDL_Event &, bool) override;

    public:
        void loadSell(uint64_t, std::vector<uint32_t>);

    private:
        size_t getStartIndex() const;

    public:
        uint32_t selectedItemID() const;

    private:
        void setExtendedItemID(uint32_t);

    public:
        void onBuySucceed(uint64_t, uint32_t, uint32_t);
        void setSellItemList(SDSellItemList);

    private:
        int extendedBoardGfxID() const;

    private:
        int extendedPageCount() const;

    private:
        int getExt1PageGrid() const;
        static std::tuple<int, int, int, int> getExt1PageGridLoc(int, int);

    private:
        void drawExt1() const;
        void drawExt2() const;
        void drawExt1GridHoverText(int) const;

    private:
        std::tuple<uint32_t, uint32_t> getExtSelectedItemSeqID() const;

    private:
        size_t getItemPrice(int) const;

    private:
        void drawItemInGrid(const char8_t *, uint32_t, int, int) const;

    private:
        static SDL_Texture *getItemTexture(const char8_t *, uint32_t);
};
