#include <Geode/Geode.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/utils/random.hpp>
#include <Geode/utils/string.hpp>
#include <Geode/utils/StringMap.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <filesystem>
#include <string>
#include <vector>

using namespace geode::prelude;

class $modify(PLHook, PlayLayer) {
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

		auto reaction = this->getReactionPath();
		if (reaction.isErr()) {
			log::error("{}", reaction.unwrapErr());
			return;
		}

		auto audioEngine = FMODAudioEngine::get();
		audioEngine->m_globalChannel->setPaused(false);
		audioEngine->playEffectAdvanced(
			string::pathToString(std::move(reaction).unwrap()),
			1.f,
			0, // idk what this is (I don't think anyone knows)
			mod->getSettingValue<int64_t>("volume") / 100.f, // volume
			1.f, false, false, 0, 0, 0, 0, false, 0, false, // stuff we don't care abt
			true, // this should remove lag (I think)
			0, 0, 0, 0 // we also don't care abt this
		);
	}

	Result<std::filesystem::path> getReactionPath() {
		auto resources = Mod::get()->getResourcesDir();
		auto files = getFiles();

		std::string reaction = Mod::get()->getSettingValue<std::string>("reaction");
		if (reaction == "Random") {
			auto it = files.begin();
			std::advance(it, random::generate(0, files.size()));
			return Ok(resources / it->second);
		} else if (reaction == "Random (With Custom)") {
			std::vector<std::filesystem::path> filePaths;
			filePaths.reserve(files.size() + 1);

			for (const auto& [_, file] : files) {
				filePaths.emplace_back(resources / file);
			}

			auto custom = getCustomReaction();
			if (custom.isErr()) {
				return Err("Could not get custom reactions: {}", custom.unwrapErr());
			}
			filePaths.push_back(std::move(custom).unwrap());

			return Ok(filePaths[random::generate(0, filePaths.size())]);
		} else if (reaction == "Custom") {
			return getCustomReaction();
		} else if (files.contains(reaction)) {
			return Ok(resources / files[reaction]);
		}

		// This should be unreachable so if we reach this, it's definitely a bug
		log::error("Please report this issue to the OMG! developer");
		return Err("Unknown reaction: {} (THIS SHOULD BE UNREACHABLE)", reaction);
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
			{"Thinking Space II (Zoink)", 					"zoink-ts2.ogg"},
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
			{"Killbot (Kingsammelot)", 						"kingsammelot-killbot.ogg"},
			{"Tidal Wave (Zoink)", 							getNormalOrSwear("zoink", "swoink", "tw")},
			{"Acheron (Zoink)",								getNormalOrSwear("zoink", "swoink", "acheron")},
			{"Unnerfed Sakupen Circles (totalgd)", 			"totalgd-uskc.ogg"},
			{"Time Extreme (Vortrox)", 						"vortrox-timeextreme.ogg"},
			{"Yatagarasu (Vortrox)", 						"vortrox-yata.ogg"},
			{"Boobawamba (Zoink)", 							"zoink-boobawamba.ogg"}
		};
	}

	Result<std::filesystem::path> getCustomReaction() const {
		auto reaction = Mod::get()->getSettingValue<std::filesystem::path>("custom-reaction");
		std::error_code ec;
		auto exists = std::filesystem::exists(reaction, ec);
		if (ec || !exists) {
			return Err("The custom reaction's file was not found");
		}
		return Ok(reaction);
	}
};