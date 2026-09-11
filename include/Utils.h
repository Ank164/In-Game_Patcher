#pragma once

#include "SkyPrompt/API.hpp"

inline REL::Version Version;
inline bool SkyPlace_installed = false;
inline bool PatchingMode = false;
inline RE::ObjectRefHandle PatcherSelectedRef;


struct OverridesData {
    bool hasPos = false;
    bool hasRot = false;
    bool hasScale = false;

    float pos[3]{};
    float rot[3]{};
    float scale{};
};

struct BOSOriginalData {
    RE::FormID formID{};
    RE::NiPoint3 pos{};
    RE::NiPoint3 rot{};
    float scale{1.0f};
};

struct BOSTransform {
    std::string origRefID;
    std::string propertyOverrides;
};

struct KIDEntry {
    std::string keyword;
    std::string type;
    std::string objectID;
};

namespace SkyPlace {
    bool IsAvailable();
    bool MoveObject(RE::TESObjectREFR* ref);
    bool IsMovingObject();
    bool PlaceMovingObject();
    bool CancelMovingObject();
    bool SetObjectTransform(RE::TESObjectREFR* ref, const RE::NiPoint3& position,
                            const RE::NiPoint3& angle, float scale);
}

namespace Utils {

    bool IsDynamicForm(RE::TESForm* form);

    RE::BGSKeyword* FindOrCreateKeyword(std::string kwd);

    std::string NormalizeFormID(RE::TESForm* form);

    RE::TESForm* GetFormFromString(const std::string& str);
    const RE::TESFile* GetMasterFile(RE::TESForm* ref);
    std::string GetObjectTypeName(RE::TESBoundObject* obj);

    OverridesData ParseOverrides(const std::string& str);
    std::string BuildOverrides(const OverridesData& data);

    bool CreateNewBOSFile(const std::string& filePath);
    bool CreateNewKIDFile(const std::string& filePath);

    std::vector<RE::TESForm*> GetIndirectKIDTargets(RE::TESObjectREFR* ref);

    bool IsPluginLoaded(const std::string& pluginName);

}
