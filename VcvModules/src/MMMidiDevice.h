#pragma once

#include <MidiDevice.h>

#include <memory>
#include <rack.hpp>
#include <string>
#include <vector>

class MMMidiDevice : public EraeApi::MidiDevice {
public:
    static constexpr int MAX_QUEUE_SIZE = 512;

    MMMidiDevice(unsigned inQueueSize = MAX_QUEUE_SIZE, unsigned outQueueSize = MAX_QUEUE_SIZE);
    virtual ~MMMidiDevice();
    bool init(const char* indevice, const char* outdevice, bool virtualOutput = false) override;
    void deinit() override;

    void onMessage(rack::midi::Message msg);
    void onMessage(EraeApi::MidiMsg& msg);

protected:
    bool queueInMsg(const EraeApi::MidiMsg& msg) override;
    bool queueOutMsg(const EraeApi::MidiMsg& msg) override;
    bool nextInMsg(EraeApi::MidiMsg& msg) override;
    bool nextOutMsg(EraeApi::MidiMsg& msg) override;

    bool isOutputOpen() override;
    bool send(const EraeApi::MidiMsg& msg) override;

    bool buildSysExMsg(EraeApi::MidiMsg& msg);

    rack::midi::Output midiOutput_;
    bool sysExActive_ = false;
    std::vector<uint8_t> sysExIn_;
    rack::dsp::RingBuffer<EraeApi::MidiMsg, 100> inputQueue_;
    rack::dsp::RingBuffer<EraeApi::MidiMsg, 100> outputQueue_;
};
