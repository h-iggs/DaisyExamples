#include "daisy_field.h"
#include "daisysp.h"
// #include "dev/usb_midi.h"
// #include "usb_midi.h"

using namespace daisysp;
using namespace daisy;

#define NUM_CONTROLS 4

DaisyField hw;
ModalVoice modal;
MidiUsbHandler usb_midi;

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
                   0.f,
                   1.f,
                   3.f,
                   0.f,
                   6.f,
                   8.f,
                   10.f,
                   0.0f};
float active_note = scale[0];

int8_t octaves = 0;

// Use two side buttons to change octaves.
float kvals[NUM_CONTROLS];

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    hw.ProcessAnalogControls();
    hw.ProcessDigitalControls();

    octaves += hw.sw[0].RisingEdge() ? -1 : 0;
    octaves += hw.sw[1].RisingEdge() ? 1 : 0;
    octaves = DSY_MIN(DSY_MAX(0, octaves), 4);

    for(int i = 0; i < NUM_CONTROLS; i++)
    {
        kvals[i] = hw.GetKnobValue(i);
    }

    float brightness = kvals[0] + hw.GetCvValue(0);  // mix knob + CV
    brightness = fminf(1.0f, brightness);                // clip to valid range
    modal.SetBrightness(brightness);

    float structure = kvals[1] + hw.GetCvValue(1);  // mix knob + CV
    structure = fminf(1.0f, structure);                // clip to valid range
    modal.SetStructure(structure);

    float damping = kvals[2] + hw.GetCvValue(2);  // mix knob + CV
    damping = fminf(1.0f, damping);                // clip to valid range
    modal.SetDamping(damping);

    float accent = kvals[3] + hw.GetCvValue(3);  // mix knob + CV
    accent = fminf(1.0f, accent);                // clip to valid range
    modal.SetAccent(accent);


    // Handle USB MIDI input.
    while(usb_midi.HasEvents())
    {
        MidiEvent event = usb_midi.PopEvent();
        switch(event.type)
        {
            case NoteOn:
                if(event.data[1] != 0)
                {
                    modal.Trig();
                    float note = static_cast<float>(event.data[0]);
                    modal.SetFreq(mtof(note));
                    modal.SetAccent(static_cast<float>(event.data[1]) / 127.0f);
                }
                break;
            case NoteOff:
                // Handle note off event if needed
                break;
            case ControlChange:
                if(event.data[0] == 1)
                {
                    modal.SetBrightness(static_cast<float>(event.data[1]) / 127.0f);
                }
                else if(event.data[0] == 2)
                {
                    modal.SetStructure(static_cast<float>(event.data[1]) / 127.0f);
                }
                else if(event.data[0] == 3)
                {
                    modal.SetDamping(static_cast<float>(event.data[1]) / 127.0f);
                }
                else if(event.data[0] == 4)
                {
                    float accent = static_cast<float>(event.data[1]) / 127.0f;
                    modal.SetAccent(accent);
                }
                break;
            default:
                break;
        }
        if(event.type == NoteOn && event.data[1] != 0)
        {
            modal.Trig();
            float note = static_cast<float>(event.data[0]);
            modal.SetFreq(mtof(note));
            modal.SetAccent(static_cast<float>(event.data[1]) / 127.0f);
        }
    }

    for(size_t i = 0; i < 16; i++)
    {
        if(hw.KeyboardRisingEdge(i) && i != 8 && i != 11 && i != 15)
        {
            modal.Trig();
            float m = (12.0f * octaves) + 24.0f + scale[i];
            modal.SetFreq(mtof(m));
        }
    }

    for(size_t i = 0; i < size; i++)
    {
        out[0][i] = out[1][i] = modal.Process();
    }
}

void UpdateLeds(float *knob_vals)
{
    // knob_vals is exactly 8 members
    size_t knob_leds[] = {
        DaisyField::LED_KNOB_1,
        DaisyField::LED_KNOB_2,
        DaisyField::LED_KNOB_3,
        DaisyField::LED_KNOB_4,
        DaisyField::LED_KNOB_5,
        DaisyField::LED_KNOB_6,
        DaisyField::LED_KNOB_7,
        DaisyField::LED_KNOB_8,
    };
    size_t keyboard_leds[] = {
        DaisyField::LED_KEY_A1,
        DaisyField::LED_KEY_A2,
        DaisyField::LED_KEY_A3,
        DaisyField::LED_KEY_A4,
        DaisyField::LED_KEY_A5,
        DaisyField::LED_KEY_A6,
        DaisyField::LED_KEY_A7,
        DaisyField::LED_KEY_A8,
        DaisyField::LED_KEY_B2,
        DaisyField::LED_KEY_B3,
        DaisyField::LED_KEY_B5,
        DaisyField::LED_KEY_B6,
        DaisyField::LED_KEY_B7,
    };
    for(size_t i = 0; i < 8; i++)
    {
        float val = i < NUM_CONTROLS ? knob_vals[i] : 0.f;
        hw.led_driver.SetLed(knob_leds[i], val);
    }
    for(size_t i = 0; i < 13; i++)
    {
        hw.led_driver.SetLed(keyboard_leds[i], 1.f);
    }
    hw.led_driver.SwapBuffersAndTransmit();
}

int main(void)
{
    float sr;
    hw.Init();
    sr = hw.AudioSampleRate();

    // Initialize controls.
    octaves = 2;

    modal.Init(sr);

    // Initialize USB MIDI interface
    MidiUsbHandler::Config usb_midi_config;
    usb_midi_config.transport_config.periph = MidiUsbTransport::Config::INTERNAL;
    usb_midi.Init(usb_midi_config);
    usb_midi.StartReceive();

    hw.StartAdc();
    hw.StartAudio(AudioCallback);

    // hw.display.Clear();
    hw.display.Fill(false);
    hw.display.SetCursor(0, 0);
    hw.display.WriteString("Modal Voice", Font_7x10, true);
    hw.display.Update();
    for(;;)
    {
        UpdateLeds(kvals);
        System::Delay(6);
    }
}
