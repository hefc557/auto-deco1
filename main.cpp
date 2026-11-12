#include <Geode/Geode.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>
#include <algorithm>
#include <functional>
#include <random>
#include <set>
#include <sstream>
#include <vector>

using namespace geode::prelude;

// Color channels used for themed decorations (high numbers to avoid clashing with yours).
static constexpr int CH_A = 990;
static constexpr int CH_B = 991;

struct Theme {
	const char* name;
	ccColor3B a;       // main deco color
	ccColor3B b;       // secondary deco color
	ccColor3B bg;      // background color
	ccColor3B ground;  // ground color
	std::vector<int> ids;
};

static std::vector<Theme> const& themes() {
	static std::vector<Theme> t = {
		{ "Hell",   ccc3(220, 20, 20),   ccc3(110, 110, 110), ccc3(40, 0, 0),    ccc3(70, 15, 15),   { 8, 39, 392, 88 } },
		{ "Heaven", ccc3(255, 255, 255), ccc3(255, 220, 100), ccc3(150, 210, 255), ccc3(240, 240, 255), { 39, 392, 36, 98 } },
		{ "Water",  ccc3(0, 200, 255),   ccc3(0, 90, 200),    ccc3(0, 40, 90),   ccc3(0, 60, 110),   { 84, 67, 98, 392 } },
		{ "Forest", ccc3(40, 180, 60),   ccc3(120, 80, 40),   ccc3(10, 50, 20),  ccc3(30, 70, 30),   { 39, 392, 103, 89 } },
		{ "Space",  ccc3(150, 60, 255),  ccc3(255, 255, 255), ccc3(10, 0, 30),   ccc3(25, 10, 60),   { 141, 36, 98, 88 } },
		{ "Neon",   ccc3(255, 0, 200),   ccc3(0, 255, 255),   ccc3(10, 10, 20),  ccc3(20, 20, 40),   { 8, 392, 140, 84 } },
		{ "Candy",  ccc3(255, 120, 190), ccc3(120, 255, 200), ccc3(255, 220, 240), ccc3(255, 190, 220), { 36, 141, 98, 39 } },
		{ "Sunset", ccc3(255, 140, 0),   ccc3(255, 210, 60),  ccc3(120, 40, 20), ccc3(150, 60, 30),  { 8, 103, 98, 392 } },
		{ "Ice",    ccc3(180, 230, 255), ccc3(255, 255, 255), ccc3(20, 50, 90),  ccc3(60, 100, 150), { 39, 392, 103, 67 } },
		{ "Toxic",  ccc3(130, 255, 0),   ccc3(120, 0, 160),   ccc3(20, 30, 0),   ccc3(40, 50, 10),   { 8, 392, 88, 89 } },
	};
	return t;
}

static std::vector<int> parseIds(std::string const& str) {
	std::vector<int> out;
	std::stringstream ss(str);
	std::string tok;
	while (std::getline(ss, tok, ',')) {
		try {
			int id = std::stoi(tok);
			if (id > 0) out.push_back(id);
		} catch (...) {}
	}
	return out;
}

static void placeColorTrigger(LevelEditorLayer* lel, int channel, ccColor3B col, float y) {
	auto o = lel->createObject(899, { 15.f, y }, true); // 899 = Color trigger
	if (!o) return;
	auto t = static_cast<EffectGameObject*>(o);
	t->m_targetColorID = channel;
	t->m_triggerTargetColor = col;
	t->m_duration = 0.f;
	t->m_opacity = 1.f;
}

static ccColor3B mixColor(ccColor3B x, float fx, ccColor3B y, float fy) {
	auto c = [&](GLubyte p, GLubyte q) { return (GLubyte)std::min(255.f, p * fx + q * fy); };
	return ccc3(c(x.r, y.r), c(x.g, y.g), c(x.b, y.b));
}

// Theme picker: dark panel, themed cards with color swatches, quick toggles.
class AutoDecoPanel : public CCLayer {
protected:
	std::function<void(int)> m_onPick;

	bool init(std::function<void(int)> cb) {
		if (!CCLayer::init()) return false;
		m_onPick = std::move(cb);
		this->setTouchMode(kCCTouchesOneByOne);
		this->setTouchEnabled(true);
		this->setKeypadEnabled(true);

		auto win = CCDirector::get()->getWinSize();

		// Dim background
		auto dim = CCLayerColor::create({ 0, 0, 0, 0 }, win.width, win.height);
		dim->setOpacity(0);
		dim->runAction(CCFadeTo::create(0.2f, 170));
		this->addChild(dim);

		// Everything else lives in a container centered on screen (so it can pop in)
		auto box = CCNode::create();
		box->setPosition(win / 2);
		box->setScale(0.8f);
		box->runAction(CCEaseBackOut::create(CCScaleTo::create(0.28f, 1.f)));
		this->addChild(box);

		auto bg = CCScale9Sprite::create("square02b_001.png");
		bg->setContentSize({ 450.f, 290.f });
		bg->setColor(ccc3(18, 18, 26));
		bg->setOpacity(245);
		box->addChild(bg);

		auto frame = CCScale9Sprite::create("square02b_001.png");
		frame->setContentSize({ 458.f, 298.f });
		frame->setColor(ccc3(255, 190, 60));
		frame->setOpacity(70);
		box->addChild(frame, -1);

		auto title = CCLabelBMFont::create("AUTO DECO", "goldFont.fnt");
		title->setScale(0.8f);
		title->setPosition({ 0.f, 125.f });
		box->addChild(title);

		auto sub = CCLabelBMFont::create("Choose a style for your level", "bigFont.fnt");
		sub->setScale(0.3f);
		sub->setColor(ccc3(170, 170, 190));
		sub->setPosition({ 0.f, 106.f });
		box->addChild(sub);

		auto menu = CCMenu::create();
		menu->setTouchPriority(-501); // above our touch-swallowing layer (-500)
		menu->setPosition({ 0.f, 0.f });
		box->addChild(menu);

		// Cards: 10 themes + Random + Custom
		auto& list = themes();
		for (size_t i = 0; i < list.size(); i++) {
			auto& t = list[i];
			addCard(menu, box, t.name, mixColor(t.bg, 0.55f, t.a, 0.30f), t.a, t.b, (int)i, i);
		}
		addCard(menu, box, "Random", ccc3(60, 60, 80), ccc3(255, 255, 255), ccc3(255, 200, 60), -2, list.size());
		addCard(menu, box, "Custom", ccc3(50, 50, 60), ccc3(160, 160, 170), ccc3(100, 100, 110), -1, list.size() + 1);

		// Footer toggles
		addToggle(menu, box, "Recolor BG & Ground", -205.f, 1, Mod::get()->getSettingValue<bool>("recolor-scene"));
		addToggle(menu, box, "Random Size", -55.f, 2, Mod::get()->getSettingValue<bool>("random-scale"));

		auto xs = ButtonSprite::create("Close");
		xs->setScale(0.65f);
		auto xb = CCMenuItemSpriteExtra::create(xs, this, menu_selector(AutoDecoPanel::onClose));
		xb->setPosition({ 175.f, -122.f });
		menu->addChild(xb);

		return true;
	}

	void addCard(CCMenu* menu, CCNode* box, char const* name, ccColor3B base, ccColor3B a, ccColor3B b, int tag, size_t slot) {
		int col = (int)(slot % 4), row = (int)(slot / 4);

		auto card = CCScale9Sprite::create("square02b_001.png");
		card->setContentSize({ 96.f, 58.f });
		card->setColor(base);

		auto lbl = CCLabelBMFont::create(name, "bigFont.fnt");
		lbl->setScale(strlen(name) > 6 ? 0.4f : 0.5f);
		lbl->setPosition({ 48.f, 40.f });
		card->addChild(lbl);

		auto swatch = [&](ccColor3B c, float x) {
			auto s = CCScale9Sprite::create("square02b_001.png");
			s->setContentSize({ 28.f, 12.f });
			s->setColor(c);
			s->setPosition({ x, 14.f });
			card->addChild(s);
		};
		swatch(a, 31.f);
		swatch(b, 65.f);

		auto btn = CCMenuItemSpriteExtra::create(card, this, menu_selector(AutoDecoPanel::onPick));
		btn->setTag(tag);
		btn->setPosition({ (col - 1.5f) * 106.f, 66.f - row * 66.f });
		menu->addChild(btn);
	}

	void addToggle(CCMenu* menu, CCNode* box, char const* text, float x, int tag, bool on) {
		auto t = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(AutoDecoPanel::onToggle), 0.55f);
		t->setTag(tag);
		t->toggle(on);
		t->setPosition({ x, -122.f });
		menu->addChild(t);

		auto l = CCLabelBMFont::create(text, "bigFont.fnt");
		l->setScale(0.32f);
		l->setAnchorPoint({ 0.f, 0.5f });
		l->setPosition({ x + 15.f, -122.f });
		box->addChild(l);
	}

	void onToggle(CCObject* sender) {
		auto t = static_cast<CCMenuItemToggler*>(sender);
		bool value = !t->isToggled(); // GD reports the state from before the click
		Mod::get()->setSettingValue<bool>(t->getTag() == 1 ? "recolor-scene" : "random-scale", value);
	}

	void onPick(CCObject* sender) {
		int idx = static_cast<CCNode*>(sender)->getTag();
		Ref<AutoDecoPanel> keep = this;
		auto cb = m_onPick;
		this->removeFromParent();
		if (cb) cb(idx);
	}

	void onClose(CCObject*) { this->removeFromParent(); }
	void keyBackClicked() override { this->removeFromParent(); }

	// Swallow every touch that isn't on our menu so the pause menu underneath can't be clicked.
	void registerWithTouchDispatcher() override {
		CCDirector::get()->getTouchDispatcher()->addTargetedDelegate(this, -500, true);
	}
	bool ccTouchBegan(CCTouch*, CCEvent*) override { return true; }

public:
	static AutoDecoPanel* create(std::function<void(int)> cb) {
		auto ret = new AutoDecoPanel();
		if (ret && ret->init(std::move(cb))) {
			ret->autorelease();
			return ret;
		}
		CC_SAFE_DELETE(ret);
		return nullptr;
	}
};

class $modify(AutoDecoPause, EditorPauseLayer) {
	bool init(LevelEditorLayer* lel) {
		if (!EditorPauseLayer::init(lel)) return false;

		auto menu = CCMenu::create();
		auto spr = ButtonSprite::create("Auto Deco");
		auto btn = CCMenuItemSpriteExtra::create(
			spr, this, menu_selector(AutoDecoPause::onOpenPanel)
		);
		menu->addChild(btn);
		auto win = CCDirector::get()->getWinSize();
		menu->setPosition({ win.width / 2.f, win.height - 38.f });
		this->addChild(menu);
		return true;
	}

	void onOpenPanel(CCObject*) {
		if (this->getChildByID("auto-deco-panel")) return;
		auto panel = AutoDecoPanel::create([this](int idx) { this->handlePick(idx); });
		if (!panel) return;
		panel->setID("auto-deco-panel");
		this->addChild(panel, 100);
	}

	void handlePick(int idx) {
		auto& list = themes();
		if (idx == -2) idx = (int)(std::rand() % list.size()); // Random
		Theme const* theme = (idx >= 0 && idx < (int)list.size()) ? &list[idx] : nullptr; // -1 = Custom
		runDeco(theme);
	}

	void runDeco(Theme const* theme) {
		auto lel = m_editorLayer;
		if (!lel || !lel->m_objects) return;

		auto mod = Mod::get();
		std::vector<int> ids = theme ? theme->ids : parseIds(mod->getSettingValue<std::string>("deco-ids"));
		if (ids.empty()) {
			FLAlertLayer::create("Auto Deco", "No valid object IDs in settings.", "OK")->show();
			return;
		}
		int density = static_cast<int>(mod->getSettingValue<int64_t>("density"));
		bool top = mod->getSettingValue<bool>("decorate-top");
		bool sides = mod->getSettingValue<bool>("decorate-sides");
		bool bottom = mod->getSettingValue<bool>("decorate-bottom");
		bool noTouch = mod->getSettingValue<bool>("no-touch");
		bool recolor = mod->getSettingValue<bool>("recolor-scene");
		bool randScale = mod->getSettingValue<bool>("random-scale");
		size_t cap = static_cast<size_t>(mod->getSettingValue<int64_t>("max-objects"));

		// Collect solid block positions on a 30-unit grid.
		std::set<std::pair<int, int>> grid;
		std::vector<std::pair<int, int>> blocks;
		for (auto obj : CCArrayExt<GameObject*>(lel->m_objects)) {
			if (!obj || obj->m_objectType != GameObjectType::Solid) continue;
			auto p = obj->getPosition();
			std::pair<int, int> g{ (int)std::floor(p.x / 30.f), (int)std::floor(p.y / 30.f) };
			if (grid.insert(g).second) blocks.push_back(g);
		}

		struct Side { int dx, dy; float rot; bool enabled; };
		Side dirs[4] = {
			{ 0,  1,   0.f, top },
			{ 1,  0,  90.f, sides },
			{ 0, -1, 180.f, bottom },
			{-1,  0, 270.f, sides },
		};

		std::mt19937 rng(std::random_device{}());
		std::uniform_int_distribution<int> pct(1, 100);
		std::uniform_int_distribution<size_t> pick(0, ids.size() - 1);
		std::uniform_real_distribution<float> scl(0.7f, 1.2f);

		size_t placed = 0;
		for (auto const& b : blocks) {
			for (auto const& d : dirs) {
				if (!d.enabled || placed >= cap) continue;
				if (grid.count({ b.first + d.dx, b.second + d.dy })) continue; // covered
				if (d.dy == -1 && b.second == 0) continue;                     // on the ground
				if (pct(rng) > density) continue;

				float cx = b.first * 30.f + 15.f + d.dx * 22.f;
				float cy = b.second * 30.f + 15.f + d.dy * 22.f;

				auto o = lel->createObject(ids[pick(rng)], { cx, cy }, true);
				if (!o) continue;
				o->setRotation(d.rot);
				if (noTouch) o->m_isNoTouch = true;

				if (theme) {
					int ch = (pct(rng) <= 50) ? CH_A : CH_B;
					if (o->m_baseColor) o->m_baseColor->m_colorID = ch;
					if (o->m_detailColor) o->m_detailColor->m_colorID = ch;
					if (randScale) o->setScale(scl(rng));
				}
				placed++;
			}
		}

		// Color triggers so the theme's colors actually show up.
		if (theme) {
			placeColorTrigger(lel, CH_A, theme->a, 330.f);
			placeColorTrigger(lel, CH_B, theme->b, 360.f);
			if (recolor) {
				placeColorTrigger(lel, 1000, theme->bg, 390.f);      // background
				placeColorTrigger(lel, 1001, theme->ground, 420.f);  // ground
				placeColorTrigger(lel, 1009, theme->ground, 450.f);  // ground 2
			}
		}

		FLAlertLayer::create(
			"Auto Deco",
			fmt::format(
				"{} theme: placed {} decorations.\nUse the editor's Undo to revert.",
				theme ? theme->name : "Custom", placed
			),
			"OK"
		)->show();
	}
};
