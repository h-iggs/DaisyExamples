#include "daisy_field.h"
#include "daisysp.h"
#include "nuked/ym3438.h"

#define NUM_VOICES 16

using namespace daisy;

DaisyField hw;
MidiHandler midi;
size_t blockSize = 0;
int seconds = 0;
int cbCount = 0;
int oldClockCount = 0;
float bpm = 0;
extern int clockCount;
ym3438_t ymChip;
Bit16s ymAudio[2];

struct voice
{
    void Init()
    {
        osc_.Init(DSY_AUDIO_SAMPLE_RATE);
        amp_ = 0.0f;
        osc_.SetAmp(1.0f);
        osc_.SetWaveform(daisysp::Oscillator::WAVE_POLYBLEP_SAW);
        on_ = false;
    }
    float Process()
    {
        float sig;
        amp_ += 0.0025f * ((on_ ? 1.0f : 0.0f) - amp_);
        sig = osc_.Process() * amp_;
        return sig;
    }
    void set_note(float nn) { osc_.SetFreq(daisysp::mtof(nn)); }

    daisysp::Oscillator osc_;
    float               amp_, midibase_;
    bool                on_;
};

voice   v[NUM_VOICES];
uint8_t buttons[16];
// Use bottom row to set major scale
// Top row chromatic notes, and the inbetween notes are just the octave.
float scale[16]   = {0.f,
                   2.f,
                   4.f,
                   5.f,
                   7.f,
                   9.f,
                   11.f,
                   12.f,
                   1.f,
                   3.f,
                   0.f,
                   6.f,
                   8.f,
                   10.f,
                   0.0f};
float active_note = scale[0];

int8_t octaves = 0;

static daisysp::ReverbSc verb;
// Use two side buttons to change octaves.
float kvals[8];
float cvvals[4];

size_t knob_idx[] = {DaisyField::KNOB_1,
                     DaisyField::KNOB_2,
                     DaisyField::KNOB_3,
                     DaisyField::KNOB_4,
                     DaisyField::KNOB_5,
                     DaisyField::KNOB_6,
                     DaisyField::KNOB_7,
                     DaisyField::KNOB_8};

void AudioCallback(float *in, float *out, size_t size)
{
    cbCount++;
    if(cbCount>2000)
    {
        cbCount = 0;
        seconds++;
        bpm = ((float)clockCount)*1.25f;
        clockCount = 0;
    }
    blockSize = size;
    bool trig, use_verb;
    hw.ProcessAnalogControls();
    hw.UpdateDigitalControls();
    if(hw.GetSwitch(DaisyField::SW_1)->RisingEdge())
    {
        octaves -= 1;
        trig = true;
    }
    if(hw.GetSwitch(DaisyField::SW_2)->RisingEdge())
    {
        octaves += 1;
        trig = true;
    }
    use_verb = true;

    for(int i = 0; i < 8; i++)
    {
        kvals[knob_idx[i]] = hw.GetKnobValue(i);
        if(i < 4)
        {
            cvvals[i] = hw.GetCvValue(i);
        }
    }

    if(octaves < 0)
        octaves = 0;
    if(octaves > 4)
        octaves = 4;

    if(trig)
    {
        for(int i = 0; i < NUM_VOICES; i++)
        {
            v[i].set_note((12.0f * octaves) + 24.0f + scale[i]);
        }
    }
    for(size_t i = 0; i < 16; i++)
    {
        v[i].on_ = hw.KeyboardState(i);
    }
    float sig, send;
    float wetl, wetr;
    for(size_t i = 0; i < size; i += 2)
    {
        sig = 0.0f;
        for(int i = 0; i < NUM_VOICES; i++)
        {
            if(i != 10 && i != 14 && i != 15)
                sig += v[i].Process();
        }
        send = sig * 0.35f;
        verb.SetFeedback(hw.GetKnobValue(0)); //0.94f
        verb.SetLpFreq(8000.0f*(0.5f-hw.GetKnobValue(1))); //8000.0f
        verb.Process(send, send, &wetl, &wetr);
        //        wetl = wetr = sig;
        if(!use_verb)
            wetl = wetr = 0.0f;
        out[i]     = (sig + wetl) * 0.5f;
        out[i + 1] = (sig + wetr) * 0.5f;
        OPN2_Clock(&ymChip, ymAudio);
        out[i] = (float)ymAudio[0];
        out[i+1] = (float)ymAudio[1];

    }
}

void AudioInputTest(float *in, float *out, size_t size)
{
    float sendL, sendR, wetL, wetR;
    for(size_t i = 0; i < size; i++)
    {
        sendL = in[i] * 0.7f;
        sendR = in[i + 1] * 0.7f;
        verb.Process(sendL, sendR, &wetL, &wetR);
        out[i]     = in[i] * 0.8f + wetL;
        out[i + 1] = in[i + 1] * 0.8f + wetR;
    }
}


// Typical Switch case for Message Type.
void HandleMidiMessage(MidiEvent m)
{
    switch(m.type)
    {
        case NoteOn:
        {
            hw.midiNote = m.AsNoteOn();
        }
        break;
        case ControlChange:
        {
        }
        break;
        case RealTime:
        {
            hw.midiRealTime = m.AsRealTime();
        }
        break;
        default:
        {
        }
        break;
    }
}

void SetupOPN2(ym3438_t *chip)
{
    OPN2_Reset(chip);
    OPN2_SetChipType(ym3438_mode_ym2612);
    OPN2_Write(chip, 0x22, 0x00); // LFO off
    OPN2_Write(chip, 0x27, 0x00); // Note off (channel 0)
    OPN2_Write(chip, 0x28, 0x01); // Note off (channel 1)
    OPN2_Write(chip, 0x28, 0x02); // Note off (channel 2)
    OPN2_Write(chip, 0x28, 0x04); // Note off (channel 3)
    OPN2_Write(chip, 0x28, 0x05); // Note off (channel 4)
    OPN2_Write(chip, 0x28, 0x06); // Note off (channel 5)
    OPN2_Write(chip, 0x2B, 0x00); // DAC off
    OPN2_Write(chip, 0x30, 0x71); //
    OPN2_Write(chip, 0x34, 0x0D); //
    OPN2_Write(chip, 0x38, 0x33); //
    OPN2_Write(chip, 0x3C, 0x01); // DT1/MUL
    OPN2_Write(chip, 0x40, 0x23); //
    OPN2_Write(chip, 0x44, 0x2D); //
    OPN2_Write(chip, 0x48, 0x26); //
    OPN2_Write(chip, 0x4C, 0x00); // Total level
    OPN2_Write(chip, 0x50, 0x5F); //
    OPN2_Write(chip, 0x54, 0x99); //
    OPN2_Write(chip, 0x58, 0x5F); //
    OPN2_Write(chip, 0x5C, 0x94); // RS/AR 
    OPN2_Write(chip, 0x60, 0x05); //
    OPN2_Write(chip, 0x64, 0x05); //
    OPN2_Write(chip, 0x68, 0x05); //
    OPN2_Write(chip, 0x6C, 0x07); // AM/D1R
    OPN2_Write(chip, 0x70, 0x02); //
    OPN2_Write(chip, 0x74, 0x02); //
    OPN2_Write(chip, 0x78, 0x02); //
    OPN2_Write(chip, 0x7C, 0x02); // D2R
    OPN2_Write(chip, 0x80, 0x11); //
    OPN2_Write(chip, 0x84, 0x11); //
    OPN2_Write(chip, 0x88, 0x11); //
    OPN2_Write(chip, 0x8C, 0xA6); // D1L/RR
    OPN2_Write(chip, 0x90, 0x00); //
    OPN2_Write(chip, 0x94, 0x00); //
    OPN2_Write(chip, 0x98, 0x00); //
    OPN2_Write(chip, 0x9C, 0x00); // Proprietary
    OPN2_Write(chip, 0xB0, 0x32); // Feedback/algorithm
    OPN2_Write(chip, 0xB4, 0xC0); // Both speakers on
    OPN2_Write(chip, 0x28, 0x00); // Key off
    OPN2_Write(chip, 0xA4, 0x22);	// 
    OPN2_Write(chip, 0xA0, 0x69); // Set frequency
}

int main(void)
{
    OPN2_Reset(&ymChip);
    OPN2_Write(&ymChip, 0x28, 0xF1);

    hw.Init();
    // Initialize controls.
    octaves = 2;
    for(int i = 0; i < NUM_VOICES; i++)
    {
        v[i].Init();
        v[i].set_note((12.0f * octaves) + 24.0f + scale[i]);
    }

    verb.Init(hw.SampleRate());
    verb.SetFeedback(0.94f);
    verb.SetLpFreq(8000.0f);

    hw.StartAdc();
    hw.StartAudio(AudioCallback);
    midi.Init(MidiHandler::INPUT_MODE_UART1, MidiHandler::OUTPUT_MODE_NONE);
    midi.StartReceive();

    for(;;)
    {
        hw.VegasMode();
        dsy_system_delay(1);
        dsy_dac_write(DSY_DAC_CHN1, hw.GetKnobValue(0) * 4095);
        dsy_dac_write(DSY_DAC_CHN2, hw.GetKnobValue(1) * 4095);
        dsy_gpio_toggle(&hw.gate_out_);
        midi.Listen();
        // Handle MIDI Events
        while (midi.HasEvents())
        {
            HandleMidiMessage(midi.PopEvent());
        }
    }
}
