#pragma once

#include <MidiDevice.h>

#include <memory>
#include <vector>

#include <readerwriterqueue.h>

#include <atomic>


class MMMidiDevice : public EraeApi::MidiDevice {

public:
    MMMidiDevice(unsigned inQueueSize = EraeApi::MidiDevice::MAX_QUEUE_SIZE, unsigned outQueueSize =EraeApi::MidiDevice::MAX_QUEUE_SIZE);
    virtual ~MMMidiDevice();
    bool init(const char *indevice, const char *outdevice, bool virtualOutput = false) override { return false; }
    void deinit() override {;}

protected:

    bool isOutputOpen() override { return false;}
    bool send(const EraeApi::MidiMsg &msg) override {return false;}

    // std::unique_ptr<RtMidiIn> midiInDevice_;
    // std::unique_ptr<RtMidiOut> midiOutDevice_;
};

