#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/utils/string.hpp>
#include <Geode/fmod/fmod.h>
#include <Geode/fmod/fmod_errors.h>
#include <filesystem>
#include <random>
#include <string>
#include <system_error>
#include <vector>

using namespace geode::prelude;

$execute {
	if (!std::filesystem::exists(Mod::get()->getConfigDir() / "reactions")) {
		(void)file::createDirectory(Mod::get()->getConfigDir() / "reactions");
	}
}

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
		if (!mod->getSettingValue<bool>("quick-enable")) return;
		if (mod->getSettingValue<bool>("no-practice") && m_isPracticeMode) return;
		if (mod->getSettingValue<bool>("no-startpos") && m_isTestMode) return;
		if (mod->getSettingValue<bool>("no-normal") && !m_isPracticeMode && !m_isTestMode) return;
		if (mod->getSettingValue<bool>("no-platformer") && m_isPlatformer) return;

		auto soundRes = getSound();
		if (soundRes.isErr()) {
			log::error("{}", soundRes.unwrapErr());
			return;
		}
		FMOD::Sound* sound = soundRes.unwrap();
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
			"npesta-wow.mp3"
		};

		std::string file;

		// Wonderful if-else chain ;-;
		std::string reaction = Mod::get()->getSettingValue<std::string>("reaction");
		if (reaction == "Random") {
			file = options[randint(0, options.size() - 1)];
		} else if (reaction == "Custom") {
			auto customReactions = getCustomReactions();
			if (customReactions.isErr()) {
				return Err("Could not get custom reactions: {}", customReactions.unwrapErr());
			} else {
				auto customs = customReactions.unwrap();
				return Ok(customs[randint(0, customs.size() - 1)]);
			}
		} else if (reaction == "Random (With Customs)") {
			std::vector<std::filesystem::path> files;
			for (const auto& option : options) {
				files.push_back(option);
			}

			auto customReactions = getCustomReactions();
			if (customReactions.isErr()) {
				return Err("Could not get custom reactions: {}", customReactions.unwrapErr());
			} else {
				for (const auto& reaction : customReactions.unwrap()) {
					files.push_back(reaction);
				}
				return Ok(files[randint(0, files.size() - 1)]);
			}
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
		} else {
			log::error("Please report this issue to the OMG! developer");
			return Err("Unknown reaction: {} (THIS SHOULD BE UNREACHABLE)", reaction); // SHOULD BE UNREACHABLE
		}

		return Ok(Mod::get()->getResourcesDir() / file);
	}

	Result<std::vector<std::filesystem::path>> getCustomReactions() {
		auto reactionsPath = Mod::get()->getConfigDir() / "reactions";

		std::error_code err;
		if (!std::filesystem::exists(reactionsPath, err) || err) {
			return Err("There is no custom reactions");
		}
		if (!std::filesystem::is_directory(reactionsPath, err) || err) {
			return Err("Excuse me did you just make the reactions folder a file?");
		}
		if (std::filesystem::is_empty(reactionsPath, err) || err) {
			return Err("There is no custom reactions");
		}

		auto filesRes = file::readDirectory(reactionsPath);
		if (filesRes.isErr()) {
			// This should be unreachable, surely
			return Err("{}", filesRes.unwrapErr());
		}

		auto files = filesRes.unwrap();

		// Remove anything that isn't an audio file
		std::erase_if(files, [](const auto& file) {
			return !std::filesystem::is_regular_file(file) || (file.extension() != ".mp3" && file.extension() != ".ogg");
		});
		if (files.empty()) {
			return Err("There is no custom reactions");
		}
		return Ok(files);
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