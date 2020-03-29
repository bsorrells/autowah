/*
autowah for the bela platform
author: Brice Sorrells
*/

#include <Bela.h>
#include <algorithm>
#include <math.h>
#include <libraries/ADSR/ADSR.h>
#include <Biquad.h>
#include <libraries/Midi/Midi.h>

int gAudioInChannelNum; // number of audio in channels to iterate over
int gAudioOutChannelNum; // number of audio in channels to iterate over
float gSampleRate; // autio sample rate

/*
filt:
    Sets the type of filter to
sensitivity: 
    Sets the filter trigger level, tune this carefully to fit your guitar/bass output and your playing touch. You can
    further change the sensitivity from your guitar volume knob while playing.
bias:
    Controls the filter resonance frequency. When Sensitivity is turned fully off
    the Bias can be used as a sweepable filter.
resonance:
    Controls the sharpness or Q-factor of the filter.
wah_decay:
    Controls how fast the filter frequency falls back to resting point (that is set with the Bias control).
    This can be set fast (CW) so you get the wah effect on every note or slow for a more traditional auto wah sound.
*/
Biquad::filter_type filt = Biquad::bandpass; // 0 = lowpass, 1 = highpass, 2 = bandpass
float sensitivity = 0.0001;
float bias = 300;
float resonance = 0.707;
float wah_decay = 0.05;
float volume = 0.5;

// internal variables
ADSR wah_amount;
Biquad bq, target_bq;

float mono_input = 0.0;
float mono_output = 0.0;
float last_envelope = -0.0f;
float envelope = 0.0;

// wah constants
float wah_attack = 0.05;
float wah_release = 0.05;

float min_bias = 300;
float max_bias = 1400;

float min_resonance = 0.707;
float max_resonance = 5.0;

float min_decay = 0.0;
float max_decay = 2.0;

float min_freq = 300;
float max_freq = 1400;

/*
 * This callback is called every time a new input Midi message is available
 *
 * Note that this is called in a different thread than the audio processing one.
 *
 */
void midiMessageCallback(MidiChannelMessage message, void* arg){
    if(message.getType() == kmmControlChange){
        float data = message.getDataByte(1) / 127.0f;
        switch (message.getDataByte(0)){
        case 20 :
            filt = (Biquad::filter_type)(int)(data * 2 + 0.5); // CC2 selects a filter type between 0,1,2
            bq.setType(filt);
            break;
        case 21 :
            sensitivity = data;
            break;
        case 22 :
            bias = min_bias + data * (max_bias - min_bias);
            bq.setFc(bias);
            break;
        case 23:
            resonance = min_resonance + data * (max_resonance - min_resonance);
            bq.setQ(resonance);
            break;
        case 24:
            wah_decay = min_decay + data * (max_decay - min_decay);
            wah_amount.setDecayRate(wah_decay * gSampleRate);
            break;
        case 25:
            volume = data;
            break;
        default:
            rt_printf("received unexpected MIDI message: ");
            message.prettyPrint();
        }
        rt_printf("vol:%0.2f filter_type:%d, sensitivity: %0.3f, bias: %0.1f, resonance: %0.2f, decay: %0.2f, \n",
                  volume, filt, sensitivity, bias, resonance, wah_decay);
        rt_printf("\nBiquad Coeffs\n\n%0.8f\n%0.8f\n%0.8f\n\n%0.8f\n%0.8f\n%0.8f\n",
            target_bq.getA0(), target_bq.getA1(), target_bq.getA2(), 1.0f, target_bq.getB1(), target_bq.getB2());
    }
}

Midi midi;

const char* gMidiPort0 = "hw:1,0,0";

bool setup(BelaContext *context, void *userData)
{
    // setup midi
    midi.readFrom(gMidiPort0);
    midi.writeTo(gMidiPort0);
    midi.enableParser(true);
    midi.setParserCallback(midiMessageCallback, (void*) gMidiPort0);

    // initialize wah filter and amount
    gSampleRate = context->audioSampleRate;
    bq.setup(min_freq, gSampleRate, filt, resonance, 0.0f);
    target_bq.setup(min_freq, gSampleRate, filt, resonance, 0.0f);
    wah_amount.setAttackRate(wah_attack * gSampleRate);
    wah_amount.setReleaseRate(wah_release * gSampleRate);
    wah_amount.setSustainLevel(0.0f);
    
    return true;
}
/*
const double bq_coeff_step_size = 0.01;
void smoothly_modify_biquad(Biquad& current_bq, Biquad& next_bq) {
    bool updated = false;

    // A0
    if(abs(current_bq.getA0() - next_bq.getA0()) > bq_coeff_step_size)  {
        updated = true;
        if(current_bq.getA0() > next_bq.getA0())
            current_bq.setA0( current_bq.getA0() - bq_coeff_step_size);
        else
            current_bq.setA0( current_bq.getA0() + bq_coeff_step_size);
    } else
        current_bq.setA0( next_bq.getA0() );

    // A1
    if(abs(current_bq.getA1() - next_bq.getA1()) > bq_coeff_step_size)  {
        updated = true;
        if(current_bq.getA1() > next_bq.getA1())
            current_bq.setA1( current_bq.getA1() - bq_coeff_step_size);
        else
            current_bq.setA1( current_bq.getA1() + bq_coeff_step_size);
    } else
        current_bq.setA1( next_bq.getA1() );

    // A2
    if(abs(current_bq.getA2() - next_bq.getA2()) > bq_coeff_step_size)  {
        updated = true;
        if(current_bq.getA2() > next_bq.getA2())
            current_bq.setA2( current_bq.getA2() - bq_coeff_step_size);
        else
            current_bq.setA2( current_bq.getA2() + bq_coeff_step_size);
    } else
        current_bq.setA2( next_bq.getA2() );

    // B1
    if(abs(current_bq.getB1() - next_bq.getB1()) > bq_coeff_step_size)  {
        updated = true;
        if(current_bq.getB1() > next_bq.getB1())
            current_bq.setB1( current_bq.getB1() - bq_coeff_step_size);
        else
            current_bq.setB1( current_bq.getB1() + bq_coeff_step_size);
    } else
        current_bq.setB1( next_bq.getB1() );

    // B2
    if(abs(current_bq.getB2() - next_bq.getB2()) > bq_coeff_step_size)  {
        updated = true;
        if(current_bq.getB2() > next_bq.getB2())
            current_bq.setB2( current_bq.getB2() - bq_coeff_step_size);
        else
            current_bq.setB2( current_bq.getB2() + bq_coeff_step_size);
    } else
        current_bq.setB2( next_bq.getB2() );

    // if(updated)
    //     rt_printf("\nBiquad Coeffs\n\n%0.8f\n%0.8f\n%0.8f\n\n%0.8f\n%0.8f\n%0.8f\n",
    //         current_bq.getA0(), current_bq.getA1(), current_bq.getA2(), 1.0f, current_bq.getB1(), current_bq.getB2());
}
*/
void render(BelaContext *context, void *userData)
{
    double new_fc = 0.0;
    for(unsigned int n = 0; n < context->audioFrames; n++) {
        mono_input = 0.0;
        for(unsigned int ch = 0; ch < context->audioInChannels; ch++){
            mono_input += audioRead(context, n, ch);
        }
        envelope = sqrt(mono_input * mono_input);

        if(last_envelope < sensitivity && envelope >= sensitivity) {
            rt_printf("\nGate HIGH\n");
            wah_amount.gate(1);
        } else if(last_envelope >= sensitivity && envelope < sensitivity) {
            rt_printf("\nGate LOW\n");
            wah_amount.gate(0);
        } 
        last_envelope = envelope;

        if(sensitivity > 0.0f){
            new_fc = bias + wah_amount.process() * (max_bias - bias);
            if(std::abs(bq.getFc()*gSampleRate - new_fc) > 0.001)
                rt_printf("\nCurrent Fc: %0.5f New Fc: %0.5f\n", bq.getFc()*gSampleRate, new_fc);
            // target_bq.setFc(new_fc);
            bq.setFc(new_fc);
        }
        // smoothly_modify_biquad(bq, target_bq);

        mono_output = bq.process(mono_input) * volume;
        // mono_output = mono_input * volume;

        for (unsigned int ch = 0; ch < context->audioOutChannels; ch++)
        {
            audioWrite(context, n, ch, mono_output);
        }
    }
}

void cleanup(BelaContext *context, void *userData)
{

}
