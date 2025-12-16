#pragma once

#include <MidiDevice.h>

#include <memory>
#include <vector>


class MMMidiDevice : public EraeApi::MidiDevice {
public:
    static constexpr int MAX_QUEUE_SIZE = 512;

    MMMidiDevice(unsigned inQueueSize = MAX_QUEUE_SIZE, unsigned outQueueSize = MAX_QUEUE_SIZE);
    virtual ~MMMidiDevice();
    bool init(const char *indevice, const char *outdevice, bool virtualOutput = false) override { return false; }
    void deinit() override {;}

    bool queueInMsg(const EraeApi::MidiMsg &msg) override { return false;}
    bool queueOutMsg(const EraeApi::MidiMsg &msg) override { return false;}
protected:
    bool nextInMsg(EraeApi::MidiMsg &msg) override { return false;}
    bool nextOutMsg(EraeApi::MidiMsg &msg) override { return false;}

    bool isOutputOpen() override { return false;}
    bool send(const EraeApi::MidiMsg &msg) override {return false;}
};

