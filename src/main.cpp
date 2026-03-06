#include <Geode/Geode.hpp>
#include <Geode/utils/string.hpp>
#include <Geode/fmod/fmod.h>
#include <Geode/fmod/fmod_errors.h>
#include <Geode/modify/PlayLayer.hpp>
#include <random>
#include <string>
#include <vector>

using namespace geode::prelude;

class $modify(PLHook, PlayLayer) {
	struct Fields {
		FMODAudioEngine* m_engine = FMODAudioEngine::sharedEngine();
		FMOD::Channel* m_channel;
	};

	void playEndAnimationToPos(CCPoint position) {
		PlayLayer::playEndAnimationToPos(position);
		auto mod = Mod::get();
		if (!mod->getSettingValue<bool>("quick-enable")) return;
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
			float volume = mod->getSettingValue<int64_t>("volume") / 200.f;
			m_fields->m_channel->setVolume(volume);
		}
	}

	inline std::string getSpaceUKSlaughterhouse() {
		return Mod::get()->getSettingValue<bool>("swearuk") ? "swearuk-slaughterhouse.mp3" : "spaceuk-slaughterhouse.mp3";
	}

	Result<FMOD::Sound*> getSound() {
		std::vector<std::string> options{
			"npesta-kenos.mp3",
			"riot-bloodbath.mp3",
			"knobbelboy-bloodlust.mp3",
			"kingsammelot-nhelv.mp3",
			"zoink-ts2.mp3",
			getSpaceUKSlaughterhouse(),
			"cuatrocientos-flamewall.mp3",
			"doggie-silentclubstep.mp3",
			"glow-unsaryneverclear.mp3",
			"cold-rupture.mp3"
		};
		std::random_device rd;
		std::mt19937 rng(rd());
		std::uniform_int_distribution<> dist(0, options.size() - 1);

		std::string file = options[dist(rng)];
		auto path = Mod::get()->getResourcesDir() / file;

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