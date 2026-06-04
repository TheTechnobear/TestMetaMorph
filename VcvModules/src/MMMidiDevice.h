#pragma once

#include <MidiDevice.h>

#include <rack.hpp>

#include <memory>
#include <vector>

class MMMidiDevice : public EraeApi::MidiDevice {
public:
    static constexpr int MAX_QUEUE_SIZE = 512;

    MMMidiDevice(unsigned inQueueSize = MAX_QUEUE_SIZE, unsigned outQueueSize = MAX_QUEUE_SIZE);
    virtual ~MMMidiDevice();
    bool init(const char *indevice, const char *outdevice, bool virtualOutput = false) override;
    void deinit() override;

    bool queueInMsg(const EraeApi::MidiMsg &msg) override;
    bool queueOutMsg(const EraeApi::MidiMsg &msg) override;
protected:
    bool nextInMsg(EraeApi::MidiMsg &msg) override;
    bool nextOutMsg(EraeApi::MidiMsg &msg) override;

    bool isOutputOpen();
    bool send(const EraeApi::MidiMsg &msg) override;

    // rack callback?
    // void onMessage(const rack::midi::Message &message);

    rack::midi::InputQueue midInput_;
    rack::midi::Output midiOuput_;
    bool sysExActive_ = false;
    std::vector<int> sysExIn_;
};



