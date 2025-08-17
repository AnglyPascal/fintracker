#include "sig/signal_types.h"

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
        {Severity::Urgent, Source::Price, SignalClass::Entry, "bounce"}  //
    },
    {
        ReasonType::MacdHistogramCross,                                //
        {Severity::Medium, Source::MACD, SignalClass::Entry, "macd⤯"}  //
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
        {Severity::High, Source::MACD, SignalClass::Exit, "macd⤰"}  //
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
        HintType::Ema9ConvEma21,                                     //
        {Severity::Low, Source::EMA, SignalClass::Entry, "ema9↗21"}  //
    },
    {
        HintType::RsiConv50,                                        //
        {Severity::Low, Source::RSI, SignalClass::Entry, "rsi↝50"}  //
    },
    {
        HintType::MacdRising,                                          //
        {Severity::Medium, Source::MACD, SignalClass::Entry, "macd↗"}  //
    },
    {
        HintType::Pullback,                                                //
        {Severity::Medium, Source::Price, SignalClass::Entry, "pullback"}  //
    },

    {
        HintType::RsiBullishDiv,                                     //
        {Severity::Medium, Source::RSI, SignalClass::Entry, "rsi⤯"}  //
    },
    {
        HintType::RsiBearishDiv,                                     //
        {Severity::Medium, Source::RSI, SignalClass::Entry, "rsi⤰"}  //
    },
    {
        HintType::MacdBullishDiv,                                      //
        {Severity::Medium, Source::MACD, SignalClass::Entry, "macd⤯"}  //
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

    // Trends entry
    {
        HintType::PriceUp,                                           //
        {Severity::Medium, Source::Trend, SignalClass::Entry, "p↗"}  //
    },
    {
        HintType::PriceUpStrongly,                                   //
        {Severity::Medium, Source::Trend, SignalClass::Entry, "p⇗"}  //
    },
    {
        HintType::Ema21Up,                                             //
        {Severity::Medium, Source::Trend, SignalClass::Entry, "e21↗"}  //
    },
    {
        HintType::Ema21UpStrongly,                                     //
        {Severity::Medium, Source::Trend, SignalClass::Entry, "e21⇗"}  //
    },
    {
        HintType::RsiUp,                                             //
        {Severity::Medium, Source::Trend, SignalClass::Entry, "r↗"}  //
    },
    {
        HintType::RsiUpStrongly,                                     //
        {Severity::Medium, Source::Trend, SignalClass::Entry, "r⇗"}  //
    },

    // Trends exit
    {
        HintType::PriceDown,                                        //
        {Severity::Medium, Source::Trend, SignalClass::Exit, "p↘"}  //
    },
    {
        HintType::PriceDownStrongly,                                 //
        {Severity::Medium, Source::Trend, SignalClass::Entry, "p⇘"}  //
    },
    {
        HintType::Ema21Down,                                          //
        {Severity::Medium, Source::Trend, SignalClass::Exit, "e21↘"}  //
    },
    {
        HintType::Ema21DownStrongly,                                  //
        {Severity::Medium, Source::Trend, SignalClass::Exit, "e21⇘"}  //
    },
    {
        HintType::RsiDown,                                          //
        {Severity::Medium, Source::Trend, SignalClass::Exit, "r↘"}  //
    },
    {
        HintType::RsiDownStrongly,                                  //
        {Severity::Medium, Source::Trend, SignalClass::Exit, "r⇘"}  //
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
SignalType<ReasonType, ReasonType::None>::SignalType(ReasonType type)
    : type{type} {
  auto it = reason_meta.find(type);
  meta = it == reason_meta.end() ? nullptr : &it->second;
}

template <>
SignalType<HintType, HintType::None>::SignalType(HintType type) : type{type} {
  auto it = hint_meta.find(type);
  meta = it == hint_meta.end() ? nullptr : &it->second;
}

template <>
SignalType<StopHitType, StopHitType::None>::SignalType(StopHitType type)
    : type{type} {
  auto it = stop_hit_meta.find(type);
  meta = it == stop_hit_meta.end() ? nullptr : &it->second;
}
