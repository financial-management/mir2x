#pragma once
#include <cmath>
#include <tuple>
#include <vector>
#include <memory>
#include <cstdint>
#include "uidf.hpp"
#include "buffrecord.hpp"
#include "dbcomrecord.hpp"

class BattleObject;
class BaseBuffActAura;
class BaseBuffAct;
class BuffList;

// buff is an embeded class of BattleObject
// it always has a bound BO pointer and overlives buff
// it should be possible to put into BattleObject as an nested class but we split it out

class BaseBuff
{
    private:
        friend class BuffList;

    private:
        struct BuffActRunner
        {
            long tpsCount = 0;
            std::unique_ptr<BaseBuffAct> ptr;
        };

    protected:
        BattleObject * const m_bo;

    protected:
        const uint64_t m_fromUID;

    protected:
        const uint32_t m_fromBuff;

    protected:
        const uint32_t m_id;

    protected:
        double m_accuTime = 0.0;

    protected:
        std::vector<BuffActRunner> m_runList;

    public:
        BaseBuff(BattleObject *, uint64_t, uint32_t, uint32_t);

    public:
        virtual ~BaseBuff();

    public:
        uint32_t id() const
        {
            return m_id;
        }

        uint64_t fromUID() const
        {
            return m_fromUID;
        }

        uint32_t fromBuff() const
        {
            return m_fromBuff;
        }

    public:
        BattleObject * getBO()
        {
            return m_bo;
        }

        const BattleObject * getBO() const
        {
            return m_bo;
        }

    public:
        virtual bool done() const
        {
            if(getBR().duration <= 0){
                return false;
            }
            return std::lround(m_accuTime) >= getBR().duration;
        }

    public:
        virtual bool update(double ms)
        {
            m_accuTime += ms;
            runOnUpdate();

            if(done()){
                runOnDone();
                return true;
            }
            return false;
        }

    public:
        virtual void runOnUpdate();
        virtual void runOnTrigger(int);
        virtual void runOnDone();

    public:
        virtual void runOnMove(int);

    public:
        std::vector<BaseBuffActAura *> getAuraList();

    public:
        void sendAura(uint64_t);

    public:
        void dispatchAura();

    public:
        void updateAura(uint64_t);

    public:
        const BuffRecord &getBR() const
        {
            return DBCOM_BUFFRECORD(id());
        }

    public:
        const BuffRecord &fromBR() const
        {
            return DBCOM_BUFFRECORD(m_fromBuff);
        }

    public:
        const BuffRecord::BuffActRecordRef *fromAuraBAREF() const
        {
            for(const auto &baref: fromBR().actList){
                const auto &bar = DBCOM_BUFFACTRECORD(baref.name);
                fflassert(bar, baref.name);

                if(bar.isAura() && std::u8string_view(bar.aura.buff) == getBR().name){
                    return &baref;
                }
            }
            return nullptr;
        }
};
