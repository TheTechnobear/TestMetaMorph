#include "VcvMidiDevice.h"

#include <fstream>
#include <iostream>
#include <vector>

#define LOG_0(x) std::cerr << x << std::endl;
#define LOG_1(x) std::cerr << x << std::endl;


static void logData(std::ostringstream& oss, const unsigned char* data, unsigned sz) {
    oss << "[ ";
    for (size_t i = 0; i < sz; ++i) {
        if (i > 0) oss << ' ';
        oss << "0x" << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    oss << "] ";
}

static std::string logMsg(const EraeApi::MidiMsg& msg) {
    std::ostringstream oss;
    oss << "sz " << std::dec << msg.size() << " data ";
    logData(oss, msg.data(), msg.size());
    return oss.str();
}

VcvMidiDevice::VcvMidiDevice(unsigned inQueueSizeE, unsigned outQueueSize) {
    int coreMidiId = -1;          // 1 = coremidi on my system
    int eraeInputDeviceId = -1;   // 2 on mine
    int eraeOutputDeviceId = -1;  // 2 on mine
    std::vector<int> driverIds = rack::midi::getDriverIds();
    for (int id : driverIds) {
        auto* driver = rack::midi::getDriver(id);
        std::string name = driver->getName();

        // printf("Driver %d = %s\n", id, name.c_str());
        if (name == "Core MIDI") coreMidiId = id;  // 1 for me
    }
    if (coreMidiId >= 0) {
        auto* driver = rack::midi::getDriver(coreMidiId);
        std::vector<int> indevices = driver->getInputDeviceIds();
        for (int dev : indevices) {
            std::string name = driver->getInputDeviceName(dev);
            // printf("In Device %d = %s\n", dev, name.c_str());
            if (name == "Erae 2 MIDI") eraeInputDeviceId = dev;  // 2 for me
        }

        std::vector<int> outdevices = driver->getOutputDeviceIds();
        for (int dev : outdevices) {
            std::string name = driver->getOutputDeviceName(dev);
            // printf("Out Device %d = %s\n", dev, name.c_str());
            if (name == "Erae 2 MIDI") eraeOutputDeviceId = dev;  // 2 for me
        }

        // if(eraeInputDeviceId >= 0) {
        //     printf("Found Erae Input\n");
        //     midiInput.setDriverId(coreMidiId);
        //     midiInput.setDeviceId(eraeInputDeviceId);
        //     midiInput.setChannel(-1); // all channels
        // }
        if (eraeOutputDeviceId >= 0) {
            // LOG_0("VcvMidiDevice :: Found Erae Output " << eraeOutputDeviceId);
            midiOutput_.setDriverId(coreMidiId);
            midiOutput_.setDeviceId(eraeOutputDeviceId);
        }
        active_ = true;
    }
}

VcvMidiDevice::~VcvMidiDevice() {
}

bool VcvMidiDevice::init(const char* indevice, const char* outdevice, bool virtualOutput) {
    return false;
}
void VcvMidiDevice::deinit() {
    ;
}
void VcvMidiDevice::onMessage(rack::midi::Message midimsg) {
    unsigned size = midimsg.getSize();
    unsigned char* data = new unsigned char[midimsg.getSize()];
    for (unsigned i = 0; i < size; i++) { data[i] = midimsg.bytes[i]; }
    EraeApi::MidiMsg msg(data, size);
    onMessage(msg);
}

void VcvMidiDevice::onMessage(EraeApi::MidiMsg& msg) {
    if (!queueInMsg(msg)) { msg.destroy(); }
}

// Note: we cannot use QMidiDevice as moodycamel does not support platform
bool VcvMidiDevice::queueInMsg(const EraeApi::MidiMsg& msg) {
    if (inputQueue_.full()) return false;
    // LOG_0("VcvMidiDevice :: queueInMsg  sz" << msg.size());
    inputQueue_.push(msg);
    return true;
}

bool VcvMidiDevice::queueOutMsg(const EraeApi::MidiMsg& msg) {
    // LOG_0("VcvMidiDevice :: queueOutMsg  sz" << msg.size());
    if (outputQueue_.full()) return false;
    outputQueue_.push(msg);
    return true;
}

bool VcvMidiDevice::nextInMsg(EraeApi::MidiMsg& msg) {
    // called by eraeApi::process()->MidiDevice::processIn to get next input message
    // which it then will send to the callback handler, and interpret accordingly
    if (inputQueue_.empty()) return false;
    msg = inputQueue_.shift();
    // LOG_0("VcvMidiDevice :: nextInMsg" << logMsg(msg));
    return true;
}
bool VcvMidiDevice::nextOutMsg(EraeApi::MidiMsg& msg) {
    // called by eraeApi::process()->MidiDevice::processOut to get next output message
    // which it'll then use for send...
    if (outputQueue_.empty()) return false;
    msg = outputQueue_.shift();
    // LOG_0("VcvMidiDevice :: nextOutMsg  sz" << msg.size());
    return true;
}


bool VcvMidiDevice::isOutputOpen() {
    return true;
}


bool VcvMidiDevice::send(const EraeApi::MidiMsg& msg) {
    if (!isOutputOpen()) return false;

    // LOG_0("VcvMidiDevice :: send " << logMsg(msg));
    rack::midi::Message midimsg;
    midimsg.bytes.resize(msg.size());
    for (unsigned i = 0; i < msg.size(); i++) { midimsg.bytes[i] = msg.byte(i); }
    midiOutput_.sendMessage(midimsg);
    // LOG_0("VcvMidiDevice midimsg " << midimsg.toString());
    return true;
}
