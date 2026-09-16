#pragma once
extern bool buzzerEnabled;
extern "C" void lathe_audio_tone(unsigned hz,unsigned ms);
class Buzzer {
public:
    static Buzzer &getInstance() { static Buzzer buzzer; return buzzer; }
    void beepSuccess() { if(buzzerEnabled) lathe_audio_tone(1200,70); }
    void beginContinuousBeep(unsigned hz) { if(buzzerEnabled) lathe_audio_tone(hz,0); }
    void endContinuousBeep() { lathe_audio_tone(0,0); }
};
