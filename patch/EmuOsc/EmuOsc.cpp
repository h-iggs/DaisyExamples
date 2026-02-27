#include "daisy_patch.h"
#include "daisysp.h"
#include "sid.h"
#include <cmath>
#include <string>

using namespace daisy;

namespace
{
constexpr double kSidClockHz     = 985248.0;
constexpr float  kSidOutputScale = 1.0f / 32768.0f;

MidiUsbHandler midi;
DaisyPatch     patch;
reSID::SID     sid;

std::string waveNames[4];

int     waveform               = 0;
int     active_note            = -1;
double  sid_cycles_per_sample  = 0.0;
double  sid_cycle_accumulator  = 0.0;
uint8_t sid_control            = 0x20;

void UpdateControls();
void UpdateOled();

uint8_t CurrentWaveControl()
{
    switch(waveform)
    {
        case 0: return 0x10; // Triangle
        case 1: return 0x20; // Saw
        case 2: return 0x40; // Pulse
        default: return 0x80; // Noise
    }
}

void WriteVoiceControl(bool gate)
{
    sid_control = CurrentWaveControl();
    sid.write(0x04, sid_control | (gate ? 0x01 : 0x00));
}

void WriteVoiceFrequency(float hz)
{
    uint32_t sid_freq = static_cast<uint32_t>(
        std::lround((hz * 16777216.0f) / static_cast<float>(kSidClockHz)));
    if(sid_freq > 0xffff)
    {
        sid_freq = 0xffff;
    }

    sid.write(0x00, sid_freq & 0xff);
    sid.write(0x01, (sid_freq >> 8) & 0xff);
}

void WritePitchCv(uint8_t note)
{
    float cv_dac = (static_cast<float>(note) / 12.0f) * 819.2f;
    if(cv_dac < 0.0f)
    {
        cv_dac = 0.0f;
    }
    else if(cv_dac > 4095.0f)
    {
        cv_dac = 4095.0f;
    }

    patch.seed.dac.WriteValue(DacHandle::Channel::ONE,
                              static_cast<uint16_t>(cv_dac));
}

void HandleNoteOn(uint8_t note)
{
    active_note = note;
    WriteVoiceFrequency(daisysp::mtof(note));
    WriteVoiceControl(false);
    WriteVoiceControl(true);
    WritePitchCv(note);
}

void HandleNoteOff(uint8_t note)
{
    if(active_note == note)
    {
        WriteVoiceControl(false);
        active_note = -1;
    }
}

void SetupSid(float sample_rate)
{
    sid.set_chip_model(reSID::MOS8580);
    sid.set_sampling_parameters(kSidClockHz, reSID::SAMPLE_FAST, sample_rate);
    sid.enable_filter(false);
    sid.enable_external_filter(true);
    sid.reset();

    sid_cycles_per_sample = kSidClockHz / sample_rate;
    sid_cycle_accumulator = 0.0;

    sid.write(0x02, 0x00);
    sid.write(0x03, 0x08);
    sid.write(0x05, 0x24);
    sid.write(0x06, 0xc5);
    sid.write(0x18, 0x0f);
    WriteVoiceControl(false);
}

void SetupWaveNames()
{
    waveNames[0] = "triangle";
    waveNames[1] = "saw";
    waveNames[2] = "pulse";
    waveNames[3] = "noise";
}

static void AudioCallback(AudioHandle::InputBuffer  in,
                          AudioHandle::OutputBuffer out,
                          size_t                    size)
{
    UpdateControls();

    for(size_t i = 0; i < size; i++)
    {
        sid_cycle_accumulator += sid_cycles_per_sample;
        int sid_cycles = static_cast<int>(sid_cycle_accumulator);
        sid_cycle_accumulator -= sid_cycles;

        sid.clock(sid_cycles);

        float sid_sig = static_cast<float>(sid.output()) * kSidOutputScale * 0.6f;

        out[0][i] = (sid_sig + in[0][i]) * 0.5f;
        out[1][i] = (sid_sig + in[1][i]) * 0.5f;
        out[2][i] = sid_sig;
        out[3][i] = sid_sig;
    }
}

} // namespace

int main(void)
{
    patch.Init();

    SetupWaveNames();
    SetupSid(patch.AudioSampleRate());

    patch.StartAdc();
    patch.StartAudio(AudioCallback);

    MidiUsbHandler::Config midi_cfg;
    midi_cfg.transport_config.periph = MidiUsbTransport::Config::INTERNAL;
    midi.Init(midi_cfg);

    while(1)
    {
        UpdateOled();
        midi.Listen();

        while(midi.HasEvents())
        {
            auto msg = midi.PopEvent();
            switch(msg.type)
            {
                case NoteOn:
                {
                    auto note_msg = msg.AsNoteOn();
                    if(note_msg.velocity == 0)
                    {
                        HandleNoteOff(note_msg.note);
                    }
                    else
                    {
                        HandleNoteOn(note_msg.note);
                    }
                    break;
                }
                case NoteOff:
                {
                    auto note_msg = msg.AsNoteOff();
                    HandleNoteOff(note_msg.note);
                    break;
                }
                default: break;
            }
        }
    }
}

namespace
{

void UpdateOled()
{
    patch.display.Fill(false);

    patch.display.SetCursor(0, 0);
    std::string title = "SID MIDI";
    patch.display.WriteString(&title[0], Font_7x10, true);

    patch.display.SetCursor(0, 18);
    std::string wave = "wave:";
    patch.display.WriteString(&wave[0], Font_7x10, true);

    patch.display.SetCursor(42, 18);
    patch.display.WriteString(&waveNames[waveform][0], Font_7x10, true);

    patch.display.SetCursor(0, 36);
    std::string note = active_note >= 0 ? "note:on" : "note:off";
    patch.display.WriteString(&note[0], Font_7x10, true);

    patch.display.Update();
}

void UpdateControls()
{
    patch.ProcessDigitalControls();
    patch.ProcessAnalogControls();

    float ctrl[4];
    for(int i = 0; i < 4; i++)
    {
        ctrl[i] = patch.GetKnobValue(static_cast<DaisyPatch::Ctrl>(i));
    }

    waveform += patch.encoder.Increment();
    waveform = (waveform % 4 + 4) % 4;

    uint16_t pulse_width = 0x008 + static_cast<uint16_t>(ctrl[0] * 0x0ff0);
    if(pulse_width > 0x0fff)
    {
        pulse_width = 0x0fff;
    }

    uint8_t attack  = static_cast<uint8_t>(ctrl[1] * 15.99f);
    uint8_t release = static_cast<uint8_t>(ctrl[2] * 15.99f);
    uint8_t volume  = static_cast<uint8_t>(ctrl[3] * 15.99f);

    sid.write(0x02, pulse_width & 0xff);
    sid.write(0x03, (pulse_width >> 8) & 0x0f);
    sid.write(0x05, (attack << 4) | 0x04);
    sid.write(0x06, 0xc0 | release);
    sid.write(0x18, volume & 0x0f);

    WriteVoiceControl(active_note >= 0);
}

} // namespace
