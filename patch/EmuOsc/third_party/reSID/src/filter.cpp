#include "filter.h"

namespace reSID
{

namespace
{
short Clamp16(int value)
{
    if(value < -32768)
    {
        return -32768;
    }
    if(value > 32767)
    {
        return 32767;
    }
    return static_cast<short>(value);
}
} // namespace

Filter::Filter()
{
    reset();
}

void Filter::set_chip_model(chip_model model)
{
    sid_model = model;
}

void Filter::enable_filter(bool enable)
{
    filter_enabled = enable;
}

void Filter::adjust_filter_bias(double)
{
}

void Filter::set_voice_mask(reg4 mask)
{
    voice_mask = mask;
}

void Filter::reset()
{
    sid_model       = MOS8580;
    filter_enabled  = false;
    fc              = 0;
    res             = 0;
    filt            = 0;
    mode            = 0;
    vol             = 0x0f;
    voice_mask      = 0x0f;
    ext_input       = 0;
    current_output  = 0;
}

void Filter::input(short sample)
{
    ext_input = sample;
}

void Filter::writeFC_LO(reg8 value)
{
    fc = (fc & 0x07f8) | (value & 0x07);
}

void Filter::writeFC_HI(reg8 value)
{
    fc = ((value & 0xff) << 3) | (fc & 0x07);
}

void Filter::writeRES_FILT(reg8 value)
{
    res  = (value >> 4) & 0x0f;
    filt = value & 0x0f;
}

void Filter::writeMODE_VOL(reg8 value)
{
    mode = value & 0xf0;
    vol  = value & 0x0f;
}

void Filter::clock(int voice1, int voice2, int voice3)
{
    UpdateOutput(voice1, voice2, voice3);
}

void Filter::clock(cycle_count, int voice1, int voice2, int voice3)
{
    UpdateOutput(voice1, voice2, voice3);
}

short Filter::output()
{
    return current_output;
}

void Filter::UpdateOutput(int voice1, int voice2, int voice3)
{
    int mix = 0;

    if(voice_mask & 0x01)
    {
        mix += voice1;
    }
    if(voice_mask & 0x02)
    {
        mix += voice2;
    }
    if(voice_mask & 0x04)
    {
        mix += voice3;
    }
    if(voice_mask & 0x08)
    {
        mix += static_cast<int>(ext_input) << 6;
    }

    // Full analog filter emulation is too large for a stock Daisy Patch build.
    // This shim keeps the register interface intact and returns a scaled mix.
    if(filter_enabled)
    {
        mix = (mix * (8 + res)) / 16;
    }

    mix = (mix * vol) / 15;
    current_output = Clamp16(mix >> 4);
}

} // namespace reSID
