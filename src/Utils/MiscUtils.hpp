#pragma once

#include "DataHandler.hpp"

class MiscUtils
{
	public:

	template <typename T = RE::TESObjectREFR, typename HandleT>
	static T* ResolveHandleAs(const HandleT& handle)
	{
		auto ptr = handle ? handle.get() : nullptr;
		if (!ptr) return nullptr;

		return ptr->As<T>();
	}

	static bool IsFormIDValid(const RE::FormID formID)
	{
		return (formID > 0x0 && formID < 0xFFFFFFFF);
	}

	template <typename T = RE::TESObjectREFR>
	static T* GetValidReference(RE::FormID formID, const bool extraChecks = false)
	{
		if (!MiscUtils::IsFormIDValid(formID)) return nullptr;
		return GetValidReference<T>(RE::TESForm::LookupByID<RE::TESObjectREFR>(formID), extraChecks);
	}

	template <typename T = RE::TESObjectREFR>
	static T* GetValidReference(RE::TESObjectREFR* ref, const bool extraChecks = false)
	{
		using namespace ModData;

		if (!ref || !ref->As<T>() || !MiscUtils::IsFormIDValid(ref->formID) || ref->IsDeleted()) return nullptr;
		
		if (extraChecks) {
			if (ref->IsDisabled() || ref->IsMarkedForDeletion()) return nullptr;
		}

		if constexpr (std::is_same_v<T, RE::Actor>) {
			auto* refActor = ref->As<RE::Actor>();
			if (!refActor || !ref->Is(RE::FormType::ActorCharacter)) return nullptr;
			
			if (extraChecks && (refActor->GetActorRuntimeData().criticalStage != RE::ACTOR_CRITICAL_STAGE::kNone)) return nullptr;
		}

		if constexpr (std::is_same_v<T, RE::Projectile>) {
			auto* refProjectile = ref->As<RE::Projectile>();
			if (!refProjectile) return nullptr;
			
			if (extraChecks && (refProjectile->GetProjectileRuntimeData().flags.any(RE::Projectile::Flags::kDestroyed))) return nullptr;
		}

		return ref->As<T>();
	}

	static float GetRandomNumber(float min = 0.0f, float max = 1.0f)
	{
		static std::mt19937 generator(std::random_device{}());
		std::uniform_real_distribution<float> distribution(min, max);
		return distribution(generator);
	}
};
