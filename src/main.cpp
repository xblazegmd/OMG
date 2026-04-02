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
		for (const auto& [key, file] : std::initializer_list<std::pair<std::string, std::string>>{
			{"Kenos (Npesta)", 								"npesta-kenos.mp3"},
			{"Bloodbath (Riot)", 							"riot-bloodbath.mp3"},
			{"Bloodlust (Knobbelboy)", 						"knobbelboy-bloodlust.mp3"},
			{"Nhelv (Kingsammelot)", 						"kingsammelot-nhelv.mp3"},
			{"Thinking Space II", 							"zoink-ts2.mp3"},
			{"Slaugherhouse (SpaceUK's \"completion\")", 	getNormalOrSwear("spaceuk", "swearuk", "slaughterhouse")},
			{"Silent Clubstep (Doggie)", 					"doggie-silentclubstep.mp3"},
			{"Unnerfed Sary Never Clear (Glow)", 			"glow-unsaryneverclear.mp3"},
			{"Rupture (Cold)", 								"cold-rupture.mp3"},
			{"Unnerfed Zodiac (nebnoob)", 					"nebnoob-unzodiac.mp3"},
			{"Orbit (Zoink)", 								getNormalOrSwear("zoink", "swoink", "orbit")},
			{"Artificial Ascent (Kingsammelot)", 			"kingsammelot-artificialascent.mp3"},
			{"Deimos (Npesta)", 							getNormalOrSwear("npesta", "swearpesta", "deimos")},
			{"Tartarus (AeonAir)", 							"aeonair-tartarus.mp3"},
			{"WOW (Npesta)", 								"npesta-wow.mp3"},
			{"Killbot (Kingsammelot)", 						"kingsammelot-killbot.mp3"}
		}) {
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

	Result<FMOD::Sound*> getSound() {
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

		for (const auto& [key, sound] : m_fields->m_preloadedSounds) {
			if (key == reaction) {
				return Ok(sound);
			}
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

	inline std::string getNormalOrSwear(
		const std::string& normal,
		const std::string& swear,
		const std::string& level
	) {
		return fmt::format("{}-{}.mp3", Mod::get()->getSettingValue<bool>("swearuk") ? swear : normal, level);
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

	Result<std::filesystem::path> getReactionPath() {
		std::vector<std::string> options{
			"npesta-kenos.mp3",
			"riot-bloodbath.mp3",
			"knobbelboy-bloodlust.mp3",
			"kingsammelot-nhelv.mp3",
			"zoink-ts2.mp3",
			getNormalOrSwear("spaceuk", "swearuk", "slaughterhouse"),
			"cuatrocientos-flamewall.mp3",
			"doggie-silentclubstep.mp3",
			"glow-unsaryneverclear.mp3",
			"cold-rupture.mp3",
			"nebnoob-unzodiac.mp3",
			getNormalOrSwear("zoink", "swoink", "orbit"), // W swoink
			"kingsammelot-artificialascent.mp3",
			getNormalOrSwear("npesta", "swearpesta", "deimos"),
			"aeonair-tartarus.mp3",
			"npesta-wow.mp3",
			"kingsammelot-killbot.mp3"
		};

		std::string file;

		// Wonderful if-else chain ;-;
		std::string reaction = Mod::get()->getSettingValue<std::string>("reaction");
		if (reaction == "Random") {
			file = options[randint(0, options.size() - 1)];
		} else if (reaction == "Custom") {
			return getCustomReaction();
		} else if (reaction == "Random (With Custom)") {
			std::vector<std::filesystem::path> files;
			for (const auto& option : options) {
				files.push_back(option);
			}

			auto custom = getCustomReaction();
			if (custom.isErr()) {
				return Err("Could not get custom reactions: {}", custom.unwrapErr());
			}
			files.push_back(custom.unwrap());

			return Ok(files[randint(0, files.size() - 1)]);
		} else if (reaction == "Kenos (Npesta)") {
			file = options[0];
		} else if (reaction == "Bloodbath (Riot)") {
			file = options[1];
		} else if (reaction == "Bloodlust (Knobbelboy)") {
			file = options[2];
		} else if (reaction == "Nhelv (Kingsammelot)") {
			file = options[3];
		} else if (reaction == "Thinking Space II (Zoink)") {
			file = options[4];
		} else if (reaction == "Slaugherhouse (SpaceUK's \"completion\")") {
			file = options[5];
		} else if (reaction == "Flamewall (Cuatrocientos)") {
			file = options[6];
		} else if (reaction == "Silent Clubstep (Doggie)") {
			file = options[7];
		} else if (reaction == "Unnerfed Sary Never Clear (Glow)") {
			file = options[8];
		} else if (reaction == "Rupture (Cold)") {
			file = options[9];
		} else if (reaction == "Unnerfed Zodiac (nebnoob)") {
			file = options[10];
		} else if (reaction == "Orbit (Zoink)") {
			file = options[11];
		} else if (reaction == "Artificial Ascent (Kingsammelot)") {
			file = options[12];
		} else if (reaction == "Deimos (Npesta)") {
			file = options[13];
		} else if (reaction == "Tartarus (AeonAir)") {
			file = options[14];
		} else if (reaction == "WOW (Npesta)") {
			file = options[15];
		} else if (reaction == "Killbot (Kingsammelot)") {
			file = options[16];
		} else {
			log::error("Please report this issue to the OMG! developer");
			return Err("Unknown reaction: {} (THIS SHOULD BE UNREACHABLE)", reaction); // SHOULD BE UNREACHABLE
		}

		return Ok(Mod::get()->getResourcesDir() / file);
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