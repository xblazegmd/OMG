#include <Geode/Geode.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/utils/random.hpp>
#include <Geode/utils/string.hpp>
#include <Geode/utils/StringMap.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <asp/fs.hpp>
#include <string>

using namespace geode::prelude;

class $modify(PLHook, PlayLayer) {
    struct Fields {
        utils::StringMap<std::string> m_files;
        std::vector<const std::string*> m_filesVector; // For easier/faster lookups by index
    };

    bool init (GJGameLevel* level, bool dontCreateObjects) {
        if (!PlayLayer::init(level, dontCreateObjects)) return false;

        m_fields->m_files = {
			{"Kenos (Npesta)", 								"npesta-kenos.ogg"},
			{"Bloodbath (Riot)", 							"riot-bloodbath.ogg"},
			{"Bloodlust (Knobbelboy)", 						"knobbelboy-bloodlust.ogg"},
			{"Nhelv (Kingsammelot)", 						"kingsammelot-nhelv.ogg"},
			{"Thinking Space II (Zoink)", 					"zoink-ts2.ogg"},
			{"Slaugherhouse (SpaceUK's \"completion\")", 	getNormalOrSwear("spaceuk", "swearuk", "slaughterhouse")},
            {"Flamewall (Cuatrocientos)",                   "cuatrocientos-flamewall.ogg"},
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
			{"Boobawamba (Zoink)", 							getNormalOrSwear("zoink", "swoink", "boobawamba")},
			{"Thinking Space II (Wpopoff)",					"wpopoff-ts2.ogg"},
			{"Zodiac (Zoink)",								"zoink-zodiac.ogg"},
			{"Slaugherhouse (RaeveZ)", 						"raevez-slaughterhouse.ogg"},
            {"Grief (Doggie)",                              "doggie-grief.ogg"}
		};
        m_fields->m_filesVector.reserve(m_fields->m_files.size());
        for (const auto& [_, reaction] : m_fields->m_files) {
            m_fields->m_filesVector.push_back(&reaction);
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

		auto reaction = this->getReactionPath();
		if (reaction.isErr()) {
			log::error("{}", reaction.unwrapErr());
			return;
		}

		auto audioEngine = FMODAudioEngine::get();
		audioEngine->m_globalChannel->setPaused(false);
		audioEngine->playEffectAdvanced(
			std::move(reaction).unwrap().string(),
			1.f,
			0, // idk what this is (I don't think anyone knows)
			mod->getSettingValue<int64_t>("volume") / 100.f, // volume
			1.f, false, false, 0, 0, 0, 0, false, 0, false, // stuff we don't care abt
			true, // this should remove lag (I think)
			0, 0, 0, 0 // we also don't care abt this
		);
	}

	Result<asp::fs::Path> getReactionPath() {
		auto resources = Mod::get()->getResourcesDir();
        auto f = m_fields.self();

		std::string reaction = Mod::get()->getSettingValue<std::string>("reaction");
		if (reaction == "Random") {
			// BUT IS IT POSSIBLE WITH ACCURATE HITBOXES???????
			if (random::chance(0.001))
				return Ok(resources / "accurate-hitboxes.ogg");

            int idx = random::generate(0, f->m_filesVector.size());
            return Ok(resources / *f->m_filesVector[idx]);
		} else if (reaction == "Random (With Custom)") {
			if (random::chance(0.001))
				return Ok(resources / "accurate-hitboxes.ogg");

            auto idx = random::generate(0, f->m_filesVector.size() + 1);
            if (idx == f->m_filesVector.size()) {
                return getCustomReaction();
            }

            return Ok(resources / *f->m_filesVector[idx]);
		} else if (reaction == "Custom") {
			return getCustomReaction();
		} else if (auto it = f->m_files.find(reaction); it != f->m_files.end()) {
			return Ok(resources / it->second);
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

	Result<asp::fs::Path> getCustomReaction() const {
		auto reaction = Mod::get()->getSettingValue<asp::fs::Path>("custom-reaction");
        if (!reaction.exists()) {
			return Err("The custom reaction's file was not found");
        }
		return Ok(reaction);
	}
};
