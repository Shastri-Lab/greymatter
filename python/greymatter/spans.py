import enum

class CurrentSpan(enum.IntEnum):
    """LTC2662 current range codes."""
    HI_Z       = 0x0
    MA_3_125   = 0x1
    MA_6_25    = 0x2
    MA_12_5    = 0x3
    MA_25      = 0x4
    MA_50      = 0x5
    MA_100     = 0x6
    MA_200     = 0x7
    SWITCH_NEG = 0x8
    MA_300     = 0xF


class VoltageSpan(enum.IntEnum):
    """LTC2664 voltage range codes."""
    V_0_5   = 0x0
    V_0_10  = 0x1
    V_PM5   = 0x2
    V_PM10  = 0x3
    V_PM2_5 = 0x4
