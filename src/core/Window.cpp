#include "Window.hpp"

#include "imgui.h"
#include "imgui-SFML.h"
#include "imgui_internal.h"

#include "Application.hpp"

namespace ts
{

Window::Window()
    : window_(sf::VideoMode(1920, 1080), "Traffic Simulator", sf::Style::Default, sf::ContextSettings(0, 0, 8)),
      view_(sf::View(sf::FloatRect(0, 0, sf::VideoMode::getDesktopMode().width, sf::VideoMode::getDesktopMode().height)))
{
    window_.setView(view_);
    window_.setVerticalSyncEnabled(true);
    ImGui::SFML::Init(window_);
    window_.resetGLStates();
    ImGui::SFML::Update(window_, clock_.restart());
    ImGui::SFML::Render(window_);
    ImGui::GetFont()->Scale = 2.0f;
}

bool Window::isGuiHovered() const
{
    return ImGui::GetIO().WantCaptureMouse;
}

void Window::clear()
{
    ImGui::SFML::Update(window_, clock_.restart());
    window_.clear();
}

void Window::display()
{
    // Show accident counter
    ImGui::SetNextWindowPos(ImVec2(window_.getSize().x - 180, window_.getSize().y - 50), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(180, 40), ImGuiCond_Always);
    ImGui::Begin("Accident Counter", nullptr, 
        ImGuiWindowFlags_NoTitleBar | 
        ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoMove | 
        ImGuiWindowFlags_NoScrollbar | 
        ImGuiWindowFlags_NoCollapse | 
        ImGuiWindowFlags_NoBackground);
    
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Text("Car Accidents: %d", Car::getAccidentCount());
    ImGui::End();

    ImGui::SFML::Render(window_);
    window_.display();
}

void Window::pollEvent()
{
    auto app = Application::GetInstance();
    sf::Event ev;
    while (window_.pollEvent(ev))
    {
        ImGui::SFML::ProcessEvent(ev);
        if (ev.type == sf::Event::Closed)
            window_.close();
        else if (ev.type == sf::Event::MouseWheelScrolled)
        {
            zoomView(sf::Vector2i(ev.mouseWheelScroll.x, ev.mouseWheelScroll.y), ev.mouseWheelScroll.delta);
        }
        app->handleEvent(ev);
    }
}

void Window::setViewPos(const sf::Vector2f &pos)
{
    view_.setCenter(pos);
    window_.setView(view_);
}

void Window::moveView(const sf::Vector2i &delta_pos)
{
    if (isGuiHovered())
        return;
    view_.move(delta_pos.x * zoom_, delta_pos.y * zoom_);
    window_.setView(view_);
}

void Window::setZoom(float zoom)
{
    zoom_ = zoom;
    view_.setSize(window_.getSize().x * zoom_, window_.getSize().y * zoom_);
    window_.setView(view_);
}


void Window::zoomView(const sf::Vector2i &relative_to, float zoom_dir)
{
    if (zoom_dir == 0 || isGuiHovered())
        return;
    const sf::Vector2f beforeCoord{window_.mapPixelToCoords(relative_to)};
    const float zoomfactor = 1.1f;
    float old_zoom = zoom_;
    zoom_ = zoom_ * (zoom_dir < 0 ? zoomfactor : 1.f / zoomfactor);
    // Max zoom
    if(zoom_ < 0.5 || zoom_ > 5)
    {
        zoom_ = old_zoom;
        return;
    }
    view_.setSize(window_.getSize().x * zoom_, window_.getSize().y * zoom_);
    window_.setView(view_);
    const sf::Vector2f afterCoord{window_.mapPixelToCoords(relative_to)};
    const sf::Vector2f offsetCoords{beforeCoord - afterCoord};
    view_.move(offsetCoords);
    window_.setView(view_);
}

} // namespace ts
