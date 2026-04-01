#include <Geode/Geode.hpp>
#include <Geode/utils/string.hpp>
#include <Geode/fmod/fmod.h>
#include <Geode/fmod/fmod_errors.h>
#include <Geode/modify/PlayLayer.hpp>
#include <string>

using namespace geode::prelude;

class $modify(PLHook, PlayLayer) {
	struct Fields {
		FMODAudioEngine* m_engine = FMODAudioEngine::sharedEngine();
		FMOD::Channel* m_channel;
	};

	void playEndAnimationToPos(CCPoint position) {
		PlayLayer::playEndAnimationToPos(position);
		playSound();
	}

	void playPlatformerEndAnimationToPos(CCPoint position, bool instant) {
		PlayLayer::playPlatformerEndAnimationToPos(position, instant);
		playSound();
	}

	void playSound() {
		auto mod = Mod::get();
		if (mod->getSettingValue<bool>("quick-enable")) { 
			Notification::create("BLUD FORGOT TO ENABLE THE MOD LOOOOOOOL", NotificationIcon::Error)->show();
			return;
		}
		if (mod->getSettingValue<bool>("no-practice") && m_isPracticeMode) return;
		if (mod->getSettingValue<bool>("no-startpos") && m_isTestMode) return;
		if (mod->getSettingValue<bool>("no-normal") && !m_isPracticeMode && !m_isTestMode) return;

		auto soundRes = getSound();
		if (soundRes.isErr()) {
			log::error("{}", soundRes.unwrapErr());
			return;
		}
		FMOD::Sound* sound = soundRes.unwrap();
		if (m_fields->m_engine->m_system->playSound(sound, nullptr, false, &m_fields->m_channel) == FMOD_OK) {
			m_fields->m_channel->setVolume(.8f); // IDC IF YOU GET A HEART ATTACK (for legal reasons that's a joke)
		}
	}

	Result<FMOD::Sound*> getSound() {
		auto file = getReactionPath();
		if (file.isErr()) {
			log::error("Please report this issue to the OMG! developer");
			return Err("{}", file.unwrapErr());
		}

		auto path = Mod::get()->getResourcesDir() / file.unwrap();

		FMOD::Sound* ret;
		FMOD_RESULT res = m_fields->m_engine->m_system->createSound(
			string::pathToString(path).c_str(),
			FMOD_DEFAULT,
			nullptr,
			&ret
		);
		if (res != FMOD_OK) {
			return Err(
				"Could not make sound for '{}': {} (0x{:02X})", file, FMOD_ErrorString(res), (int)res
			);
		}

		return Ok(ret);
	}

	Result<std::filesystem::path> getReactionPath() {
		return Ok(Mod::get()->getResourcesDir() / "six-seven.mp3");
	}

	void stopSound() {
		if (m_fields->m_channel) {
			m_fields->m_channel->stop();
			m_fields->m_channel = nullptr;
		}
	}

	void onExit() {
		PlayLayer::onExit();
		stopSound();
	}

	void resetLevel() {
		PlayLayer::resetLevel();
		stopSound();
	}
};