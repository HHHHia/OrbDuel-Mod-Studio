// 主菜单实现：布局 → 命中检测 → 绘制，全部集中在这里。
// 布局只有一个来源 layoutFor()，update() 与 draw() 共用，避免"看到的位置"和"能点的位置"不一致。

#include "orbduel/game/Menu.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "BodyShape.hpp"
#include "Names.hpp"

namespace orbduel {
namespace {

constexpr Rgba kBackground{10, 11, 16, 255};
constexpr Rgba kPanel{22, 25, 36, 235};
constexpr Rgba kPanelEdge{70, 85, 130, 255};
constexpr Rgba kText{235, 240, 255, 255};
constexpr Rgba kDim{150, 160, 190, 255};
constexpr Rgba kAccentLeft{255, 130, 60, 255};
constexpr Rgba kAccentRight{90, 160, 255, 255};
constexpr Rgba kButtonIdle{34, 39, 56, 255};
constexpr Rgba kButtonHot{56, 66, 96, 255};
constexpr Rgba kConfirmMask{6, 7, 12, 200};

bool inside(Rect rect, Vec2 point) {
    return point.x >= rect.x && point.x <= rect.right() &&
           point.y >= rect.y && point.y <= rect.bottom();
}

struct Layout {
    Rect leftCard;
    Rect rightCard;
    Rect leftPrev;
    Rect leftNext;
    Rect rightPrev;
    Rect rightNext;
    Rect startButton;
    Rect quitButton;
    Rect dialog;
    Rect yesButton;
    Rect noButton;
    Vec2 leftAvatar;
    Vec2 rightAvatar;
    float avatarRadius = 54.0f;
};

Layout layoutFor(float width, float height) {
    Layout l;
    const float cardWidth = 380.0f;
    const float cardHeight = 360.0f;
    const float gap = 48.0f;
    const float cardsY = 150.0f;

    l.leftCard = Rect{width * 0.5f - gap * 0.5f - cardWidth, cardsY, cardWidth, cardHeight};
    l.rightCard = Rect{width * 0.5f + gap * 0.5f, cardsY, cardWidth, cardHeight};

    const float arrowWidth = 40.0f;
    const float arrowHeight = 34.0f;
    const float arrowY = cardsY + cardHeight - 54.0f;
    l.leftPrev = Rect{l.leftCard.x + 24.0f, arrowY, arrowWidth, arrowHeight};
    l.leftNext = Rect{l.leftCard.right() - 24.0f - arrowWidth, arrowY, arrowWidth, arrowHeight};
    l.rightPrev = Rect{l.rightCard.x + 24.0f, arrowY, arrowWidth, arrowHeight};
    l.rightNext = Rect{l.rightCard.right() - 24.0f - arrowWidth, arrowY, arrowWidth, arrowHeight};

    const float buttonWidth = 260.0f;
    const float buttonHeight = 46.0f;
    const float startY = cardsY + cardHeight + 26.0f;
    l.startButton = Rect{width * 0.5f - buttonWidth * 0.5f, startY, buttonWidth, buttonHeight};
    l.quitButton = Rect{width * 0.5f - buttonWidth * 0.5f, startY + buttonHeight + 14.0f,
                        buttonWidth, buttonHeight};

    const float dialogWidth = 420.0f;
    const float dialogHeight = 180.0f;
    l.dialog = Rect{width * 0.5f - dialogWidth * 0.5f, height * 0.5f - dialogHeight * 0.5f,
                    dialogWidth, dialogHeight};
    const float choiceWidth = 120.0f;
    const float choiceHeight = 44.0f;
    l.yesButton = Rect{l.dialog.x + 40.0f, l.dialog.bottom() - choiceHeight - 28.0f,
                       choiceWidth, choiceHeight};
    l.noButton = Rect{l.dialog.right() - 40.0f - choiceWidth,
                      l.dialog.bottom() - choiceHeight - 28.0f, choiceWidth, choiceHeight};

    l.leftAvatar = Vec2{l.leftCard.center().x, l.leftCard.y + 92.0f};
    l.rightAvatar = Vec2{l.rightCard.center().x, l.rightCard.y + 92.0f};
    return l;
}

void drawCentered(Renderer& renderer, const std::string& text, float centerX, float y,
                  int fontSize, Rgba color) {
    const int width = renderer.measureText(text, fontSize);
    renderer.drawText(text, Vec2{centerX - static_cast<float>(width) * 0.5f, y}, fontSize, color);
}

void drawButton(Renderer& renderer, Rect box, const std::string& label, Rgba textColor, bool hot) {
    renderer.drawRect(box, hot ? kButtonHot : kButtonIdle);
    renderer.drawRectLines(box, 1.0f, kPanelEdge);
    const int width = renderer.measureText(label, 16);
    renderer.drawText(label,
                      Vec2{box.center().x - static_cast<float>(width) * 0.5f, box.center().y - 8.0f},
                      16, textColor);
}

std::string numberText(float value, int decimals) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, static_cast<double>(value));
    return std::string(buffer);
}

std::string skillLine(const ContentPack& pack, const CharacterDef& def, bool cjk) {
    std::string line;
    for (std::size_t i = 0; i < def.skills.size(); ++i) {
        const SkillDef* skill = pack.findSkill(def.skills[i]);
        std::string label = def.skills[i];
        if (skill != nullptr) {
            label = skillLabel(*skill, cjk);
        }
        if (i != 0) {
            line += ", ";
        }
        line += label;
    }
    if (line.empty()) {
        line = "no skill";
    }
    return line;
}

void drawCharacterCard(Renderer& renderer, AssetCache& assets, const ContentPack& pack,
                       const CharacterDef& def, Rect card, Vec2 avatar, float avatarRadius,
                       Rgba accent, const std::string& contentRoot, const std::string& slot,
                       bool chineseLabels) {
    renderer.drawRect(card, kPanel);
    renderer.drawRectLines(card, 2.0f, accent);

    drawCentered(renderer, slot, card.center().x, card.y + 16.0f, 8, accent);

    // 程序化皮肤（三角形等）不读贴图；菜单里朝向固定朝右
    if (!drawShapeSkin(renderer, def.skin, avatar, avatarRadius, 0.0f, def.skin.tint)) {
        const std::string path = contentRoot + "/" + def.sourceDir + "/" + def.skin.texture;
        SDL_Texture* texture = assets.get(path);
        if (texture != nullptr) {
            int textureWidth = 1;
            int textureHeight = 1;
            SDL_QueryTexture(texture, nullptr, nullptr, &textureWidth, &textureHeight);
            const float scale = (avatarRadius * 2.0f) / static_cast<float>(std::max(1, textureWidth));
            renderer.drawSprite(texture, avatar, 0.0f, scale, def.skin.tint);
        } else {
            renderer.drawCircle(avatar, avatarRadius, accent);
        }
    }

    const bool cjk = chineseLabels && renderer.hasFallbackFont();
    drawCentered(renderer, characterLabel(def, cjk), card.center().x, avatar.y + avatarRadius + 16.0f,
                 16, kText);

    const float statY = card.y + 226.0f;
    const float statX = card.x + 28.0f;
    renderer.drawText("HP    " + numberText(def.stats.maxHp, 0), Vec2{statX, statY}, 8, kDim);
    renderer.drawText("MASS  " + numberText(def.stats.mass, 2), Vec2{statX, statY + 20.0f}, 8, kDim);
    renderer.drawText("SPEED " + numberText(def.stats.startSpeed, 0), Vec2{statX, statY + 40.0f}, 8,
                      kDim);
    // 中文技能名在 8 号像素字下看不清，单独放大一档
    renderer.drawText("SKILL " + skillLine(pack, def, cjk), Vec2{statX, statY + 62.0f},
                      cjk ? 12 : 8, kDim);
}

} // namespace

void Menu::open(const ContentPack& pack, const std::string& leftId, const std::string& rightId) {
    pack_ = &pack;
    leftIndex_ = 0;
    rightIndex_ = 0;
    confirmOpen_ = false;
    startRequested_ = false;
    quitRequested_ = false;

    const int count = static_cast<int>(pack.characters.size());
    if (count == 0) {
        return;
    }
    rightIndex_ = count > 1 ? 1 : 0;

    for (int i = 0; i < count; ++i) {
        if (pack.characters[i].id == leftId) {
            leftIndex_ = i;
        }
        if (pack.characters[i].id == rightId) {
            rightIndex_ = i;
        }
    }
}

void Menu::update(Renderer& renderer) {
    if (pack_ == nullptr || pack_->characters.empty()) {
        return;
    }

    const Layout layout =
        layoutFor(static_cast<float>(renderer.width()), static_cast<float>(renderer.height()));
    const Vec2 mouse = renderer.mousePosition();
    const bool clicked = renderer.wasMouseClicked();
    const int count = static_cast<int>(pack_->characters.size());

    if (confirmOpen_) {
        if (renderer.wasKeyPressed(SDL_SCANCODE_ESCAPE)) {
            confirmOpen_ = false;
            return;
        }
        if (!clicked) {
            return;
        }
        if (inside(layout.yesButton, mouse)) {
            quitRequested_ = true;
            return;
        }
        if (inside(layout.noButton, mouse) || !inside(layout.dialog, mouse)) {
            confirmOpen_ = false;
        }
        return;
    }

    if (renderer.wasKeyPressed(SDL_SCANCODE_RETURN) ||
        renderer.wasKeyPressed(SDL_SCANCODE_KP_ENTER)) {
        startRequested_ = true;
        return;
    }
    if (!clicked) {
        return;
    }

    if (inside(layout.leftPrev, mouse)) {
        leftIndex_ = (leftIndex_ + count - 1) % count;
        return;
    }
    if (inside(layout.leftNext, mouse)) {
        leftIndex_ = (leftIndex_ + 1) % count;
        return;
    }
    if (inside(layout.rightPrev, mouse)) {
        rightIndex_ = (rightIndex_ + count - 1) % count;
        return;
    }
    if (inside(layout.rightNext, mouse)) {
        rightIndex_ = (rightIndex_ + 1) % count;
        return;
    }
    if (inside(layout.startButton, mouse)) {
        startRequested_ = true;
        return;
    }
    if (inside(layout.quitButton, mouse)) {
        confirmOpen_ = true;
    }
}

void Menu::draw(Renderer& renderer, AssetCache& assets, const std::string& contentRoot,
                bool chineseLabels) const {
    if (pack_ == nullptr || pack_->characters.empty()) {
        return;
    }

    const float width = static_cast<float>(renderer.width());
    const float height = static_cast<float>(renderer.height());
    const Layout layout = layoutFor(width, height);
    const Vec2 mouse = renderer.mousePosition();

    drawCentered(renderer, "ORBDUEL", width * 0.5f, 44.0f, 24, kText);
    drawCentered(renderer, "TWO ORBS - ONE ARENA", width * 0.5f, 92.0f, 8, kDim);

    const CharacterDef& left = pack_->characters[leftIndex_];
    const CharacterDef& right = pack_->characters[rightIndex_];
    drawCharacterCard(renderer, assets, *pack_, left, layout.leftCard, layout.leftAvatar,
                      layout.avatarRadius, kAccentLeft, contentRoot, "PLAYER", chineseLabels);
    drawCharacterCard(renderer, assets, *pack_, right, layout.rightCard, layout.rightAvatar,
                      layout.avatarRadius, kAccentRight, contentRoot, "ENEMY", chineseLabels);

    drawButton(renderer, layout.leftPrev, "<", kText, inside(layout.leftPrev, mouse));
    drawButton(renderer, layout.leftNext, ">", kText, inside(layout.leftNext, mouse));
    drawButton(renderer, layout.rightPrev, "<", kText, inside(layout.rightPrev, mouse));
    drawButton(renderer, layout.rightNext, ">", kText, inside(layout.rightNext, mouse));

    drawButton(renderer, layout.startButton, "START GAME", kText,
               inside(layout.startButton, mouse));
    drawButton(renderer, layout.quitButton, "QUIT GAME", kDim, inside(layout.quitButton, mouse));

    if (confirmOpen_) {
        renderer.drawRect(Rect{0.0f, 0.0f, width, height}, kConfirmMask);
        renderer.drawRect(layout.dialog, kPanel);
        renderer.drawRectLines(layout.dialog, 2.0f, kPanelEdge);
        drawCentered(renderer, "QUIT GAME?", layout.dialog.center().x, layout.dialog.y + 34.0f, 16,
                     kText);
        drawButton(renderer, layout.yesButton, "YES", kText, inside(layout.yesButton, mouse));
        drawButton(renderer, layout.noButton, "NO", kText, inside(layout.noButton, mouse));
    }
}

Rgba Menu::background() const { return kBackground; }

bool Menu::wantsStart() const { return startRequested_; }
bool Menu::wantsQuit() const { return quitRequested_; }

void Menu::clearRequests() {
    startRequested_ = false;
    quitRequested_ = false;
}

const std::string& Menu::leftId() const {
    static const std::string empty;
    if (pack_ == nullptr || pack_->characters.empty()) {
        return empty;
    }
    return pack_->characters[leftIndex_].id;
}

const std::string& Menu::rightId() const {
    static const std::string empty;
    if (pack_ == nullptr || pack_->characters.empty()) {
        return empty;
    }
    return pack_->characters[rightIndex_].id;
}

} // namespace orbduel
