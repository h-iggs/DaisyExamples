#include "daisy_field.h"
#include "daisysp.h"
#include <string>
#include <cstring>

#define MAX_PAGES 6
#define LED_VALUES 9

namespace test {
#include "printf.h"
};

using namespace daisy;
using namespace daisysp;

DaisyField  hw;
MidiHandler midi;
UsbHandle   usb_handle;

// char flash_buffer3[1024];
// char DSY_SDRAM_BSS flash_buffer2[1024];
// char DSY_QSPI_BSS flash_buffer[1024];

void AudioCallback(float *in, float *out, size_t size)
{
    hw.ProcessDigitalControls();
    hw.ProcessAnalogControls();
    if(hw.GetSwitch(hw.SW_1)->FallingEdge())
    {
    }
    for(size_t i = 0; i < size; i += 2)
    {
        out[i]     = in[i];
        out[i+1]   = in[i+1];
    }
}

// Main -- Init, and Midi Handling
int main(void)
{
    bool vegasToggle = false;
    char buf[512];
    float ledValues[9] = {0.0f,0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f, 0.875f,1.0f};
    int ledValuesIndex = 0;
    int page=0;
    uint32_t tickCount = 0;
    NoteOnEvent mNote;
    ControlChangeEvent mControlChange;
    MidiEvent mEvent;

    // Init
    hw.Init();
    midi.Init(MidiHandler::INPUT_MODE_UART1, MidiHandler::OUTPUT_MODE_UART1);
    usb_handle.Init(UsbHandle::FS_INTERNAL);
    // test::sprintf(flash_buffer, "Flash test: %d %d %d", 1, 2, 3);
    // test::sprintf(flash_buffer2, "Flash test: %d %d %d", 1, 2, 3);

    //display
    const char str[] = "IOTests";
    char *     cstr  = (char *)str;
    hw.display.Fill(true);
    hw.display.WriteString(cstr, Font_7x10, false);
    hw.display.Update();

    // Start stuff.
    midi.StartReceive();
    hw.StartAdc();
    hw.StartAudio(AudioCallback);
    tickCount = System::GetTick();

    for(;;)
    {
        midi.Listen();
        // Handle MIDI Events
        while(midi.HasEvents())
        {
            mEvent = midi.PopEvent();
            switch(mEvent.type)
            {
                case NoteOn:
                {
                    mNote = mEvent.AsNoteOn();
                }
                break;
                case NoteOff:
                {
                    mNote = mEvent.AsNoteOn();
                }
                break;

                case ControlChange:
                {
                    mControlChange = mEvent.AsControlChange();
                }
                break;

                default:
                {
                }
                break;
            }
        }

        if(hw.GetSwitch(hw.SW_1)->FallingEdge())
        {
            page--;
            if(page < 0)
            {
                page = MAX_PAGES;
            }
        }
        if(hw.GetSwitch(hw.SW_2)->FallingEdge())
        {
            page++;
            if(page > MAX_PAGES)
            {
                page = 0;
            }
        }
        // test::sprintf(buf, "%d", page);
        // hw.display.WriteString(buf, Font_16x26, true);

        switch(page)
        {
            // MIDI
            case 0:
            {
                uint8_t midiOut[3] = {0x90,0,0};
                hw.display.Fill(false);
                hw.display.SetCursor(0, 16);
                test::sprintf(buf, "Note: %d %d %d", mNote.channel, mNote.note, mNote.velocity);
                hw.display.WriteString(buf, Font_6x8, true);
                hw.display.SetCursor(0, 24);
                test::sprintf(buf, "Control: %d %d %d", mControlChange.channel, mControlChange.control_number, mControlChange.value);
                hw.display.WriteString(buf, Font_6x8, true);
                for(int i=0; i<16; i++)
                {
                    if(hw.KeyboardRisingEdge(i))
                    {
                        test::sprintf(buf, "Button: %d up\r\n", i);
                        usb_handle.TransmitInternal((uint8_t*)buf, strlen(buf));
                        midiOut[0] = 0x90;
                        midiOut[1] = i;
                        midiOut[2] = i;
                        midi.SendMessage(midiOut, 3);
                        // if(i == 0)
                        // {
                        //     asm("bkpt 255");
                        // }
                    }
                    if(hw.KeyboardFallingEdge(i))
                    {
                        test::sprintf(buf, "Button: %d down\r\n", i);
                        usb_handle.TransmitInternal((uint8_t*)buf, strlen(buf));
                        midiOut[0] = 0x80;
                        midiOut[1] = i;
                        midiOut[2] = 0;
                        midi.SendMessage(midiOut, 3);
                    }
                }

                uint32_t now = System::GetTick();
                if(now-tickCount > 200000000)
                {
                    tickCount = now;
                    // test::sprintf(buf, "Tick:\t%u\r\n", tickCount);
                    // test::sprintf(buf, "Ptr1:'%x'\r\nPtr2:'%x'\r\nPtr3:'%x'\r\nPtr4:'%x'\r\n", buf, flash_buffer, flash_buffer2, flash_buffer3);
                    // usb_handle.TransmitInternal((uint8_t*)buf, strlen(buf));
                }
            }
            break;

            case 1:
            {
                float knobValue;
                hw.display.Fill(false);
                for(int i=0; i<8; i++)
                {
                    hw.display.SetCursor(0,i*8);
                    knobValue = hw.GetKnobValue(i);
                    test::sprintf(buf, "Knob %d: %d", i, (int)(knobValue*SSD1309_WIDTH));
                    hw.display.WriteString(buf, Font_6x8, true);
                }
            }
            break;

            case 2:
            {
                float knobValue;
                int lineWidth;
                int y=0;
                hw.display.Fill(false);
                for(int i=0; i<8; i++)
                {
                    knobValue = hw.GetKnobValue(i);
                    lineWidth = (int)(knobValue*SSD1309_WIDTH);
                    for(int lineCount=0; lineCount < 7; lineCount++)
                    {
                        hw.display.DrawLine(0, y, lineWidth, y, true);
                        y++;
                    }
                    y++;
                }
            }
            break;

            case 3:
            {
                char buf2[32];
                hw.display.Fill(false);
                for(int i=0; i<8; i++)
                {
                    buf[i] = '0';
                    if(hw.KeyboardState(i+8))
                    {
                        buf[i] = '1';
                    }
                    buf2[i] = '0';
                    if(hw.KeyboardState(i))
                    {
                        buf2[i] = '1';
                    }
                }
                buf[8] = '\0';
                buf2[8] = '\0';
                hw.display.SetCursor(0,0);
                hw.display.WriteString(buf, Font_11x18, true);
                hw.display.SetCursor(0,18);
                hw.display.WriteString(buf2, Font_11x18, true);

            }
            break;

            case 4:
            {
                bool gateIn;
                float knobValue;
                hw.display.Fill(false);
                for(int i=0; i<4; i++)
                {
                    hw.display.SetCursor(0,i*8);
                    knobValue = hw.GetCvValue(i);
                    test::sprintf(buf, "CV %d: %f", i, knobValue);
                    hw.display.WriteString(buf, Font_6x8, true);
                }
                hw.display.SetCursor(0,4*8);
                gateIn = hw.gate_in.State();
                test::sprintf(buf, "Gate In: %s", gateIn?"true":"false");
                hw.display.WriteString(buf, Font_6x8, true);
            }
            break;

            case 5:
            {
                float knobValue;
                hw.display.Fill(false);
                for(int i=0; i<8; i++)
                {
                    hw.display.SetCursor(0,i*8);
                    knobValue = hw.GetKnobValue(i);
                    test::sprintf(buf, "Knob %d: %f", i, knobValue);
                    hw.display.WriteString(buf, Font_6x8, true);
                }
            }
            break;

            case 6:
            {
                // hw.VegasMode();
                uint32_t now = System::GetTick();
                if(now-tickCount > 20000000)
                {
                    hw.display.Fill(vegasToggle);
                    vegasToggle = !vegasToggle;
                    for(int i=0; i <= DaisyField::LED_KNOB_8; i++)
                    {
                        hw.led_driver.SetLed(i, ledValues[ledValuesIndex]);
                        ledValuesIndex++;
                        if(ledValuesIndex > LED_VALUES)
                        {
                            ledValuesIndex = 0;
                        }
                    }
                    hw.led_driver.SwapBuffersAndTransmit();
                    tickCount = now;
                }
            }
            break;
        }
        hw.display.Update();
    }
}
