#include "daisy_field.h"
#include "daisysp.h"
#include <string>
#include "ym3438.h"

#define YM_MASTER_CLOCK 7670454UL

using namespace daisy;
using namespace daisysp;

DaisyField hw;
MidiUsbHandler usb_midi;
ym3438_t ym;           // declared globally or static

static float samplerate_global = 0.0f;

static void midi_note_to_fnum_block(int note, float sample_rate,
                                    uint16_t &fnum, int &block)
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

class Voice
{
  public:
    Voice() {}
    ~Voice() {}
    void Init(float samplerate)
    {
        active_ = false;
        osc_.Init(samplerate);
        osc_.SetAmp(0.75f);
        osc_.SetWaveform(Oscillator::WAVE_POLYBLEP_SAW);
        env_.Init(samplerate);
        env_.SetSustainLevel(0.5f);
        env_.SetTime(ADSR_SEG_ATTACK, 0.005f);
        env_.SetTime(ADSR_SEG_DECAY, 0.005f);
        env_.SetTime(ADSR_SEG_RELEASE, 0.2f);
        filt_.Init(samplerate);
        filt_.SetFreq(6000.f);
        filt_.SetRes(0.6f);
        filt_.SetDrive(0.8f);

        // OPN2_Reset(&ym);  // line removed as per instructions
    }

    float Process()
    {
        if(active_)
        {
            float sig, amp;
            amp = env_.Process(env_gate_);
            if(!env_.IsRunning())
                active_ = false;
            sig = osc_.Process();
            filt_.Process(sig);
            return filt_.Low() * (velocity_ / 127.f) * amp;
        }
        return 0.f;
    }

    void OnNoteOn(float note, float velocity)
    {
        note_     = note;
        velocity_ = velocity;
        osc_.SetFreq(mtof(note_));
        active_   = true;
        env_gate_ = true;
    }

    void OnNoteOff() { env_gate_ = false; }

    void SetCutoff(float val) { filt_.SetFreq(val); }

    inline bool  IsActive() const { return active_; }
    inline float GetNote() const { return note_; }

  private:
    Oscillator osc_;
    Svf        filt_;
    Adsr       env_;
    float      note_, velocity_;
    bool       active_;
    bool       env_gate_;
};

template <size_t max_voices>
class VoiceManager
{
  public:
    VoiceManager() {}
    ~VoiceManager() {}

    void Init(float samplerate)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            voices[i].Init(samplerate);
        }
    }

    float Process()
    {
        float sum;
        sum = 0.f;
        for(size_t i = 0; i < max_voices; i++)
        {
            sum += voices[i].Process();
        }
        return sum;
    }

    void OnNoteOn(float notenumber, float velocity)
    {
        Voice *v = FindFreeVoice();
        if(v == NULL)
            return;
        v->OnNoteOn(notenumber, velocity);
    }

    void OnNoteOff(float notenumber, float velocity)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            Voice *v = &voices[i];
            if(v->IsActive() && v->GetNote() == notenumber)
            {
                v->OnNoteOff();
            }
        }
    }

    void FreeAllVoices()
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            voices[i].OnNoteOff();
        }
    }

    void SetCutoff(float all_val)
    {
        for(size_t i = 0; i < max_voices; i++)
        {
            voices[i].SetCutoff(all_val);
        }
    }


  private:
    Voice  voices[max_voices];
    Voice *FindFreeVoice()
    {
        Voice *v = NULL;
        for(size_t i = 0; i < max_voices; i++)
        {
            if(!voices[i].IsActive())
            {
                v = &voices[i];
                break;
            }
        }
        return v;
    }
};

static VoiceManager<24> voice_handler;

void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                   AudioHandle::InterleavingOutputBuffer out,
                   size_t                                size)
{
    hw.ProcessDigitalControls();
    hw.ProcessAnalogControls();
    if(hw.GetSwitch(hw.SW_1)->FallingEdge())
    {
        voice_handler.FreeAllVoices();
    }
    voice_handler.SetCutoff(250.f + hw.GetKnobValue(hw.KNOB_1) * 8000.f);

    for(size_t i = 0; i < size; i += 2)
    {
        // Clock the YM3438 core at its internal rate (~master_clock/6) and decimate to audio rate
        const int clocks_per_sample = static_cast<int>((YM_MASTER_CLOCK / 6.0) / samplerate_global + 0.5);
        float acc = 0.0f;
        for(int j = 0; j < clocks_per_sample; ++j)
        {
            Bit16s buf16;
            OPN2_Clock(&ym, &buf16);
            acc += buf16;
        }
        // average and normalize from signed-9 to ±1.0
        float s = acc / (clocks_per_sample * 512.0f);
        out[i]     = s;
        out[i + 1] = s;
    }
}

// Typical Switch case for Message Type.
void HandleMidiMessage(MidiEvent m)
{
    switch(m.type)
    {
        case NoteOn:
        {
            NoteOnEvent p = m.AsNoteOn();
            // Note Off can come in as Note On w/ 0 Velocity
            if(p.velocity == 0.f)
            {
                voice_handler.OnNoteOff(p.note, p.velocity);
            }
            else
            {
                voice_handler.OnNoteOn(p.note, p.velocity);
                uint16_t fnum;
                int      block;
                float samplerate = hw.AudioSampleRate();
                midi_note_to_fnum_block(p.note, samplerate, fnum, block);

                int ch = 0; // your channel 0–5

                // write F-Number LSB (register A0–A5)
                OPN2_Write(&ym, 0, 0xA0 | ch);
                OPN2_Write(&ym, 1, fnum & 0xFF);

                // write F-Number MSB + Block (register A4–A9)
                OPN2_Write(&ym, 0, 0xA4 | ch);
                OPN2_Write(&ym, 1, ((fnum >> 8) & 0x07) | (block << 3));

                // key-on: register 28–2D, data = operator bits (usually 0xF0)
                OPN2_Write(&ym, 0, 0x28 | ch);
                OPN2_Write(&ym, 1, 0xF0);

            }
        }
        break;
        case NoteOff:
        {
            NoteOnEvent p = m.AsNoteOn();
            voice_handler.OnNoteOff(p.note, p.velocity);
        }
        break;
        default: break;
    }
}

// Main -- Init, and Midi Handling
int main(void)
{
    // Init
    hw.Init();

    // Configure core for YM2612 mode
    OPN2_SetChipType(ym3438_mode_ym2612);

    // Initialize YM2612 core and load a basic patch
    OPN2_Reset(&ym);

    // Example minimal patch: disable LFO and PCM, simple algorithm for channel 0
    // You can expand this with full operator settings from a YM2612 register dump
    OPN2_Write(&ym, 0, 0x22);  // LFO off
    OPN2_Write(&ym, 1, 0x00);
    OPN2_Write(&ym, 0, 0x27);  // PCM off
    OPN2_Write(&ym, 1, 0x00);
    OPN2_Write(&ym, 0, 0xB0);  // CH0: ALGORITHM=0, FEEDBACK=0
    OPN2_Write(&ym, 1, 0x00);

    samplerate_global = hw.AudioSampleRate();
    voice_handler.Init(samplerate_global);

    //display
    const char str[] = "USB Midi";
    char *     cstr  = (char *)str;
    hw.display.WriteString(cstr, Font_7x10, true);
    hw.display.Update();

    MidiUsbHandler::Config usb_midi_config;
    usb_midi_config.transport_config.periph = MidiUsbTransport::Config::INTERNAL;
    usb_midi.Init(usb_midi_config);
    usb_midi.StartReceive();

    // Start stuff.
    // hw.midi.StartReceive();
    hw.StartAdc();
    hw.StartAudio(AudioCallback);
    for(;;)
    {
        usb_midi.Listen();
        // Handle MIDI Events
        while(usb_midi.HasEvents())
        {
            HandleMidiMessage(usb_midi.PopEvent());
        }
    }
}
