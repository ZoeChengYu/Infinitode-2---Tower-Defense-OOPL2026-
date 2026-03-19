#include "App.hpp"

#include "Util/AtlasLoader.hpp"
#include "Util/Image.hpp"
#include "Util/Input.hpp"
#include "Util/Keycode.hpp"
#include "Util/Logger.hpp"

void App::Start() {
    LOG_TRACE("Start");

    // 建立圖片讀取物件。
    m_AtlasImage = std::make_shared<Util::AtlasLoader>(
        RESOURCE_DIR "/combined.atlas", RESOURCE_DIR "/combined.png");

    // 直接用名稱取出 atlas 裡的小圖。
    const std::shared_ptr<Util::Image> image =
        m_AtlasImage->Get("blueprint-SPECIAL_I");

    if (image != nullptr) {
        m_ImageObject->SetDrawable(image);
        m_ImageObject->m_Transform.translation = {0.0F, 0.0F};
        m_ImageObject->m_Transform.scale = {0.5F, 0.5F};
        m_Renderer.AddChild(m_ImageObject);
    }

    m_CurrentState = State::UPDATE;
}

void App::Update() {
    constexpr float speed = 5.0F;

    // WASD 控制圖片移動。
    if (Util::Input::IsKeyDown(Util::Keycode::A)) {
        m_ImageObject->m_Transform.translation.x -= speed;
    }
    if (Util::Input::IsKeyDown(Util::Keycode::D)) {
        m_ImageObject->m_Transform.translation.x += speed;
    }
    if (Util::Input::IsKeyDown(Util::Keycode::W)) {
        m_ImageObject->m_Transform.translation.y += speed;
    }
    if (Util::Input::IsKeyDown(Util::Keycode::S)) {
        m_ImageObject->m_Transform.translation.y -= speed;
    }

    m_Renderer.Update();

    // 保留最基本的離開方式，避免視窗無法正常關閉。
    if (Util::Input::IsKeyUp(Util::Keycode::ESCAPE) ||
        Util::Input::IfExit()) {
        m_CurrentState = State::END;
    }
}

void App::End() { // NOLINT(this method will mutate members in the future)
    LOG_TRACE("End");
}
