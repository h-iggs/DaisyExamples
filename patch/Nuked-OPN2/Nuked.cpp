#include "ym3438.h"
#include "daisy_patch.h"
#include "daisysp.h"
#include <string>

#define YM_MASTER_CLOCK 7670454UL

static void midi_note_to_fnum_block(int note, double sample_rate, uint16_t &fnum, int &block);

using namespace daisy;
using namespace daisysp;

int midiNoteOnCount = 0;
int midiRealtimeMsgCount = 0;
int midiClockMsgCount = 0;
int midiStartCount = 0;
int midiStopCount = 0;
DaisyPatch hw;
Oscillator osc;
Svf        filt;
ym3438_t ym;

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    float fm_rate      = YM_MASTER_CLOCK / 6.0f;        // ≈1.278 MHz  
    float audio_rate   = hw.AudioSampleRate();          // e.g. 48 000 Hz  
    int   clocks_per_sample = int(fm_rate / audio_rate + 0.5f);
    Bit16s raw;
    float sig;
    
    for(size_t i = 0; i < size; i++)
    {
        sig = osc.Process();
        filt.Process(sig);
        for(size_t chn = 0; chn < 4; chn++)
        {
            out[chn][i] = filt.Low();
        }

        // // how many FM ticks per audio sample?
        float acc = 0.0f;

        // clocks_per_sample == 27
        for(int i = 0; i < 8; ++i)
        {
            // OPN2_Clock(&ym, &raw);
            // acc += raw;
        }
        // // average & normalize to –1…+1
        // float s = (acc / clocks_per_sample) / 32767.0f;
        // for (size_t chn = 0; chn < 4; ++chn)
        // {
        //     out[chn][i] = s;
        // }

    }
}

// Typical Switch case for Message Type.
void HandleMidiMessage(MidiEvent m)
{
    switch(m.type)
    {
        case SystemRealTime:
        {
            midiRealtimeMsgCount++;

            switch(m.srt_type)
            {
                case daisy::TimingClock:
                {
                    midiClockMsgCount++;
                }
                break;

                case daisy::Start:
                {
                    midiStartCount++;
                }
                break;

                case daisy::Stop:
                {
                    midiStopCount++;
                }
                break;
            }
        }
        break;
        case NoteOn:
        {
            NoteOnEvent p = m.AsNoteOn();
            // This is to avoid Max/MSP Note outs for now..
            if(m.data[1] != 0)
            {
                midiNoteOnCount++;
                p = m.AsNoteOn();
                osc.SetFreq(mtof(p.note));
                osc.SetAmp((p.velocity / 127.0f));

                uint16_t fnum;
                int      block;
                midi_note_to_fnum_block(p.note, hw.AudioSampleRate(), fnum, block);

                OPN2_Write(&ym, 0, 0xA0 | 0x00 /*chan*/);
                OPN2_Write(&ym, 1, fnum & 0xFF);
                
                // set MSB of F-Number + Block (tone octave)
                OPN2_Write(&ym, 0, 0xA4 | 0x00 /*chan*/);
                OPN2_Write(&ym, 1, ((fnum >> 8) & 0x03) | (block << 2));
                
                // key on:
                OPN2_Write(&ym, 0, 0x28 | /*chan*/0);
                OPN2_Write(&ym, 1, /*op1-slot bits*/0xF0);
                    
            }
        }
        break;
        case ControlChange:
        {
            ControlChangeEvent p = m.AsControlChange();
            switch(p.control_number)
            {
                case 1:
                    // CC 1 for cutoff.
                    filt.SetFreq(mtof((float)p.value));
                    break;
                case 2:
                    // CC 2 for res.
                    filt.SetRes(((float)p.value / 127.0f));
                    break;
                default: break;
            }
            break;
        }
        default: break;
    }
}


// Main -- Init, and Midi Handling
int main(void)
{
    // Init
    float samplerate;
    hw.Init();
    samplerate = hw.AudioSampleRate();

    // Synthesis
    osc.Init(samplerate);
    osc.SetWaveform(Oscillator::WAVE_POLYBLEP_SAW);
    filt.Init(samplerate);

    //display
    std::string str  = "                     ";
    sprintf(&str[0], "NoteOn: %d", midiNoteOnCount);
    char*       cstr = &str[0];

    hw.display.Fill(false);
    hw.display.SetCursor(0, 0);
    hw.display.WriteString(cstr, Font_7x10, true);
    hw.display.Update();

    // Start stuff.

    OPN2_SetChipType(ym3438_mode_ym2612);
    OPN2_Reset(&ym);

    // Minimal YM2612 channel 0 patch: set moderate Total Level (TL) on all four operators
    for(int op = 0; op < 4; ++op)
    {
      // TL registers: 0x40 + (op*4) for operators 0–3
      uint8_t reg = 0x40 + (op << 2); 
      OPN2_Write(&ym, 0, reg | 0x00);  // channel 0
      OPN2_Write(&ym, 1, 0x20);        // moderate TL
    }

    hw.midi.StartReceive();

    hw.StartAdc();
    hw.StartAudio(AudioCallback);

    for(;;)
    {
        hw.midi.Listen();
        // Handle MIDI Events
        while(hw.midi.HasEvents())
        {
            HandleMidiMessage(hw.midi.PopEvent());
        }
        hw.display.Fill(false);

        int y = 0;
        hw.display.SetCursor(0, y+=12);
        sprintf(&str[0], "NoteOn: %d ", midiNoteOnCount);
        hw.display.WriteString(&str[0], Font_7x10, true);

        // hw.display.SetCursor(0, y+=12);
        // sprintf(&str[0], "All RT: %d ", midiRealtimeMsgCount);
        // hw.display.WriteString(&str[0], Font_7x10, true);

        hw.display.SetCursor(0, y+=12);
        sprintf(&str[0], "Clock: %d ", midiClockMsgCount);
        hw.display.WriteString(&str[0], Font_7x10, true);

        hw.display.SetCursor(0, y+=12);
        sprintf(&str[0], "Start: %d ", midiStartCount);
        hw.display.WriteString(&str[0], Font_7x10, true);

        hw.display.SetCursor(0, y+=12);
        sprintf(&str[0], "Stop: %d ", midiStopCount);
        hw.display.WriteString(&str[0], Font_7x10, true);

        hw.display.Update();
    }
}

static void midi_note_to_fnum_block(int note, double sample_rate, uint16_t &fnum, int &block)
{
    // 1) compute frequency in Hz
    double freq = 440.0 * std::pow(2.0, (note - 69) / 12.0);

    // 2) find smallest block so that fnum < 2048
    //    fnum = freq * (2^(20-block)) / sample_rate
    for (block = 0; block < 8; ++block)
    {
        double calc = freq * std::ldexp(1.0, 20 - block) / sample_rate;
        if (calc < 2048.0)
        {
            fnum = static_cast<uint16_t>(calc + 0.5);
            return;
        }
    }
    // clamp to max
    block = 7;
    fnum  = 2047;
}