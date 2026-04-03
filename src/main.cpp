#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/utils/string.hpp>
#include <Geode/fmod/fmod.h>
#include <Geode/fmod/fmod_errors.h>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

using namespace geode::prelude;

class $modify(PLHook, PlayLayer) {
	struct Fields {
		FMODAudioEngine* m_engine = FMODAudioEngine::sharedEngine();
		FMOD::Channel* m_channel;
		utils::StringMap<FMOD::Sound*> m_preloadedSounds;
	};

	void preloadSounds() {
		m_fields->m_preloadedSounds.clear();
		for (const auto& [key, file] : getFiles()) {
			auto path = Mod::get()->getResourcesDir() / file;
			auto sound = makeSound(string::pathToString(path).c_str());
			if (sound.isErr()) {
				log::error("Failed to preload sound '{}': {}", file, sound.unwrapErr());
				continue;
			}
			m_fields->m_preloadedSounds[key] = sound.unwrap();
			log::info("Preloaded sound '{}'", file);
		}

		// Preload custom reactions
		auto reaction = Mod::get()->getSettingValue<std::string>("reaction");
		if (reaction == "Custom" || reaction == "Random (With Custom)") {
			auto custom = getCustomReaction();
			if (custom.isErr()) {
				log::error("Failed to preload custom reaction: {}", custom.unwrapErr());
				return;
			}

			auto sound = makeSound(string::pathToString(custom.unwrap()).c_str());
			if (sound.isErr()) {
				log::error("Failed to preload custom reaction '{}': {}", custom.unwrap(), sound.unwrapErr());
			} else {
				m_fields->m_preloadedSounds["Custom"] = sound.unwrap();
				log::info("Preloaded custom reaction '{}'", custom.unwrap());
			}
		}
	}

	bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
		if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
		if (Mod::get()->getSettingValue<bool>("preload-sounds")) {
			log::info("Preloading sounds...");
			preloadSounds();
		}
		return true;
	}

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
		if (!mod->getSettingValue<bool>("quick-enable")) return;
		if (mod->getSettingValue<bool>("no-practice") && m_isPracticeMode) return;
		if (mod->getSettingValue<bool>("no-startpos") && m_isTestMode) return;
		if (mod->getSettingValue<bool>("no-normal") && !m_isPracticeMode && !m_isTestMode) return;
		if (mod->getSettingValue<bool>("no-platformer") && m_isPlatformer) return;

		FMOD::Sound* sound;
		if (Mod::get()->getSettingValue<bool>("preload-sounds")) {
			auto soundRes = getPreloadedSound();
			if (soundRes.isErr()) {
				log::error("{}", soundRes.unwrapErr());
				return;
			}
			sound = soundRes.unwrap();
	 	} else {
			auto soundRes = getSound();
			if (soundRes.isErr()) {
				log::error("{}", soundRes.unwrapErr());
				return;
			}
			sound = soundRes.unwrap();
		}

		if (m_fields->m_engine->m_system->playSound(sound, nullptr, false, &m_fields->m_channel) == FMOD_OK) {
			float volume = mod->getSettingValue<int64_t>("volume") / 100.f;
			m_fields->m_channel->setVolume(volume);
		}
	}

	inline Result<FMOD::Sound*> getSound() {
		auto file = getReactionPath();
		if (file.isErr()) {
			return Err("{}", file.unwrapErr());
		}
		auto ret = makeSound(string::pathToString(file.unwrap()).c_str());
		if (ret.isErr()) {
			return Err("{}", ret.unwrapErr());
		}
		return Ok(ret.unwrap());
	}

	Result<FMOD::Sound*> getPreloadedSound() {
		if (m_fields->m_preloadedSounds.empty()) {
			return Err("There are no preloaded sounds");
		}

		std::string reaction = Mod::get()->getSettingValue<std::string>("reaction");
		if (reaction == "Random") {
			// Remove custom sound and pick randomly
			auto preloaded = m_fields->m_preloadedSounds;
			preloaded.erase("Custom");

			auto it = preloaded.begin();
			std::advance(it, randint(0, preloaded.size() - 1));
			return Ok(it->second);
		} else if (reaction == "Random (With Custom)") {
			auto it = m_fields->m_preloadedSounds.begin();
			std::advance(it, randint(0, m_fields->m_preloadedSounds.size() - 1));
			return Ok(it->second);
		}

		if (m_fields->m_preloadedSounds.count(reaction)) {
			return Ok(m_fields->m_preloadedSounds[reaction]);
		}

		// This should be unreachable so if we reach this, it's definitely a bug
		log::error("Please report this issue to the OMG! developer");
		return Err("Unknown reaction: {} (THIS SHOULD BE UNREACHABLE)", reaction);
	}

	Result<FMOD::Sound*> makeSound(const char* file) {
		FMOD::Sound* ret;
		FMOD_RESULT res = m_fields->m_engine->m_system->createSound(
			file,
			FMOD_DEFAULT,
			nullptr,
			&ret
		);
		if (res != FMOD_OK) {
			return Err("Could not make sound for '{}': {} (0x{:02X})", file, FMOD_ErrorString(res), (int)res);
		}
		return Ok(ret);
	}

	Result<std::filesystem::path> getReactionPath() {
		auto resources = Mod::get()->getResourcesDir();
		auto files = getFiles();

		std::string reaction = Mod::get()->getSettingValue<std::string>("reaction");
		if (reaction == "Random") {
			auto it = files.begin();
			std::advance(it, randint(0, files.size() - 1));
			return Ok(resources / it->second);
		} else if (reaction == "Random (With Custom)") {
			std::vector<std::filesystem::path> filePaths;
			for (const auto& [_, file] : files) {
				filePaths.push_back(Mod::get()->getResourcesDir() / file);
			}

			auto custom = getCustomReaction();
			if (custom.isErr()) {
				return Err("Could not get custom reactions: {}", custom.unwrapErr());
			}
			filePaths.push_back(custom.unwrap());

			return Ok(filePaths[randint(0, filePaths.size() - 1)]);
		} else if (reaction == "Custom") {
			return getCustomReaction();
		} else if (files.count(reaction)) {
			return Ok(resources / files[reaction]);
		} else {
			// This should be unreachable so if we reach this, it's definitely a bug
			log::error("Please report this issue to the OMG! developer");
			return Err("Unknown reaction: {} (THIS SHOULD BE UNREACHABLE)", reaction);
		}
	}

	inline std::string getNormalOrSwear(
		const std::string& normal,
		const std::string& swear,
		const std::string& level
	) {
		return fmt::format("{}-{}.ogg", Mod::get()->getSettingValue<bool>("swearuk") ? swear : normal, level);
	}

	inline utils::StringMap<std::string> getFiles() {
		return {
			{"Kenos (Npesta)", 								"npesta-kenos.ogg"},
			{"Bloodbath (Riot)", 							"riot-bloodbath.ogg"},
			{"Bloodlust (Knobbelboy)", 						"knobbelboy-bloodlust.ogg"},
			{"Nhelv (Kingsammelot)", 						"kingsammelot-nhelv.ogg"},
			{"Thinking Space II", 							"zoink-ts2.ogg"},
			{"Slaugherhouse (SpaceUK's \"completion\")", 	getNormalOrSwear("spaceuk", "swearuk", "slaughterhouse")},
			{"Silent Clubstep (Doggie)", 					"doggie-silentclubstep.ogg"},
			{"Unnerfed Sary Never Clear (Glow)", 			"glow-unsaryneverclear.ogg"},
			{"Rupture (Cold)", 								"cold-rupture.ogg"},
			{"Unnerfed Zodiac (nebnoob)", 					"nebnoob-unzodiac.ogg"},
			{"Orbit (Zoink)", 								getNormalOrSwear("zoink", "swoink", "orbit")},
			{"Artificial Ascent (Kingsammelot)", 			"kingsammelot-artificialascent.ogg"},
			{"Deimos (Npesta)", 							getNormalOrSwear("npesta", "swearpesta", "deimos")},
			{"Tartarus (AeonAir)", 							"aeonair-tartarus.ogg"},
			{"WOW (Npesta)", 								"npesta-wow.ogg"},
			{"Killbot (Kingsammelot)", 						"kingsammelot-killbot.ogg"}
		};
	}

	int randint(int a, int b) {
		static std::random_device rd;
		static std::mt19937 rng(rd());
		std::uniform_int_distribution<> dist(a, b);
		return dist(rng);
	}

	Result<std::filesystem::path> getCustomReaction() const {
		auto reaction = Mod::get()->getSettingValue<std::filesystem::path>("custom-reaction");
		if (!std::filesystem::exists(reaction)) {
			return Err("The custom reaction's file was not found");
		}
		return Ok(reaction);
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