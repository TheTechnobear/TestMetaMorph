#include "MMMidiDevice.h"

// Design notes: aka what needs to change...
// this is overly complex, as the erae api is kind of enforcing a queuing model
// this is so that the Erae protocol layer has a simplified midi model to work with
// perhaps a better way to do this, would be for it to be a producer/consumer of midi streams
// and NOT tak responsibility for the device.
// this is kind of clashing with vcv
// other issues are :
// memory management, using new/delete in MidiMsg
// having 2 representations of midi messages, one is fixed size (vcv), the other variable (erae)


#include <cstdio>
#include <string>

static std::string logData(const unsigned char* data, unsigned sz) {
    std::string result;
    result.reserve(sz * 5 + 4);  // rough estimate
    result += "[ ";
    char buf[8];
    for (unsigned i = 0; i < sz; ++i) {
        if (i > 0) {
            if ((i + 1) % 3 == 0) {
                result += '\n';
            } else {
                result += ' ';
            }
        }
        std::snprintf(buf, sizeof(buf), "0x%02X", data[i]);
        result += buf;
    }

    result += " ]";
    return result;
}


MMMidiDevice::MMMidiDevice(unsigned inQueueSizeE, unsigned outQueueSize) {
    // midiOuput_.setDriverId(midiInput.getDefaultDriverId());
    midiOutput_.setDeviceId(midiOutput_.getDefaultDeviceId());
    // midiOuput_.setChannel(-1);
    active_ = true;
}

MMMidiDevice::~MMMidiDevice() {
}

bool MMMidiDevice::init(const char* indevice, const char* outdevice, bool virtualOutput) {
    sysExIn_.reserve(32);  // finger is 30 including F0/F7
    sysExActive_ = false;
    return true;
}
void MMMidiDevice::deinit() {
    ;
}
void MMMidiDevice::onMessage(rack::midi::Message midimsg) {
    static const size_t MAX_DEBUG_LEN = 64;
    unsigned cin = midimsg.getUsbCIN();
    switch (cin) {
        case 0: {
            bool isRealTime = midimsg.bytes[0] >= 0xF8;
            if (!isRealTime && !sysExActive_) {
                EraeApi::MidiMsg msgRecd = EraeApi::MidiMsg::create(midimsg.bytes[0], midimsg.bytes[1], midimsg.bytes[2]);
                onMessage(msgRecd);
                sysExActive_ = false;
                sysExIn_.clear();
            }
            // Real-time messages (0xF8-0xFF) or any message during active sysex: ignore/drop
            break;
        }
        case 4: {
            bool startSysEx = midimsg.bytes[0] == 0xF0;
            if (startSysEx) {
                debugState_ = true;
                debugString_ += "S4";
                sysExActive_ = true;
                sysExIn_.clear();
                sysExIn_.push_back(midimsg.bytes[1]);
                sysExIn_.push_back(midimsg.bytes[2]);
            } else if (sysExActive_) {
                debugString_ += ".";
                sysExIn_.push_back(midimsg.bytes[0]);
                sysExIn_.push_back(midimsg.bytes[1]);
                sysExIn_.push_back(midimsg.bytes[2]);
            } else {
                debugString_ += "?";
                EraeApi::MidiMsg msgRecd =
                    EraeApi::MidiMsg::create(midimsg.bytes[0], midimsg.bytes[1], midimsg.bytes[2]);
                onMessage(msgRecd);
            }
            break;
        }
        case 5: {
            bool endSysEx = midimsg.bytes[0] == 0xF7;
            if (endSysEx && sysExActive_) {
                debugString_ += "E5";
                debugString_ += std::to_string(sysExIn_.size());
                EraeApi::MidiMsg msgRecd;
                buildSysExMsg(msgRecd);
                onMessage(msgRecd);
                debugString_ += "X";
                sysExActive_ = false;
                sysExIn_.clear();
            } else if (sysExActive_) {
                debugString_ += "c5";
                sysExIn_.push_back(midimsg.bytes[0]);
            } else {
                EraeApi::MidiMsg msgRecd = EraeApi::MidiMsg::create(midimsg.bytes[0]);
                onMessage(msgRecd);
            }
            break;
        }
        case 6: {
            bool startSysEx = midimsg.bytes[0] == 0xF0;
            bool endSysEx = midimsg.bytes[1] == 0xF7;
            if (startSysEx && endSysEx) {
                debugString_ += "SE6X";
                EraeApi::MidiMsg msgRecd;
                buildSysExMsg(msgRecd);
                onMessage(msgRecd);
                sysExActive_ = false;
                sysExIn_.clear();
            } else if (startSysEx) {
                debugState_ = true;
                debugString_ += "S6";
                sysExActive_ = true;
                sysExIn_.clear();
                sysExIn_.push_back(midimsg.bytes[1]);
            } else if (sysExActive_ && endSysEx) {
                debugString_ += "E6";
                sysExIn_.push_back(midimsg.bytes[0]);
                debugString_ += std::to_string(sysExIn_.size());
                EraeApi::MidiMsg msgRecd;
                buildSysExMsg(msgRecd);
                onMessage(msgRecd);
                debugString_ += "X";
                sysExActive_ = false;
                sysExIn_.clear();
            } else if (sysExActive_) {
                debugString_ += "c6";
                sysExIn_.push_back(midimsg.bytes[0]);
                sysExIn_.push_back(midimsg.bytes[1]);
            } else if (endSysEx) {
                debugString_ += "e6X";
                sysExIn_.push_back(midimsg.bytes[0]);
                EraeApi::MidiMsg msgRecd;
                buildSysExMsg(msgRecd);
                onMessage(msgRecd);
                sysExActive_ = false;
                sysExIn_.clear();
            } else {
                EraeApi::MidiMsg msgRecd = EraeApi::MidiMsg::create(midimsg.bytes[0], midimsg.bytes[1]);
                onMessage(msgRecd);
            }
            break;
        }
        case 7: {
            bool startSysEx = midimsg.bytes[0] == 0xF0;
            bool endSysEx = midimsg.bytes[2] == 0xF7;
            if (startSysEx && endSysEx) {
                debugString_ += "SE7X";
                EraeApi::MidiMsg msgRecd =
                    EraeApi::MidiMsg::create(midimsg.bytes[0], midimsg.bytes[1], midimsg.bytes[2]);
                onMessage(msgRecd);
            } else if (startSysEx) {
                debugState_ = true;
                debugString_ += "S7";
                sysExActive_ = true;
                sysExIn_.clear();
                sysExIn_.push_back(midimsg.bytes[1]);
                sysExIn_.push_back(midimsg.bytes[2]);
            } else if (sysExActive_ && endSysEx) {
                debugString_ += "E7X";
                sysExIn_.push_back(midimsg.bytes[0]);
                sysExIn_.push_back(midimsg.bytes[1]);
                EraeApi::MidiMsg msgRecd;
                buildSysExMsg(msgRecd);
                onMessage(msgRecd);
                sysExActive_ = false;
                sysExIn_.clear();
            } else if (sysExActive_) {
                debugString_ += "c7";
                sysExIn_.push_back(midimsg.bytes[0]);
                sysExIn_.push_back(midimsg.bytes[1]);
                sysExIn_.push_back(midimsg.bytes[2]);
            } else {
                EraeApi::MidiMsg msgRecd =
                    EraeApi::MidiMsg::create(midimsg.bytes[0], midimsg.bytes[1], midimsg.bytes[2]);
                onMessage(msgRecd);
            }
            break;
        }
    }
    if (debugString_.size() > MAX_DEBUG_LEN) {
        debugString_ = debugString_.substr(debugString_.size() - MAX_DEBUG_LEN);
    }
}

void MMMidiDevice::onMessage(EraeApi::MidiMsg& msg) {
    if (!queueInMsg(msg)) { msg.destroy(); }
}


bool MMMidiDevice::buildSysExMsg(EraeApi::MidiMsg& msg) {
    // create sysExIn_
    unsigned size = sysExIn_.size();
    unsigned char* data = new unsigned char[size + 2];
    data[0] = 0xf0;
    for (unsigned i = 0; i < size; i++) { data[i + 1] = sysExIn_[i]; }
    data[size + 1] = 0Xf7;

    msg = EraeApi::MidiMsg(data, size + 2);
    return true;
}


// Note: we cannot use QMidiDevice as moodycamel does not support platform
bool MMMidiDevice::queueInMsg(const EraeApi::MidiMsg& msg) {
    if (inputQueue_.full()) return false;
    // debugString_ = logData(msg.data(), msg.size());
    inputQueue_.push(msg);
    return true;
}

bool MMMidiDevice::queueOutMsg(const EraeApi::MidiMsg& msg) {
    if (outputQueue_.full()) return false;
    outputQueue_.push(msg);
    return true;
}

// touch msg
// 32 data [ 0xF0 0x00 0x01 0x02 0x00 0x00 0x40 0x60 0x01 0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x52 0x52 0x30 0x02
// 0x41 0x34 0x4F 0x33 0x30 0x41 0x27 0x50 0x26 0x3F 0x24 0xF7 ]
bool MMMidiDevice::nextInMsg(EraeApi::MidiMsg& msg) {
    // called by eraeApi::process()->MidiDevice::processIn to get next input message
    // which it then will send to the callback handler, and interpret accordingly
    if (inputQueue_.empty()) return false;
    msg = inputQueue_.shift();
    if (msg.size() == 32) {
        if (msg.byte(0) == 0xF0 && msg.byte(31) == 0xF7) { debugState_ = false; }
    }
    return true;
}


bool MMMidiDevice::nextOutMsg(EraeApi::MidiMsg& msg) {
    // called by eraeApi::process()->MidiDevice::processOut to get next output message
    // which it'll then use for send...
    if (outputQueue_.empty()) return false;
    msg = outputQueue_.shift();
    return true;
}


bool MMMidiDevice::isOutputOpen() {
    return true;
}

// init sequence to erae
// [ 0xF0 0x00 0x21 0x50 0x00 0x01 0x00 0x02 0x01 0x01 0x04 0x01 0x00 0x01 0x02 0xF7]  - start stream
// [ 0xF0 0x00 0x21 0x50 0x00 0x01 0x00 0x02 0x01 0x01 0x04 0x10 0x00 0xF7]  - request zone boundary
// [ 0xF0 0x00 0x21 0x50 0x00 0x01 0x00 0x02 0x01 0x01 0x04 0x20 0x00 0xF7]  - clear 
bool MMMidiDevice::send(const EraeApi::MidiMsg& msg) {
    if (!isOutputOpen()) return false;

    // called by eraeApi::process()->MidiDevice::processOut to send a specfic message
    if (msg.size() > 0) {
        unsigned status = msg.byte(0);
        if (status == 0xF0) {
            if (msg.size() >= 2) {
                int payloadSz = msg.size() - 2;
                if (payloadSz == 0) {
                    debugString_ += ">z";
                    rack::midi::Message midimsg;
                    midimsg.sysExNoPayload();
                    midiOutput_.sendMessage(midimsg);
                } else if (payloadSz == 1) {
                    debugString_ += ">";
                    debugString_ += std::to_string(msg.byte(1) & 0xFF);
                    rack::midi::Message midimsg;
                    midimsg.sysExSingleByte(msg.byte(1));
                    midiOutput_.sendMessage(midimsg);
                } else {
                    // multibyte - filter to only allow API version request (byte 11 == 0x01)
                    unsigned msgType = (msg.size() > 11) ? msg.byte(11) : 0;
                    debugString_ += ">[4";
                    rack::midi::Message midimsg;
                    unsigned offset = 1;
                    midimsg.startSysEx(msg.byte(offset), msg.byte(offset + 1));
                    midiOutput_.sendMessage(midimsg);
                    offset += 2;
                    // we need 3 bytes, and the sysex end
                    while (offset + 3 < msg.size() - 1) {
                        debugString_ += ",";
                        midimsg.continueSysEx(msg.byte(offset), msg.byte(offset + 1), msg.byte(offset + 2));
                        midiOutput_.sendMessage(midimsg);
                        offset += 3;
                    }

                    payloadSz = msg.size() - offset - 1;  // F7
                    if (payloadSz == 0) {
                        debugString_ += "E5";
                        midimsg.endSysEx();
                        midiOutput_.sendMessage(midimsg);
                    } else if (payloadSz == 1) {
                        debugString_ += "E6";
                        midimsg.endSysEx(msg.byte(offset));
                        midiOutput_.sendMessage(midimsg);
                    } else if (payloadSz == 2) {
                        debugString_ += "E7";
                        midimsg.endSysEx(msg.byte(offset), msg.byte(offset + 1));
                        midiOutput_.sendMessage(midimsg);
                    }
                    // show size and key bytes
                    debugString_ += ">";
                    debugString_ += std::to_string(msg.size());
                    debugString_ += ".";
                    debugString_ += std::to_string(msg.byte(1) & 0xFF);
                }
            }
            // malformed, need start and end...
        } else {
            rack::midi::Message midimsg;
            for (unsigned i = 0; i < msg.size(); i++) { midimsg.bytes[i] = msg.byte(i); }
            midiOutput_.sendMessage(midimsg);
        }
    }
    return true;
}

// EraeApi::MidiMsg can be 'anysize' i.e. a long sysex is one message, so will need to be broken down for MM
// and in 'reverse' the sysex will need to be built into one message


// from MM midi.hpp
// midi::Output midiOutput;
// midi::Message msg;
//
// Send 2 bytes of sysex payload:
// msg.startSysEx(0x01, 0x02);
// midiOutput.onMessage(msg); // sends [04] 0xF0 0x01 0x02
// msg.endSysEx(0x03);
// midiOutput.onMessage(msg); // sends [06] 0x03 0xF7 0x00
//
// Send 8 bytes of sysex payload over USB cable 9
// msg.setUsbCable(9);
// msg.startSysEx(0x04, 0x05);
// midiOutput.onMessage(msg); // sends [94] 0xF0 0x04 0x05
// msg.continueSysEx(0x06, 0x07, 0x08);
// midiOutput.onMessage(msg); // sends [94] 0x06 0x07 0x08
// msg.continueSysEx(0x09, 0x0a, 0x0b);
// midiOutput.onMessage(msg); // sends [94] 0x09 0x0a 0x0b
// msg.endSysEx();
// midiOutput.onMessage(msg); // sends [95] 0xF7 0x00 0x00
