#include "sig/signal_types.h"
#include "util/math.h"

#include <unordered_map>

inline const std::unordered_map<ReasonType, Meta> reason_meta = {
    {
        ReasonType::None,                                     //
        {Severity::Low, Source::EMA, SignalClass::Entry, ""}  //
    },

    // Entry:
    {
        ReasonType::EmaCrossover,                                  //
        {Severity::High, Source::EMA, SignalClass::Entry, "ema⤯"}  //
    },
    {
        ReasonType::RsiCross50,                                        //
        {Severity::Medium, Source::RSI, SignalClass::Entry, "rsi↗50"}  //
    },
    {
        ReasonType::PullbackBounce,                                      //
        {Severity::Urgent, Source::Price, SignalClass::Entry, "b"}  //
    },
    {
        ReasonType::MacdBullishCross,                                  //
        {Severity::Medium, Source::MACD, SignalClass::Entry, "hist⤯"}  //
    },

    // Exit:
    {
        ReasonType::EmaCrossdown,                                 //
        {Severity::High, Source::EMA, SignalClass::Exit, "ema⤰"}  //
    },
    {
        ReasonType::RsiOverbought,                                    //
        {Severity::Medium, Source::RSI, SignalClass::Exit, "rsi↱70"}  //
    },
    {
        ReasonType::MacdBearishCross,                               //
        {Severity::High, Source::MACD, SignalClass::Exit, "hist⤰"}  //
    },
    // SR:
    {
        ReasonType::BrokeSupport,                              //
        {Severity::High, Source::SR, SignalClass::Exit, "⊥⤰"}  //
    },
    {
        ReasonType::BrokeResistance,                              //
        {Severity::Medium, Source::SR, SignalClass::Entry, "⊤⤯"}  //
    },
};

inline const std::unordered_map<StopHitType, Meta> stop_hit_meta = {
    {
        StopHitType::None,                                    //
        {Severity::Low, Source::None, SignalClass::None, ""}  //
    },

    {
        StopHitType::StopLossHit,                                     //
        {Severity::Urgent, Source::Stop, SignalClass::Exit, "stop⤰"}  //
    },
    {
        StopHitType::TimeExit,                                        //
        {Severity::Urgent, Source::Stop, SignalClass::Exit, "time⨯"}  //
    },
    {
        StopHitType::StopProximity,                                 //
        {Severity::High, Source::Stop, SignalClass::Exit, "stop⨯"}  //
    },
    {
        StopHitType::StopInATR,                                     //
        {Severity::High, Source::Stop, SignalClass::Exit, "stop!"}  //
    },
};

inline const std::unordered_map<HintType, Meta> hint_meta = {
    {
        HintType::None,                                       //
        {Severity::Low, Source::None, SignalClass::None, ""}  //
    },

    // Entry
    {
        HintType::Ema9ConvEma21,                                        //
        {Severity::Medium, Source::EMA, SignalClass::Entry, "ema9↗21"}  //
    },
    {
        HintType::RsiConv50,                                           //
        {Severity::Medium, Source::RSI, SignalClass::Entry, "rsi↝50"}  //
    },
    {
        HintType::MacdRising,                                          //
        {Severity::Medium, Source::MACD, SignalClass::Entry, "macd↗"}  //
    },
    {
        HintType::Pullback,                                                //
        {Severity::Medium, Source::Price, SignalClass::Entry, "pb"}  //
    },

    {
        HintType::RsiBullishDiv,                                   //
        {Severity::High, Source::RSI, SignalClass::Entry, "rsi⤯"}  //
    },
    {
        HintType::RsiBearishDiv,                                   //
        {Severity::High, Source::RSI, SignalClass::Entry, "rsi⤰"}  //
    },
    {
        HintType::MacdBullishDiv,                                    //
        {Severity::High, Source::MACD, SignalClass::Entry, "macd⤯"}  //
    },

    // Exit
    {
        HintType::Ema9DivergeEma21,                                 //
        {Severity::Low, Source::EMA, SignalClass::Exit, "ema9⤢21"}  //
    },
    {
        HintType::RsiDropFromOverbought,                            //
        {Severity::Medium, Source::RSI, SignalClass::Exit, "rsi⭛"}  //
    },
    {
        HintType::MacdPeaked,                                         //
        {Severity::Medium, Source::MACD, SignalClass::Exit, "macd▲"}  //
    },
    {
        HintType::Ema9Flattening,                                   //
        {Severity::Low, Source::EMA, SignalClass::Exit, "ema9↝21"}  //
    },

    // SR:
    {
        HintType::WithinTightRange,
        {Severity::High, Source::SR, SignalClass::Entry, "═"}  //
    },
    {
        HintType::NearWeakSupport,
        {Severity::Low, Source::SR, SignalClass::Entry, "⊥"}  //
    },
    {
        HintType::NearStrongSupport,
        {Severity::High, Source::SR, SignalClass::Entry, "⊥"}  //
    },
    {
        HintType::NearWeakResistance,
        {Severity::Low, Source::SR, SignalClass::Exit, "⊤"}  //
    },
    {
        HintType::NearStrongResistance,
        {Severity::High, Source::SR, SignalClass::Exit, "⊤"}  //
    },
};

template <>
SignalType<ReasonType, ReasonType::None>::SignalType(ReasonType type,
                                                     double scr,
                                                     const std::string& desc)
    : type{type}, score{sigmoid(scr, 5.0, 1.0)}, desc{desc} {
  auto it = reason_meta.find(type);
  meta = it == reason_meta.end() ? nullptr : &it->second;
}

template <>
SignalType<HintType, HintType::None>::SignalType(HintType type,
                                                 double scr,
                                                 const std::string& desc)
    : type{type}, score{sigmoid(scr, 5.0, 1.0)}, desc{desc} {
  auto it = hint_meta.find(type);
  meta = it == hint_meta.end() ? nullptr : &it->second;
}

template <>
SignalType<StopHitType, StopHitType::None>::SignalType(StopHitType type,
                                                       double scr,
                                                       const std::string& desc)
    : type{type}, score{sigmoid(scr, 5.0, 1.0)}, desc{desc} {
  auto it = stop_hit_meta.find(type);
  meta = it == stop_hit_meta.end() ? nullptr : &it->second;
}
