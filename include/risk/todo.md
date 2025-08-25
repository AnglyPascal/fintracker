"Survive to Thrive" 
- Wide stops, multiple smaller positions, patience

1. StopLoss Changes
- Base on daily ATR not hourly (more appropriate for swing trades)
- Volatility-adjusted multipliers: 3x to 5x based on stock's daily ATR%
- Generous support buffers: 1-2% below support levels
- Market regime adjustments: Wider stops in choppy markets
- Time decay: Slightly wider first 2 days, then tighten
- Trailing activation: At 1R profit, tightness based on profit velocity

2. PositionSizing Changes
- Target risk: 0.5% standard, 1% max per position
- Portfolio risk cap: 6% total across all positions
- Max positions: 12 concurrent positions
- Daily limit: 1-3 new positions based on market regime
- Correlation adjustment: Reduce size for highly correlated positions
- Volatility adjustment: Reduce size for >5% daily ATR stocks
- Position building: Can scale up to 1.2% total risk over time

3. ProfitTarget Changes
- Dynamic R:R: 1.5 to 3.0 based on signal quality and resistance distance
- ATR-based targets: Incorporate expected volatility moves
- Resistance awareness: Use as targets but don't cap upside prematurely
- Trailing stop takeover: After 1R profit, let trailing manage exit

4. Risk (Orchestrator) Changes
New inputs:
- SPY Metrics: For market regime detection
- OpenPositions: For portfolio risk management
- Correlation calc: 30-day correlation to SPY

New features:
- Market regime detection: 5 states from trending to bearish
- Position scaling rules: Add to winners at +5%, trim losers at -50% stop
- Earnings filter: Skip trades within 5 days of earnings
- Daily position limits: Based on market regime

5. 

// Risk Management
MAX_RISK_PER_POSITION = 0.01 (1%)
TARGET_RISK_PER_POSITION = 0.005 (0.5%)
MAX_PORTFOLIO_RISK = 0.06 (6%)
MAX_POSITIONS = 12

// Stop Loss
ATR_MULTIPLIER = 3.0 to 5.0 (based on volatility)
SUPPORT_BUFFER = 0.01 to 0.02 (based on volatility)
REGIME_ADJUSTMENT = 1.0 to 1.5 (based on market)

// Profit Target  
MIN_RR_RATIO = 1.5
TARGET_RR_RATIO = 2.0
MAX_RR_RATIO = 3.0

// Position Management
SCALE_UP_TRIGGER = +5% and resistance break
SCALE_DOWN_TRIGGER = -50% of stop distance
MAX_POSITION_RISK_SCALED = 0.012 (1.2%)

// Trailing Stop
ACTIVATION = 1.0R profit (adjusted by momentum)
TIGHTNESS = 0.7 to 1.0x initial (based on velocity)

6. Decision Flow

Check market regime → Set position limits
Calculate correlation → Adjust size if correlated
Check earnings → Skip if within 5 days
Calculate stop → Daily ATR * multiplier * adjustments
Size position → Based on stop distance and risk target
Set profit target → Dynamic R:R based on quality
Monitor for scaling → Add/trim based on price action
Activate trailing → At 1R with dynamic tightness

