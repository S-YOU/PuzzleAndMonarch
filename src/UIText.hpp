﻿#pragma once

//
// テキスト描画Widget
//

#include "UIWidgetBase.hpp"


namespace ngs { namespace UI {

class Text
  : public WidgetBase
{
  std::string text_;
  float text_size_ = 1.0f;
  glm::vec2 alignment_ = { 0.5, 0.5 };
  ci::ColorA color_ = { 1.0f, 1.0f, 1.0f, 1.0f };


public:
  Text(const ci::JsonTree& params)
    : text_(params.getValueForKey<std::string>("text"))
  {
    if (params.hasChild("text_size"))
    {
      text_size_ = params.getValueForKey<float>("text_size");
    }
    if (params.hasChild("alignment"))
    {
      alignment_ = Json::getVec<glm::vec2>(params["alignment"]);
    }
    if (params.hasChild("color"))
    {
      color_ = Json::getColorA<float>(params["color"]);
    }
  }

  ~Text() = default;


private:
  void draw(const ci::Rectf& rect, UI::Drawer& drawer) noexcept override
  {
    ci::gl::pushModelMatrix();
    ci::gl::ScopedGlslProg prog(drawer.getFontShader());

    auto& font = drawer.getFont();
    auto size = font.drawSize(text_);
    ci::gl::translate(rect.getCenter() - size * alignment_ * text_size_);
    ci::gl::scale(glm::vec3(text_size_));
    font.draw(text_, glm::vec2(0, 0), color_);
    ci::gl::popModelMatrix();
  }

};

} }