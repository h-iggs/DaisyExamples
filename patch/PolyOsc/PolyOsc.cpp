#include "daisysp.h"
#include "daisy_patch.h"
#include <string>

using namespace daisy;
using namespace daisysp;

MidiUsbHandler midi;

DaisyPatch patch;

Oscillator osc[3];

std::string waveNames[5];

int waveform;
int final_wave;

float testval;

void UpdateControls();

static void AudioCallback(AudioHandle::InputBuffer  in,
                          AudioHandle::OutputBuffer out,
                          size_t                    size)
{
    UpdateControls();
    for(size_t i = 0; i < size; i++)
    {
        float mix = 0.0f;
        //Process and output the three oscillators
        for(size_t chn = 0; chn < 3; chn++)
        {
            float sig = osc[chn].Process();
            out[chn][i] = sig;
        }

        // Mix input channel 1 into outputs 1 and 2.
        out[0][i] = (out[0][i] + in[0][i]) * 0.5f;
        out[1][i] = (out[1][i] + in[0][i]) * 0.5f;

        // Output a summed monitor mix on channel 4.
        mix = (out[0][i] + out[1][i]) * 0.5f;
        out[3][i] = mix;
    }
}

void SetupOsc(float samplerate)
{
    for(int i = 0; i < 3; i++)
    {
        osc[i].Init(samplerate);
        osc[i].SetAmp(.7);
    }
}

void SetupWaveNames()
{
    waveNames[0] = "sine";
    waveNames[1] = "triangle";
    waveNames[2] = "saw";
    waveNames[3] = "ramp";
    waveNames[4] = "square";
}

void UpdateOled();

int main(void)
{
    float samplerate;
    patch.Init(); // Initialize hardware (daisy seed, and patch)
    samplerate = patch.AudioSampleRate();

    waveform   = 0;
    final_wave = Oscillator::WAVE_POLYBLEP_TRI;

    SetupOsc(samplerate);
    SetupWaveNames();

    testval = 0.f;

    patch.StartAdc();
    patch.StartAudio(AudioCallback);

    MidiUsbHandler::Config midi_cfg;
    midi_cfg.transport_config.periph = MidiUsbTransport::Config::INTERNAL;
    midi.Init(midi_cfg);

    while(1)
    {
        UpdateOled();
        /** Listen to MIDI for new changes */
        midi.Listen();

        /** When there are messages waiting in the queue... */
        while(midi.HasEvents())
        {
            /** Pull the oldest one from the list... */
            auto msg = midi.PopEvent();
            switch(msg.type)
            {
                case NoteOn:
                {
                    /** and change the frequency of the oscillator */
                    auto note_msg = msg.AsNoteOn();
                    osc[0].SetFreq(mtof(note_msg.note));

                    // 1V/oct CV on Patch DAC CH1 (0-5V => 0-4095 counts)
                    float cv_dac = (static_cast<float>(note_msg.note) / 12.0f) * 819.2f;
                    if(cv_dac < 0.0f)
                        cv_dac = 0.0f;
                    else if(cv_dac > 4095.0f)
                        cv_dac = 4095.0f;
                    patch.seed.dac.WriteValue(DacHandle::Channel::ONE,
                                              static_cast<uint16_t>(cv_dac));
                }
                break;
                    // Since we only care about note-on messages in this example
                    // we'll ignore all other message types
                default: break;
            }
        }
    }
}

void UpdateOled()
{
    patch.display.Fill(false);

    patch.display.SetCursor(0, 0);
    std::string str  = "PolyOsc!";
    char*       cstr = &str[0];
    patch.display.WriteString(cstr, Font_7x10, true);

    str = "waveform:";
    patch.display.SetCursor(0, 30);
    patch.display.WriteString(cstr, Font_7x10, true);

    patch.display.SetCursor(70, 30);
    cstr = &waveNames[waveform][0];
    patch.display.WriteString(cstr, Font_7x10, true);

    patch.display.Update();
}

void UpdateControls()
{
    patch.ProcessDigitalControls();
    patch.ProcessAnalogControls();

    //knobs
    float ctrl[4];
    for(int i = 0; i < 4; i++)
    {
        ctrl[i] = patch.GetKnobValue((DaisyPatch::Ctrl)i);
    }

    for(int i = 0; i < 3; i++)
    {
        ctrl[i] += ctrl[3];
        ctrl[i] = ctrl[i] * 5.f;           //voltage
        ctrl[i] = powf(2.f, ctrl[i]) * 55; //Hz
    }

    testval = patch.GetKnobValue((DaisyPatch::Ctrl)2) * 5.f;

    //encoder
    waveform += patch.encoder.Increment();
    waveform = (waveform % final_wave + final_wave) % final_wave;

    //Adjust oscillators based on inputs
    for(int i = 0; i < 3; i++)
    {
        if(i != 0)
        {
            osc[i].SetFreq(ctrl[i]);
        }
        osc[i].SetWaveform((uint8_t)waveform);
    }
}
