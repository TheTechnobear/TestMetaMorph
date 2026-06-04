#include "MMMidiDevice.h"

MMMidiDevice::MMMidiDevice(unsigned inQueueSizeE, unsigned outQueueSize) {
}

MMMidiDevice::~MMMidiDevice() {
}

bool MMMidiDevice::init(const char* indevice, const char* outdevice, bool virtualOutput) {
    sysExIn_.reserve(32); // finger is 30 including F0/F7
    sysExActive_ = false;
    return false;
}
void MMMidiDevice::deinit() {
    ;
}

// void MMMidiDevice::onMessage(const rack::midi::Message &message) {

// }


// Note: we cannot use QMidiDevice as moodycamel does not support platform
bool MMMidiDevice::queueInMsg(const EraeApi::MidiMsg& msg) {

    // the midi device can queue an incoming message
    rack::midi::Message midimsg;
    if (midInput_.tryPop(&midimsg, -1)) {
        switch (midimsg.getUsbCIN()) {
            case 0: {
                // not sysex
                EraeApi::MidiMsg msgRecd =
                    EraeApi::MidiMsg::create(midimsg.bytes[0], midimsg.bytes[1], midimsg.bytes[2]);
                /// queue for later!
                sysExActive_ = false;
                sysExIn_.clear();
                break;
            }
            case 4: {
                // 3 bytes valid
                bool startSysEx = midimsg.bytes[0] == 0xF0;
                if (startSysEx) {
                    sysExActive_ = true;
                    sysExIn_.clear();
                    sysExIn_.push_back(midimsg.bytes[1]);
                    sysExIn_.push_back(midimsg.bytes[2]);
                } else {
                    if (sysExActive_) {
                        // continue
                        sysExIn_.push_back(midimsg.bytes[0]);
                        sysExIn_.push_back(midimsg.bytes[1]);
                        sysExIn_.push_back(midimsg.bytes[2]);
                    } else {
                        EraeApi::MidiMsg msgRecd =
                            EraeApi::MidiMsg::create(midimsg.bytes[0], midimsg.bytes[1], midimsg.bytes[2]);
                        /// queue for later;
                    }
                }
                break;
            }
            case 5: {
                // only 1 byte valid
                bool endSysEx = midimsg.bytes[0] == 0xF7;
                if (endSysEx) {
                    // create byte arrange and send from sysExIn_
                    // EraeApi::MidiMsg msgRecd = EraeApi::MidiMsg::create();
                    /// queue for later!
                    sysExActive_ = false;
                    sysExIn_.clear();
                } else {
                    EraeApi::MidiMsg msgRecd = EraeApi::MidiMsg::create(midimsg.bytes[0]);
                    /// queue for later!
                }
                break;
            }
            case 6: {
                // 2 bytes valid , 1 byte payload
                bool startSysEx = midimsg.bytes[0] == 0xF0;
                bool endSysEx = midimsg.bytes[1] == 0xF7;
                if (!startSysEx && endSysEx) {
                    sysExIn_.push_back(midimsg.bytes[0]);
                    // create byte arrange and send from sysExIn_
                    // EraeApi::MidiMsg msgRecd = EraeApi::MidiMsg::create();
                    /// queue for later!
                    sysExActive_ = false;
                    sysExIn_.clear();
                } else {
                    if (endSysEx) {
                        sysExActive_ = false;
                        sysExIn_.clear();
                    }
                    // this is also fine, if its a zero byte sysex
                    EraeApi::MidiMsg msgRecd = EraeApi::MidiMsg::create(midimsg.bytes[0], midimsg.bytes[1]);
                    /// queue for later!
                }
                break;
            }
            case 7: {
                // 3 bytes valid
                bool startSysEx = midimsg.bytes[0] == 0xF0;
                bool endSysEx = midimsg.bytes[2] == 0xF7;
                if (!startSysEx && endSysEx) {
                    sysExIn_.push_back(midimsg.bytes[0]);
                    sysExIn_.push_back(midimsg.bytes[1]);
                    // create byte arrange and send from sysExIn_
                    // EraeApi::MidiMsg msgRecd = EraeApi::MidiMsg::create();
                    /// queue for later!
                    sysExActive_ = false;
                    sysExIn_.clear();
                } else {
                    if (endSysEx) {
                        sysExActive_ = false;
                        sysExIn_.clear();
                    }
                    // this is also fine, if its a single byte sysex
                    EraeApi::MidiMsg msgRecd =
                        EraeApi::MidiMsg::create(midimsg.bytes[0], midimsg.bytes[1], midimsg.bytes[2]);
                    /// queue for later!
                }
            }
        }
    }
    return true;
}

bool MMMidiDevice::queueOutMsg(const EraeApi::MidiMsg& msg) {
    // EraeApi when sending sysex will queue up a lot of messages
    return false;
}
bool MMMidiDevice::nextInMsg(EraeApi::MidiMsg& msg) {
    // called by eraeApi::process()->MidiDevice::processIn to get next input message
    // which it then will send to the callback handler, and interpret accordingly
    return false;
}
bool MMMidiDevice::nextOutMsg(EraeApi::MidiMsg& msg) {
    // called by eraeApi::process()->MidiDevice::processOut to get next output message
    // which it'll then use for send...
    return false;
}

bool MMMidiDevice::isOutputOpen() {
    return true;
}
bool MMMidiDevice::send(const EraeApi::MidiMsg& msg) {
    if (!isOutputOpen()) return false;

    // called by eraeApi::process()->MidiDevice::processOut to send a specfic message
    if (msg.size() > 0) {
        unsigned status = msg.byte(0);
        if (status == 0xF0) {
            if (msg.size() >= 2) {
                int payloadSz = msg.size() - 2;
                if (payloadSz == 0) {
                    rack::midi::Message midimsg;
                    midimsg.sysExNoPayload();
                    midiOuput_.sendMessage(midimsg);
                } else if (payloadSz == 1) {
                    rack::midi::Message midimsg;
                    midimsg.sysExSingleByte(msg.byte(1));
                    midiOuput_.sendMessage(midimsg);
                } else {
                    // multibyte
                    rack::midi::Message midimsg;
                    unsigned offset = 1;
                    midimsg.startSysEx(msg.byte(offset), msg.byte(offset + 1));
                    midiOuput_.sendMessage(midimsg);
                    offset += 2;
                    // we need 3 bytes, and the sysex end
                    while (offset + 3 < msg.size()) {
                        midimsg.continueSysEx(msg.byte(offset), msg.byte(offset + 1), msg.byte(offset + 2));
                        midiOuput_.sendMessage(midimsg);
                        offset += 3;
                    }

                    payloadSz = msg.size() - offset - 1;  // F7
                    if (payloadSz == 0) {
                        midimsg.endSysEx();
                        midiOuput_.sendMessage(midimsg);
                    } else if (payloadSz == 1) {
                        midimsg.endSysEx(msg.byte(offset));
                        midiOuput_.sendMessage(midimsg);
                    } else if (payloadSz == 2) {
                        midimsg.endSysEx(msg.byte(offset), msg.byte(offset + 1));
                        midiOuput_.sendMessage(midimsg);
                    }
                }
            }
            // malformed, need start and end...
        } else {
            rack::midi::Message midimsg;
            for (unsigned i = 0; msg.size(); i++) { midimsg.bytes[i] = msg.byte(i); }
            midiOuput_.sendMessage(midimsg);
        }
    }
    return false;
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
