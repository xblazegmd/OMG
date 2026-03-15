#include <Geode/Geode.hpp>
#include <Geode/utils/string.hpp>
#include <Geode/fmod/fmod.h>
#include <Geode/fmod/fmod_errors.h>
#include <Geode/modify/PlayLayer.hpp>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

using namespace geode::prelude;

$execute {
	if (std::filesystem::create_directory(Mod::get()->getConfigDir() / "custom_reactions")) {
		log::info("Created config directory");
	}
}

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
		auto resources = Mod::get()->getResourcesDir();
		auto config = Mod::get()->getConfigDir() / "custom_reactions";

		std::vector<std::filesystem::path> options{
			resources /	"npesta-kenos.mp3",
			resources /	"riot-bloodbath.mp3",
			resources /	"knobbelboy-bloodlust.mp3",
			resources /	"kingsammelot-nhelv.mp3",
			resources /	"zoink-ts2.mp3",
			resources /	getSpaceUKSlaughterhouse(),
			resources /	"cuatrocientos-flamewall.mp3",
			resources /	"doggie-silentclubstep.mp3",
			resources /	"glow-unsaryneverclear.mp3",
			resources /	"cold-rupture.mp3",
			resources / "nebnoob-unzodiac.mp3"
		};

		for (const auto& file : std::filesystem::directory_iterator(config)) {
			if (file.is_regular_file() && file.path().extension() == ".mp3") {
				options.push_back(file.path());
			}
		}

		std::random_device rd;
		std::mt19937 rng(rd());
		std::uniform_int_distribution<> dist(0, options.size() - 1);

		auto path = options[dist(rng)];

		FMOD::Sound* ret;
		FMOD_RESULT res = m_fields->m_engine->m_system->createSound(
			string::pathToString(path).c_str(),
			FMOD_DEFAULT,
			nullptr,
			&ret
		);
		if (res != FMOD_OK) {
			return Err(
				"Could not make sound for '{}': {} (0x{:02X})", path, FMOD_ErrorString(res), (int)res
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