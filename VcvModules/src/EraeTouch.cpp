#include <EraeApi.h>

#include <iostream>
#include <memory>

#define LOG_0(x)

#include "plugin.hpp"

#ifdef METAMODULE
#include "MMMidiDevice.h"
#else
#include "VcvMidiDevice.h"
// #include <RtMidiDevice.h>
#include <stdio.h>
#endif


struct EraeTouch : Module {
    enum ParamId { PARAMS_LEN };
    enum InputId { IN_STREAM_INPUT, INPUTS_LEN };
    enum OutputId { OUT_STREAM_OUTPUT, OUT_X_OUTPUT, OUT_Y_OUTPUT, OUT_Z_OUTPUT, OUT_TOUCH_OUTPUT, OUTPUTS_LEN };
    enum LightId { LED_S1_LIGHT, LIGHTS_LEN };
    enum DisplayId { TEXT_DISPLAY = LIGHTS_LEN };


    EraeTouch() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configInput(IN_STREAM_INPUT, "");
        configOutput(OUT_STREAM_OUTPUT, "");
        configOutput(OUT_X_OUTPUT, "");
        configOutput(OUT_Y_OUTPUT, "");
        configOutput(OUT_Z_OUTPUT, "");
        configOutput(OUT_TOUCH_OUTPUT, "");

        if (api_ == nullptr) {
#ifdef METAMODULE
            // midiInput.setDriverId(midiInput.getDefaultDriverId());
            midiInput.setDeviceId(midiInput.getDefaultDeviceId());
            midiInput.setChannel(-1);
            device_ = std::make_shared<MMMidiDevice>();
#else
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

                if (eraeInputDeviceId >= 0) {
                    printf("Found Erae Input\n");
                    midiInput.setDriverId(coreMidiId);
                    midiInput.setDeviceId(eraeInputDeviceId);
                    midiInput.setChannel(-1);  // all channels
                }
                // if (eraeOutputDeviceId >= 0) {
                //     printf("Found Erae Output\n");
                //     midiOutput.setDriverId(coreMidiId);
                //     midiOutput.setDeviceId(eraeOutputDeviceId);
                // }
            }


            // device_ = std::make_shared<EraeApi::RtMidiDevice>();
            device_ = std::make_shared<VcvMidiDevice>();
#endif
            api_ = new EraeApi::EraeApi(device_, "Erae 2 MIDI");
            // api_ = new EraeApi::EraeApi("Erae Touch MIDI");
            callback_ = std::make_shared<ApiCallback>(this);
            api_->addCallback(callback_);

            api_->start();

            // unsigned x = 5, y = 5, w = 20, h = 10;
            // unsigned rgb = 0xFFFFFF;
            // api_->drawPixel(zone, x, y, rgb);
            // api_->drawRectangle(zone,  x,  y,  w,  h,  rgb);
            // api_->drawImage(zone,  x,  y,  w,  h, unsigned *rgb);
        }
    }

    ~EraeTouch() {
        if (api_) {
            api_->disableApi();
            api_->stop();
            delete api_;
            api_ = nullptr;
        }
    }

    void process(const ProcessArgs& args) override {
        midi::Message msg;
        while (midiInput.tryPop(&msg, args.frame)) { processMidi(msg); }

        if (api_ != nullptr) { api_->process(); }

        if(!initDone_) {
            if(initTimer_==0) {
                api_->enableApi();
                initTimer_ = INIT_TIMER;
                initCount_++;
                debugString_="TX enableApi" + std::to_string(initCount_);
                requestVersion();
            } else if(initTimer_== INIT_TIMER / 2) {
                // requestVersion();
            }
            initTimer_ -= initTimer_ > 0;
        }

        outputs[OUT_X_OUTPUT].setChannels(MAX_TOUCH);
        outputs[OUT_Y_OUTPUT].setChannels(MAX_TOUCH);
        outputs[OUT_Z_OUTPUT].setChannels(MAX_TOUCH);
        outputs[OUT_TOUCH_OUTPUT].setChannels(MAX_TOUCH);

        auto& zoneinfo = zones_[0];
        for (unsigned i = 0; i < MAX_TOUCH; i++) {
            auto& touchinfo = zoneinfo.touches_[i];
            outputs[OUT_X_OUTPUT].setVoltage(touchinfo.x_, i);
            outputs[OUT_Y_OUTPUT].setVoltage(touchinfo.y_, i);
            outputs[OUT_Z_OUTPUT].setVoltage(touchinfo.z_, i);
            outputs[OUT_TOUCH_OUTPUT].setVoltage(touchinfo.touch_, i);
        }
        lights[LED_S1_LIGHT].setBrightness(ledCounter_ > 0 ? 1.0f : 0.0f);
        ledCounter_ -= ledCounter_ > 0;
    }

    void processMidi(const midi::Message& msg);

    void requestVersion() {
        debugString_="TX requestVersion" + std::to_string(initCount_);
        if(api_) api_->requestVersion();
    }

    void clearZone() {
        debugString_="TX clearZone" + std::to_string(initCount_);
        if(api_) api_->clearZone(zone_);
    }

    void requestZoneBoundary() {
        debugString_="TX requestZoneBoundary" + std::to_string(initCount_);
        if(api_) api_->requestZoneBoundary(zone_);
    }


    class ApiCallback;
    friend class ApiCallback;
    std::shared_ptr<ApiCallback> callback_;
    EraeApi::EraeApi* api_ = nullptr;
    int ledCounter_ = 0;
    midi::InputQueue midiInput;

    std::string debugString_ = "";

    static constexpr int INIT_TIMER = 1000;
    int initTimer_ = INIT_TIMER;
    bool initDone_ = false;
    int initCount_ = 0;
    int zone_ = 0;


#ifdef METAMODULE
    std::shared_ptr<MMMidiDevice> device_;
#else
    std::shared_ptr<VcvMidiDevice> device_;
    // std::shared_ptr<EraeApi::RtMidiDevice> device_;
#endif

    struct Touch {
        float x_ = 0.0f;
        float y_ = 0.0f;
        float z_ = 0.0f;
        float touch_ = 0.0f;
    };

#ifdef METAMODULE
    static constexpr unsigned MAX_TOUCH = 1;
#else
    static constexpr unsigned MAX_TOUCH = rack::engine::PORT_MAX_CHANNELS;
#endif

    static constexpr unsigned MAX_ZONE = 1;
    static constexpr float MAX_V = 10.f;
    struct Zone {
        Touch touches_[16];
        float height_ = 1.0f;
        float width_ = 1.0f;
    } zones_[MAX_ZONE];


    class ApiCallback : public EraeApi::EraeApiCallback {
    public:
        ApiCallback(EraeTouch* m) : module_(m) {}

        ~ApiCallback() override {}

        void onInit() override { LOG_0("onInit"); }
        void onDeinit() override { LOG_0("onDeinit"); }
        void onError(unsigned err, const char* errStr) override { LOG_0("onError " << err << " : " << errStr); }

        // api
        void onStartTouch(unsigned zone, unsigned finger, float x, float y, float z) override {
            module_->ledCounter_ = 96000;
            unsigned touch = finger % MAX_TOUCH;
            // LOG_0("onStartTouch zone: " << zone << " touch " << touch << " " << x << " , " << y << " , " << z);
            if (zone < MAX_ZONE) {
                auto& zoneinfo = module_->zones_[zone];
                if (touch < MAX_TOUCH) {
                    module_->api_->drawPixel(zone, x, y, 0xff0000);
                    auto& touchinfo = zoneinfo.touches_[touch];
                    touchinfo.x_ = (float(x) / zoneinfo.width_) * MAX_V;
                    touchinfo.y_ = (float(y) / zoneinfo.height_) * MAX_V;
                    touchinfo.z_ = (float(z) / 1.0f) * MAX_V;
                    touchinfo.touch_ = MAX_V;
                }
            }
        }

        void onSlideTouch(unsigned zone, unsigned finger, float x, float y, float z) override {
            unsigned touch = finger % MAX_TOUCH;
            // LOG_0("onSlideTouch zone: " << zone << " touch " << touch << " " << x << " , " << y << " , " << z);
            if (zone < MAX_ZONE) {
                auto& zoneinfo = module_->zones_[zone];
                if (touch < MAX_TOUCH) {
                    module_->api_->drawPixel(zone, x, y, 0x00ff00);
                    auto& touchinfo = zoneinfo.touches_[touch];
                    touchinfo.x_ = (float(x) / zoneinfo.width_) * MAX_V;
                    touchinfo.y_ = (float(y) / zoneinfo.height_) * MAX_V;
                    touchinfo.z_ = (float(z) / 1.0f) * MAX_V;
                    touchinfo.touch_ = MAX_V;
                    // touchinfo.touch_ = 5.0f;
                }
            }
        }

        void onEndTouch(unsigned zone, unsigned finger, float x, float y, float z) override {
            unsigned touch = finger % MAX_TOUCH;
            module_->ledCounter_ = 0;
            // LOG_0("onEndTouch zone: " << zone << " touch " << touch << " " << x << " , " << y << " , " << z);
            if (zone < MAX_ZONE) {
                auto& zoneinfo = module_->zones_[zone];
                if (touch < MAX_TOUCH) {
                    module_->api_->drawPixel(zone, x, y, 0xffffff);
                    auto& touchinfo = zoneinfo.touches_[touch];
                    touchinfo.x_ = (float(x) / zoneinfo.width_) * MAX_V;
                    touchinfo.y_ = (float(y) / zoneinfo.height_) * MAX_V;
                    // touchinfo.z_ = (float(z) / 1.0f) * MAX_V;
                    touchinfo.z_ = 0.0f;
                    touchinfo.touch_ = 0.0f;
                }
            }
        }
        void onVersion(unsigned version) override{
            module_->initDone_ = true;
            module_->debugString_="RX onVersion";
            module_->requestZoneBoundary();
        }

        void onZoneData(unsigned zone, unsigned width, unsigned height) override {
            module_->debugString_="RX onZoneData";
            LOG_0("onZoneData zone: " << zone << " : " << width << " , " << height);
            module_->clearZone();
            if (zone < MAX_ZONE) {
                auto& zoneinfo = module_->zones_[zone];
                zoneinfo.width_ = float(width);
                zoneinfo.height_ = float(height);
            }
        }

        // midi
        void noteOn(unsigned ch, unsigned n, unsigned v) override {}
        void noteOff(unsigned ch, unsigned n, unsigned v) override {}
        void cc(unsigned ch, unsigned cc, unsigned v) override {}
        void pitchbend(unsigned ch, int v) override {}  // +/- 8192
        void ch_pressure(unsigned ch, unsigned v) override {}

        EraeTouch* module_ = nullptr;
    };

#ifdef METAMODULE
    size_t get_display_text(int display_id, std::span<char> text) override {
        if (display_id == TEXT_DISPLAY) {
            std::string someText = debugString_;
            std::string formatted;
            for (size_t i = 0; i < someText.size(); ++i) {
                if (i > 0 && i % 16 == 0) formatted += '\n';
                formatted += someText[i];
            }

            size_t chars_to_copy = std::min(formatted.size(), text.size());
            std::copy(formatted.data(), formatted.data() + chars_to_copy, text.begin());
            return chars_to_copy;
        }
        return 0;
    }
#endif
};

void EraeTouch::processMidi(const midi::Message& midimsg) {
    device_->onMessage(midimsg);
}


struct EraeTouchWidget : ModuleWidget {
    EraeTouchWidget(EraeTouch* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/EraeTouch.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(14.955, 21.668)), module, EraeTouch::IN_STREAM_INPUT));

        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(46.709, 21.677)), module, EraeTouch::OUT_STREAM_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(12.977, 87.842)), module, EraeTouch::OUT_X_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(25.182, 87.842)), module, EraeTouch::OUT_Y_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(37.374, 87.842)), module, EraeTouch::OUT_Z_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(48.486, 87.842)), module, EraeTouch::OUT_TOUCH_OUTPUT));

        addChild(
            createLightCentered<MediumLight<RedLight>>(mm2px(Vec(30.284, 21.476)), module, EraeTouch::LED_S1_LIGHT));

#ifdef METAMODULE
        auto display = createWidget<MetaModule::VCVTextDisplay>(mm2px(Vec(4, 40)));
        display->box.size = mm2px(Vec(50, 100));
        display->firstLightId = EraeTouch::TEXT_DISPLAY;
        display->font = "Default_10";
        display->color = Colors565::Green;
        addChild(display);
#endif
    }
};


Model* modelEraeTouch = createModel<EraeTouch, EraeTouchWidget>("EraeTouch");