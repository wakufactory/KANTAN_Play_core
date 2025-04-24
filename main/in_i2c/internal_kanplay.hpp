#ifndef KANPLAY_INTERNAL_KANPLAY_HPP
#define KANPLAY_INTERNAL_KANPLAY_HPP

#include "../system_registry.hpp"

namespace kanplay_ns {
//-------------------------------------------------------------------------
// かんぷれ本体制御用クラスの基本形。本体のハードウェアのバリエーションに応じて派生型を作る
class interface_internal_kanplay_t {
public:
    virtual bool init(void) = 0;
    virtual bool update(void) = 0;
    virtual void setupInterrupt(void) {}

    virtual uint8_t getFirmwareVersion(void) { return 0; }
    virtual bool checkUpdate(void) { return 0; }
    virtual bool execFirmwareUpdate(void) { return 0; }
    virtual void mute(void) {}
};

class internal_kanplay_t : public interface_internal_kanplay_t {
public:
    bool init(void) override;
    bool update(void) override;
    void setupInterrupt(void) override;

    uint8_t getFirmwareVersion(void) override;
    bool checkUpdate(void) override;
    bool execFirmwareUpdate(void) override;
    void mute(void) override;
protected:
    registry_t::history_code_t rgbled_history_code = 0;
};

//-------------------------------------------------------------------------
}; // namespace kanplay_ns

#endif
