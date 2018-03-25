﻿#pragma once

//
// 表示関連
//

#include <boost/noncopyable.hpp>
#include <deque>
#include <cinder/TriMesh.h>
#include <cinder/gl/Vbo.h>
#include <cinder/gl/Texture.h>
#include <cinder/ObjLoader.h>
#include <cinder/ImageIo.h>
#include "PLY.hpp"
#include "Shader.hpp"


namespace ngs {

enum {
  PANEL_SIZE = 20,
};


class View
  : private boost::noncopyable
{
  // パネル
  std::vector<std::string> panel_path;
  std::vector<ci::gl::VboMeshRef> panel_models;
  // AABBは全パネル共通
  ci::AxisAlignedBox panel_aabb_;

  // 演出用
  ci::gl::VboMeshRef blank_model;
  ci::gl::VboMeshRef selected_model;
  ci::gl::VboMeshRef cursor_model;

  // 背景
  ci::gl::VboMeshRef bg_model;
  glm::vec3 bg_scale_;

  // Field上のパネル(Modelと被っている)
  struct Panel
  {
    glm::ivec2 field_pos;
    glm::vec3 position;
    glm::vec3 rotation;
    int index;
  };
  // NOTICE 追加時にメモリ上で再配置されるのを避けるためstd::vectorではない
  std::deque<Panel> field_panels_;

  // 得点時演出用
  ci::gl::VboMeshRef effect_model;

  ci::gl::GlslProgRef field_shader_;
  ci::Anim<ci::ColorA> field_color_ = ci::ColorA::white();

  ci::gl::GlslProgRef bg_shader_;
  ci::gl::Texture2dRef bg_texture_;

  ci::gl::GlslProgRef shadow_shader_;

  // 影レンダリング用
  ci::gl::Texture2dRef shadow_map_;
  ci::gl::FboRef shadow_fbo_;

  ci::CameraPersp light_camera_;
  glm::vec3 light_pos_;

  // 画面演出用情報
  float panel_height_;
  glm::vec2 put_duration_;
  std::string put_ease_;

  // PAUSE時にくるっと回す用
  ci::Anim<float> field_rotate_offset_ = 0.0f;
  glm::vec2 pause_duration_;
  std::string pause_ease_;

  // パネル位置
  ci::Anim<glm::vec3> panel_disp_pos_;
  glm::vec2 disp_ease_duration_;
  std::string disp_ease_name_;

  // パネルの回転
  ci::Anim<float> rotate_offset_;
  glm::vec2 rotate_ease_duration_;
  std::string rotate_ease_name_;

  // 次のパネルを引いた時の演出
  ci::Anim<float> height_offset_;
  float height_ease_start_;
  float height_ease_duration_;
  std::string height_ease_name_;

  // パネルを置くゲージ演出
  float put_gauge_timer_ = 0.0f;

  struct Effect {
    bool active;
    glm::vec3 pos;
    glm::vec3 rot;
  };
  // FIXME 途中の削除が多いのでvectorは向いていない??
  std::list<Effect> effects_;

  // Tween用
  ci::TimelineRef timeline_;
  // Pause中でも動作
  ci::TimelineRef force_timeline_;

  // 読まれてないパネルを読み込む
  const ci::gl::VboMeshRef& getPanelModel(int number) noexcept
  {
    if (!panel_models[number])
    {
      auto tri_mesh = PLY::load(panel_path[number]);
      // panel_aabb[number] = tri_mesh.calcBoundingBox();
      auto mesh = ci::gl::VboMesh::create(tri_mesh);
      panel_models[number] = mesh;
    }

    return panel_models[number];
  }

  // OBJ形式を読み込む
  static ci::gl::VboMeshRef loadObj(const std::string& path) noexcept
  {
    ci::ObjLoader loader(Asset::load(path));
    auto mesh = ci::gl::VboMesh::create(ci::TriMesh(loader));

    return mesh;
  }

  // 影レンダリング用の設定
  void setupShadowMap(const glm::ivec2& fbo_size) noexcept
  {
    ci::gl::Texture2d::Format depthFormat;
    depthFormat.setInternalFormat(GL_DEPTH_COMPONENT16);
    depthFormat.setCompareMode(GL_COMPARE_REF_TO_TEXTURE);
    depthFormat.setMagFilter(GL_LINEAR);
    depthFormat.setMinFilter(GL_LINEAR);
    depthFormat.setWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);	
    depthFormat.setCompareFunc(GL_LEQUAL);

    shadow_map_ = ci::gl::Texture2d::create(fbo_size.x, fbo_size.y, depthFormat);

    try
    {
      ci::gl::Fbo::Format fboFormat;
      fboFormat.attachment(GL_DEPTH_ATTACHMENT, shadow_map_);
      shadow_fbo_ = ci::gl::Fbo::create(fbo_size.x, fbo_size.y, fboFormat);
    }
    catch (const std::exception& e)
    {
      DOUT << "FBO ERROR: " << e.what() << std::endl;
    }
  }


public:
  // 表示用の情報
  struct Info {
    bool playing;
    bool can_put;

    // 手持ちのパネル
    u_int panel_index;
    // 向き
    u_int panel_rotation;

    // 選択位置
    glm::ivec2 field_pos;
    // 背景位置
    glm::vec3 bg_pos;

    const std::vector<glm::ivec2>* blank;

    const ci::CameraPersp* main_camera;
  };


  View(const ci::JsonTree& params) noexcept
    : panel_height_(params.getValueForKey<float>("panel_height")),
      bg_scale_(Json::getVec<glm::vec3>(params["bg_scale"])),
      bg_texture_(ci::gl::Texture2d::create(ci::loadImage(Asset::load(params.getValueForKey<std::string>("bg_texture"))))),
      put_duration_(Json::getVec<glm::vec2>(params["put_duration"])),
      put_ease_(params.getValueForKey<std::string>("put_ease")),
      pause_duration_(Json::getVec<glm::vec2>(params["pause_duration"])),
      pause_ease_(params.getValueForKey<std::string>("pause_ease")),
      disp_ease_duration_(Json::getVec<glm::vec2>(params["disp_ease_duration"])),
      disp_ease_name_(params.getValueForKey<std::string>("disp_ease_name")),
      rotate_ease_duration_(Json::getVec<glm::vec2>(params["rotate_ease_duration"])),
      rotate_ease_name_(params.getValueForKey<std::string>("rotate_ease_name")),
      height_ease_start_(params.getValueForKey<float>("height_ease_start")),
      height_ease_duration_(params.getValueForKey<float>("height_ease_duration")),
      height_ease_name_(params.getValueForKey<std::string>("height_ease_name")),
      timeline_(ci::Timeline::create()),
      force_timeline_(ci::Timeline::create())
  {
    const auto& path = params["panel_path"];
    for (const auto p : path)
    {
      panel_path.push_back(p.getValue<std::string>());
    }
    panel_models.resize(panel_path.size());

    panel_aabb_ = ci::AxisAlignedBox(glm::vec3(-PANEL_SIZE / 2, 0, -PANEL_SIZE / 2),
                                     glm::vec3(PANEL_SIZE / 2, 1, PANEL_SIZE / 2));

    blank_model    = ci::gl::VboMesh::create(PLY::load(params.getValueForKey<std::string>("blank_model")));
    selected_model = ci::gl::VboMesh::create(PLY::load(params.getValueForKey<std::string>("selected_model")));
    cursor_model   = ci::gl::VboMesh::create(PLY::load(params.getValueForKey<std::string>("cursor_model")));
    bg_model       = loadObj(params.getValueForKey<std::string>("bg_model"));
    effect_model   = ci::gl::VboMesh::create(PLY::load(params.getValueForKey<std::string>("effect_model")));

    {
      auto size = Json::getVec<glm::ivec2>(params["shadow_map"]);
      setupShadowMap(size);
    }

    {
      auto name = params.getValueForKey<std::string>("field_shader");
      field_shader_ = createShader(name, name);

      field_shader_->uniform("uShadowMap", 0);
      field_shader_->uniform("uShadowIntensity", params.getValueForKey<float>("shadow_intensity"));
    }
    {
      auto name = params.getValueForKey<std::string>("bg_shader");
      bg_shader_ = createShader(name, name);

      float checker_size = bg_scale_.x / (PANEL_SIZE / 2);
      bg_shader_->uniform("u_checker_size", checker_size);
      
      bg_shader_->uniform("u_bright", Json::getVec<glm::vec4>(params["bg_bright"]));
      bg_shader_->uniform("u_dark",   Json::getVec<glm::vec4>(params["bg_dark"]));
      bg_shader_->uniform("uShadowMap", 0);
      bg_shader_->uniform("uTex", 1);
      bg_shader_->uniform("uShadowIntensity", params.getValueForKey<float>("shadow_intensity"));
    }
    {
      auto name = params.getValueForKey<std::string>("shadow_shader");
      shadow_shader_ = createShader(name, name);
    }
    {
      light_pos_ = Json::getVec<glm::vec3>(params["light.pos"]);
      
      float fov    = params.getValueForKey<float>("light.fov"); 
      float near_z = params.getValueForKey<float>("light.near_z"); 
      float far_z  = params.getValueForKey<float>("light.far_z"); 
      light_camera_.setPerspective(fov, shadow_fbo_->getAspectRatio(), near_z, far_z);
    }
  }

  ~View() = default;


  // Timelineとかの更新
  void update(double delta_time, bool game_paused) noexcept
  {
    put_gauge_timer_ += delta_time;
    force_timeline_->step(delta_time);

    if (game_paused) return;

    timeline_->step(delta_time);
  }


  void clear() noexcept
  {
    timeline_->clear();
    field_panels_.clear();
    effects_.clear();

    field_rotate_offset_ = 0.0f;
  }


  void setColor(const ci::ColorA& color) noexcept
  {
    field_color_.stop();
    field_color_ = color;
    field_shader_->uniform("u_color", color);
    bg_shader_->uniform("u_color", color);
  }

  void setColor(float duration, const ci::ColorA& color, float delay = 0.0f) noexcept
  {
    auto option = timeline_->apply(&field_color_, color, duration);
    option.updateFn([this]() noexcept
                    {
                      field_shader_->uniform("u_color", field_color_());
                      bg_shader_->uniform("u_color", field_color_());
                    });
    option.delay(delay);
  }

  // Pause演出開始
  void pauseGame() noexcept
  {
    timeline_->apply(&field_rotate_offset_, toRadians(180.0f), pause_duration_.x, getEaseFunc(pause_ease_));
  }

  // Pause演出解除
  void resumeGame() noexcept
  {
    timeline_->apply(&field_rotate_offset_, 0.0f, pause_duration_.y, getEaseFunc(pause_ease_));
  }

  // パネル追加
  void addPanel(int index, const glm::ivec2& pos, u_int rotation) noexcept
  {
    glm::ivec2 p = pos * int(PANEL_SIZE);

    static const float r_tbl[] = {
      0.0f,
      -180.0f * 0.5f,
      -180.0f,
      -180.0f * 1.5f 
    };

    Panel panel = {
      pos,
      { p.x, 0, p.y },
      { 0, ci::toRadians(r_tbl[rotation]), 0 }, 
      index
    };

    field_panels_.push_back(panel);
  }

  // パネル位置決め
  void setPanelPosition(const glm::vec3& pos) noexcept
  {
    panel_disp_pos_.stop();
    panel_disp_pos_ = pos;
  }

  // パネル移動
  void startMovePanelEase(const glm::vec3& target_pos, float rate) noexcept
  {
    // panel_disp_pos_.stop();
    auto duration = glm::mix(disp_ease_duration_.x, disp_ease_duration_.y, rate);
    timeline_->apply(&panel_disp_pos_, target_pos, duration, getEaseFunc(disp_ease_name_));
  }

  // パネル回転
  void startRotatePanelEase(float rate) noexcept
  {
    // rotate_offset_.stop();
    rotate_offset_ = 90.0f;
    auto duration = glm::mix(rotate_ease_duration_.x, rotate_ease_duration_.y, rate);
    timeline_->apply(&rotate_offset_, 0.0f, duration, getEaseFunc(rotate_ease_name_));
  }

  // パネルを置く時の演出
  void startPutEase(double time_rate) noexcept
  {
    auto duration = glm::mix(put_duration_.x, put_duration_.y, time_rate);

    auto& p = field_panels_.back();
    p.position.y = panel_height_;
    timeline_->applyPtr(&p.position.y, 0.0f,
                        duration, getEaseFunc(put_ease_));
  }

  // 次のパネルの出現演出
  void startNextPanelEase() noexcept
  {
    rotate_offset_.stop();
    rotate_offset_ = 0.0f;

    height_offset_ = height_ease_start_;
    timeline_->apply(&height_offset_, 0.0f, height_ease_duration_, getEaseFunc(height_ease_name_));
  }


  // 得点した時の演出
  void startEffect(const glm::ivec2& pos) noexcept
  {
    glm::vec3 gpos{ pos.x * PANEL_SIZE, 0, pos.y * PANEL_SIZE };

    for (int i = 0; i < 10; ++i)
    {
      glm::vec3 ofs{
        ci::randFloat(-PANEL_SIZE / 2, PANEL_SIZE / 2),
        0,
        ci::randFloat(-PANEL_SIZE / 2, PANEL_SIZE / 2)
      };

      // ランダムに落下する立方体
      effects_.push_back({ true, gpos + ofs, glm::vec3() });
      auto& effect = effects_.back();

      auto end_pos = gpos + ofs + glm::vec3(0, ci::randFloat(15.0f, 30.0f), 0);

      float duration = ci::randFloat(1.25f, 1.75f);
      auto options = timeline_->applyPtr(&effect.pos, end_pos, duration);
      options.finishFn([&effect]() noexcept
                       {
                         effect.active = false;
                       });
    }
  }

  // 演出表示
  void drawEffect() noexcept
  {
    ci::gl::ScopedModelMatrix m;

    for (auto it = std::begin(effects_); it != std::end(effects_); )
    {
      if (!it->active)
      {
        it = effects_.erase(it);
        continue;
      }

      auto mtx = glm::translate(it->pos) * glm::eulerAngleXYZ(it->rot.x, it->rot.y, it->rot.z);
      ci::gl::setModelMatrix(mtx);
      ci::gl::draw(effect_model);

      ++it;
    }
  }


  const ci::AxisAlignedBox& panelAabb(int number) const noexcept
  {
    return panel_aabb_;
  }


  void setupShadowCamera(const glm::vec3& map_center) noexcept
  {
    light_camera_.lookAt(map_center + light_pos_, map_center);
  }


  // 影のレンダリング
  void renderShadow(const Info& info) noexcept
  {
    // Set polygon offset to battle shadow acne
    ci::gl::enable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);

    // Render scene to fbo from the view of the light
    ci::gl::ScopedFramebuffer fbo(shadow_fbo_);
    ci::gl::ScopedViewport viewport(glm::vec2(0), shadow_fbo_->getSize());
    ci::gl::clear(ci::Color::black());
    ci::gl::setMatrices(light_camera_);

    ci::gl::ScopedGlslProg prog(shadow_shader_);

    drawFieldPanels();

    if (info.playing)
    {
      // 置ける場所
      drawFieldBlank(*info.blank);
      
      // 手持ちパネル
      auto pos = panel_disp_pos_() + glm::vec3(0, height_offset_, 0);
      drawPanel(info.panel_index, pos, info.panel_rotation, rotate_offset_);
    }

    // Disable polygon offset for final render
    ci::gl::disable(GL_POLYGON_OFFSET_FILL);
  }

  void renderField(const Info& info) noexcept
  {
    ci::gl::setMatrices(*info.main_camera);
    ci::gl::clear(ci::Color::black());

    auto mat = light_camera_.getProjectionMatrix() * light_camera_.getViewMatrix();
    field_shader_->uniform("uShadowMatrix", mat);
    bg_shader_->uniform("uShadowMatrix", mat);

    ci::gl::ScopedGlslProg prog(field_shader_);
    ci::gl::ScopedTextureBind texScope(shadow_map_, 0);
    drawFieldPanels();

    if (info.playing)
    {
      // 置ける場所
      drawFieldBlank(*info.blank);
      
      // 手持ちパネル
      auto pos = panel_disp_pos_() + glm::vec3(0, height_offset_, 0);
      drawPanel(info.panel_index, pos, info.panel_rotation, rotate_offset_);

      // 選択箇所
      float s = std::abs(std::sin(put_gauge_timer_ * 6.0f)) * 0.1;
      glm::vec3 scale(0.9 + s, 1, 0.9 + s);
      drawFieldSelected(info.field_pos, scale);

      // 「置けますよ」アピール
      if (info.can_put)
      {
        scale.x = 1.0 + s;
        scale.z = 1.0 + s;
        drawCursor(pos, scale);
      }
    }

    drawFieldBg(info.bg_pos);
    drawEffect();
  }


  // フィールド表示
  void drawField(const Info& info) noexcept
  {
    ci::gl::enableDepth();
    ci::gl::enable(GL_CULL_FACE);
    ci::gl::disableAlphaBlending();

    renderShadow(info);
    renderField(info);
  }

  
  // パネルを１枚表示
  void drawPanel(int number, const glm::vec3& pos, u_int rotation, float rotate_offset) noexcept
  {
    static const float r_tbl[] = {
      0.0f,
      -180.0f * 0.5f,
      -180.0f,
      -180.0f * 1.5f 
    };
    
    ci::gl::ScopedModelMatrix m;

    auto mtx = glm::translate(pos) * glm::eulerAngleXYZ(0.0f, toRadians(r_tbl[rotation] + rotate_offset), 0.0f);
    ci::gl::setModelMatrix(mtx);

    const auto& model = getPanelModel(number);
    ci::gl::draw(model);
  }

  // Fieldのパネルを全て表示
  void drawFieldPanels() noexcept
  {
    ci::gl::ScopedModelMatrix m;

    glm::vec2 offset[] = {
      {  field_rotate_offset_, 0.0f },
      { -field_rotate_offset_, 0.0f },
      { 0.0f,  field_rotate_offset_ },
      { 0.0f, -field_rotate_offset_ },
    };

    const auto& panels = field_panels_;
    for (const auto& p : panels)
    {
      // TODO この計算はaddの時に済ませる
      int index = (p.field_pos.x + p.field_pos.y * 3) & 0b11;
      const auto& ofs = offset[index];

      auto mtx = glm::translate(p.position)
                 * glm::eulerAngleXYZ(p.rotation.x + ofs.x, p.rotation.y, p.rotation.z + ofs.y);
      ci::gl::setModelMatrix(mtx);

      const auto& model = getPanelModel(p.index);
      ci::gl::draw(model);
    }
  }
  
  // Fieldの置ける場所をすべて表示
  void drawFieldBlank(const std::vector<glm::ivec2>& blank) noexcept
  {
    ci::gl::ScopedModelMatrix m;

    for (const auto& pos : blank)
    {
      glm::ivec2 p = pos * int(PANEL_SIZE);

      auto mtx = glm::translate(glm::vec3(p.x, 0.0, p.y));
      ci::gl::setModelMatrix(mtx);
      ci::gl::draw(blank_model);
    }
  }

  // 置けそうな箇所をハイライト
  void drawFieldSelected(const glm::ivec2& pos, const glm::vec3& scale) noexcept
  {
    glm::ivec2 p = pos * int(PANEL_SIZE);
    
    ci::gl::ScopedModelMatrix m;

    auto mtx = glm::translate(glm::vec3(p.x, 0.0f, p.y));
    mtx = glm::scale(mtx, scale);
    ci::gl::setModelMatrix(mtx);
    ci::gl::draw(selected_model);
  }

  void drawCursor(const glm::vec3& pos, const glm::vec3& scale) noexcept
  {
    ci::gl::ScopedModelMatrix m;
    
    auto mtx = glm::translate(pos);
    mtx = glm::scale(mtx, scale);
    ci::gl::setModelMatrix(mtx);
    ci::gl::draw(cursor_model);
  }

  // 背景
  void drawFieldBg(const glm::vec3& pos) noexcept
  {
    ci::gl::ScopedGlslProg prog(bg_shader_);
    ci::gl::ScopedTextureBind tex(bg_texture_, 1);
    ci::gl::ScopedModelMatrix m;

    glm::vec2 offset { pos.x * (1.0f / PANEL_SIZE), -pos.z * (1.0f / PANEL_SIZE) };
    bg_shader_->uniform("u_pos", offset);

    auto mtx = glm::translate(pos);
    mtx = glm::scale(mtx, bg_scale_);
    ci::gl::setModelMatrix(mtx);
    ci::gl::draw(bg_model);
  }
};


#ifdef DEBUG

// パネルのエッジを表示
void drawPanelEdge(const Panel& panel, const glm::vec3& pos, u_int rotation) noexcept
{
  static const float r_tbl[] = {
    0.0f,
    -180.0f * 0.5f,
    -180.0f,
    -180.0f * 1.5f 
  };

  ci::gl::pushModelView();
  ci::gl::translate(pos.x, pos.y, pos.z);
  ci::gl::rotate(toRadians(ci::vec3(0.0f, r_tbl[rotation], 0.0f)));

  ci::gl::lineWidth(10);

  const auto& edge = panel.getEdge();
  for (auto e : edge) {
    ci::Color col;
    if (e & Panel::PATH)   col = ci::Color(1.0, 1.0, 0.0);
    if (e & Panel::FOREST) col = ci::Color(0.0, 0.5, 0.0);
    if (e & Panel::GRASS)  col = ci::Color(0.0, 1.0, 0.0);
    ci::gl::color(col);

    ci::gl::drawLine(ci::vec3(-10.1, 1, 10.1), ci::vec3(10.1, 1, 10.1));
    ci::gl::rotate(toRadians(ci::vec3(0.0f, 90.0f, 0.0f)));
  }

  ci::gl::popModelView();

  ci::gl::color(ci::Color(1, 1, 1));
}

#endif

}

