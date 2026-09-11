#include "Manager.h"
#include "Events.h"
#include "Utils.h"
#include "BOS.h"
#include "Translations.h"

namespace TrStSkPr = Translations::Strings::SkyPrompt;

void PatcherPromptSink::RegisterSkyPrompt(std::string lang) {
    clientID = SkyPromptAPI::RequestClientID();
    if(SkyPromptAPI::RequestTheme(clientID, "InGamePatcher")) {
        logger::info("SkyPrompt theme InGamePatcher registered");
    } else {
        logger::error("SkyPrompt theme InGamePatcher registration failed");
    }
}

std::span<const SkyPromptAPI::Prompt> PatcherPromptSink::GetPrompts() const {
    if (console) {
        if (PatchingMode) {
            return ExitPrompt;
        } else {
            return EnterPrompt;
        }
    }
    return PatcherPrompts;
}

void PatcherPromptSink::ProcessEvent(SkyPromptAPI::PromptEvent event) const {
    if (event.type == SkyPromptAPI::PromptEventType::kAccepted) {
        if (event.prompt.eventID == 1) {
            if (event.prompt.actionID == 1) {
                if (!PatchingMode) {
                    logger::warn("Ignored Move prompt while Patching Mode is disabled");
                    return;
                }
                auto ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(event.prompt.refid);
                if (ref) {
                    SkyPromptAPI::RemovePrompt(PatcherPromptSink::GetSingleton(), clientID);
                    BOSIniManager::GetSingleton()->RememberOriginal(ref);
                    if (RE::NiAVObject* object3D = ref->Get3D()) {
                        object3D->TintScenegraph(RE::NiColorA(0, 0, 0, 0));
                    }
                    if (SkyPlace_installed && SkyPlace::MoveObject(ref)) {
                        dragging = true;
                        logger::info("Started moving object with SkyPlace: {}", ref->GetName());
                        if (const SKSE::TaskInterface* tasks = SKSE::GetTaskInterface()) {
                            tasks->AddTask([this]() {
                                if (!SkyPromptAPI::SendPrompt(this, clientID)) {
                                    logger::error("Failed to show In-Game Patcher movement controls");
                                }
                            });
                        }
                    } else {
                        RE::SendHUDMessage::ShowHUDMessage(
                            SkyPlace_installed ?
                                "SkyPlace could not move this object" :
                                "SkyPlace with In-Game Patcher API support is not installed");
                        if (RE::NiAVObject* object3D = ref->Get3D()) {
                            object3D->TintScenegraph(RE::NiColorA(0, 1.0f, 0, 0.5f));
                        }
                        if (!SkyPromptAPI::SendPrompt(PatcherPromptSink::GetSingleton(), clientID)) {
                            logger::error("Failed to restore the patcher prompt after SkyPlace rejected the object");
                        }
                    }
                }
            } else if (event.prompt.actionID == 4) {
                logger::info("Entered Patch Mode");
                SkyPromptAPI::RemovePrompt(PatcherPromptSink::GetSingleton(), clientID);
                PatchingMode = true;
                if (!SkyPromptAPI::SendPrompt(PatcherPromptSink::GetSingleton(), clientID)) {
                    logger::error("Failed to send prompt to SkyPrompt after entering patch mode");
                }
            } else if (event.prompt.actionID == 5) {
                logger::info("Exited Patch Mode");
                SkyPromptAPI::RemovePrompt(PatcherPromptSink::GetSingleton(), clientID);
                PatchingMode = false;
                if (!SkyPromptAPI::SendPrompt(PatcherPromptSink::GetSingleton(), clientID)) {
                    logger::error("Failed to send prompt to SkyPrompt after exiting patch mode");
                }
            }
        }
        else if (event.prompt.eventID == 2) {
             if (event.prompt.actionID == 2) {
                auto ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(event.prompt.refid);
                 if (dragging) {
                     SkyPlace::CancelMovingObject();
                     dragging = false;
                 }
                 if (ref) {
                     BOSIniManager::GetSingleton()->RememberOriginal(ref);
                     BOSIniManager::GetSingleton()->RemoveObject(ref);
                 }
                 SkyPromptAPI::RemovePrompt(PatcherPromptSink::GetSingleton(), clientID);
             }
        }
        else if(event.prompt.eventID == 3) {
            if (event.prompt.actionID == 3) {
                SkyPromptAPI::RemovePrompt(PatcherPromptSink::GetSingleton(), clientID);
                auto ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(event.prompt.refid);
                if (dragging) {
                    SkyPlace::PlaceMovingObject();
                    dragging = false;
                    logger::info("Committed movement with Save Position");
                }
                if (ref) {
                    if (!Utils::IsDynamicForm(ref)) {
                        BOSIniManager::GetSingleton()->TransformObject(ref);
                    }
                }
                if (!SkyPromptAPI::SendPrompt(PatcherPromptSink::GetSingleton(), clientID)) {
                    logger::error("Failed to send prompt to SkyPrompt after saving object");
                }
            }
        } else if (event.prompt.eventID == 4) {
            if (event.prompt.actionID == 4) {
                SkyPromptAPI::RemovePrompt(PatcherPromptSink::GetSingleton(), clientID);
                auto ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(event.prompt.refid);
                if (dragging) {
                    SkyPlace::CancelMovingObject();
                    dragging = false;
                    logger::info("Cancelled movement with Reset Position");
                } else if (ref) {
                    BOSIniManager::GetSingleton()->ResetObject(ref);
                }
                if (!SkyPromptAPI::SendPrompt(PatcherPromptSink::GetSingleton(), clientID)) {
                    logger::error("Failed to send prompt to SkyPrompt after resetting object");
                }
            }
        }
    } else if (event.type == SkyPromptAPI::PromptEventType::kDeclined) {
        SkyPromptAPI::RemovePrompt(PatcherPromptSink::GetSingleton(), clientID);
    } else if (event.type == SkyPromptAPI::PromptEventType::kTimingOut) {
        if (console || PatchingMode) {
            if (!SkyPromptAPI::SendPrompt(PatcherPromptSink::GetSingleton(), clientID)) {
                logger::error("Failed to send prompt to SkyPrompt on timeout");
            }
        } else {
            SkyPromptAPI::RemovePrompt(PatcherPromptSink::GetSingleton(), clientID);
        }
    }
}

void PatcherPromptSink::SetRef(RE::TESObjectREFR* ref) {
    PatcherSelectedRef = ref ? ref->GetHandle() : RE::ObjectRefHandle{};
    if (!ref) {
        return;
    }
    auto formID = ref->GetFormID();
    PatcherPrompts = {SkyPromptAPI::Prompt(TrStSkPr::move, 1, 1, SkyPromptAPI::PromptType::kHold, formID),
                      SkyPromptAPI::Prompt(TrStSkPr::remove, 2, 2, SkyPromptAPI::PromptType::kHold, formID),
                      SkyPromptAPI::Prompt(TrStSkPr::save, 3, 3, SkyPromptAPI::PromptType::kHold, formID),
                      SkyPromptAPI::Prompt(TrStSkPr::reset, 4, 4, SkyPromptAPI::PromptType::kHold, formID)};
}

void PatcherPromptSink::InitPrompts() {
    EnterPrompt = {SkyPromptAPI::Prompt(TrStSkPr::enter, 1, 4, SkyPromptAPI::PromptType::kHold)};
    ExitPrompt = {SkyPromptAPI::Prompt(TrStSkPr::exit, 1, 5, SkyPromptAPI::PromptType::kHold)};

    PatcherPrompts = {SkyPromptAPI::Prompt(TrStSkPr::move,1, 1, SkyPromptAPI::PromptType::kHold),
                      SkyPromptAPI::Prompt(TrStSkPr::remove, 2, 2, SkyPromptAPI::PromptType::kHold),
                      SkyPromptAPI::Prompt(TrStSkPr::save, 3, 3, SkyPromptAPI::PromptType::kHold),
                      SkyPromptAPI::Prompt(TrStSkPr::reset, 4, 4, SkyPromptAPI::PromptType::kHold)};

}
