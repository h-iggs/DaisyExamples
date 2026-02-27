// Lightweight embedded filter shim for Daisy Patch integration.
#ifndef RESID_FILTER_H
#define RESID_FILTER_H

#include "siddefs.h"

namespace reSID
{

class Filter
{
  public:
    Filter();

    void set_chip_model(chip_model model);
    void enable_filter(bool enable);
    void adjust_filter_bias(double dac_bias);
    void set_voice_mask(reg4 mask);
    void reset();

    void input(short sample);
    void writeFC_LO(reg8 value);
    void writeFC_HI(reg8 value);
    void writeRES_FILT(reg8 value);
    void writeMODE_VOL(reg8 value);

    void clock(int voice1, int voice2, int voice3);
    void clock(cycle_count delta_t, int voice1, int voice2, int voice3);
    short output();

    reg16 fc;
    reg8  res;
    reg8  filt;
    reg8  mode;
    reg8  vol;
    reg4  voice_mask;

  private:
    void UpdateOutput(int voice1, int voice2, int voice3);

    chip_model sid_model;
    bool       filter_enabled;
    short      ext_input;
    short      current_output;
};

} // namespace reSID

#endif
