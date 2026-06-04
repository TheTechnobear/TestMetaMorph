#include <EraeApi.h>

#include <memory>
// #include <iostream>
// #define LOG_0(x) std::cerr << "ETL : " << x << std::endl;
#define LOG_0(x) 

#ifdef METAMODULE
#include "MMMidiDevice.h"
#else
#include <RtMidiDevice.h>
#endif 

#include "plugin.hpp"
struct EraeTouch : Module {
    enum ParamId { PARAMS_LEN };
    enum InputId { IN_STREAM_INPUT, INPUTS_LEN };
    enum OutputId { OUT_STREAM_OUTPUT, OUT_X_OUTPUT, OUT_Y_OUTPUT, OUT_Z_OUTPUT, OUT_TOUCH_OUTPUT, OUTPUTS_LEN };
    enum LightId { LED_S1_LIGHT, LIGHTS_LEN };

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
            device_ = std::make_shared<MMMidiDevice>();
#else
            device_ = std::make_shared<EraeApi::RtMidiDevice>();
#endif
            api_ = new EraeApi::EraeApi(device_, "Erae 2 MIDI");
            // api_ = new EraeApi::EraeApi("Erae Touch MIDI");
            callback_ = std::make_shared<ApiCallback>(this);
            api_->addCallback(callback_);

            api_->start();

            api_->enableApi();

            unsigned zone = 0;

            api_->requestZoneBoundary(zone);
            api_->clearZone(zone);
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
    }

    void processMidi(const midi::Message& msg);


    class ApiCallback;
    friend class ApiCallback;
    std::shared_ptr<ApiCallback> callback_;
    EraeApi::EraeApi* api_ = nullptr;
    midi::InputQueue midiInput;

    std::shared_ptr<EraeApi::MidiDevice> device_;

    struct Touch {
        float x_ = 0.0f;
        float y_ = 0.0f;
        float z_ = 0.0f;
        float touch_ = 0.0f;
    };

    static constexpr unsigned MAX_TOUCH = 16;
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

        void onZoneData(unsigned zone, unsigned width, unsigned height) override {
            LOG_0("onZoneData zone: " << zone << " : " << width << " , " << height);
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
};

#ifdef METAMODULE 
void EraeTouch::processMidi(const midi::Message& msg) {

}
#else 
void EraeTouch::processMidi(const midi::Message& midimsg) {
    // nop, as we use own midi stack, not vcv
    unsigned char bytemsg[3] = {0,0,0};
    for(int i=0;i<midimsg.getSize();i++) { bytemsg[i]=midimsg.bytes[i];}
    EraeApi::MidiMsg msg(bytemsg, 3);
    device_->queueInMsg(msg);
}
#endif



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
    }
};


Model* modelEraeTouch = createModel<EraeTouch, EraeTouchWidget>("EraeTouch");