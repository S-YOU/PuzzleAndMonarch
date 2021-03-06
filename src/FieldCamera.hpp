﻿#pragma once

//
// Field Camera
//

#include <boost/noncopyable.hpp>


namespace ngs {

class FieldCamera
  : private boost::noncopyable
{
public:
  FieldCamera(const ci::JsonTree& params)
    : rotation_(toRadians(Json::getVec<glm::vec2>(params["camera.rotation"]))),
      rotation_y_(rotation_.y),
      distance_(params.getValueForKey<float>("camera.distance")),
      target_position_(Json::getVec<glm::vec3>(params["target_position"])),
      distance_range_(Json::getVec<glm::vec2>(params["camera_distance_range"])),
      angle_range_(toRadians(Json::getVec<glm::vec2>(params["camera_angle_range"]))),
      ease_rate_(Json::getVec<glm::dvec2>(params["camera_ease_rate"])),
      rotate_rate_(Json::getVec<glm::dvec2>(params["camera_rotate_rate"])),
      retarget_rect_(Json::getRect<float>(params["camera_retarget"])),
      field_center_(target_position_),
      field_distance_(distance_),
      map_center_(field_center_),
      initial_rotation_(rotation_),
      initial_distance_(distance_),
      initial_target_position_(target_position_)
  {
  }

  ~FieldCamera() = default;


  void update(double delta_time)
  {
    if (demo_)
    {
      demo_time_ += delta_time;
      if (demo_time_ >= demo_duration_)
      {
        demo_time_ = demo_duration_;
        demo_ = false;
      }
      auto t = demo_time_ / demo_duration_;

      distance_ = demo_start_distance_ + demo_difference_ * demo_ease_(t);
    }

    if (!active_) return;

    target_position_ += (field_center_ - target_position_) * float(1 - std::pow(ease_rate_.x, delta_time * ease_rate_.y));
    distance_ += (field_distance_ - distance_) * float(1 - std::pow(ease_rate_.x, delta_time * ease_rate_.y));

    rotation_.y += (rotation_y_ - rotation_.y) * float(1 - std::pow(rotate_rate_.x, delta_time * rotate_rate_.y));
  }


  void setActive(bool active)
  {
    active_ = active;
    demo_   = false;
  }


  // 全リセット
  void resetAll() noexcept
  {
    force_camera_ = false;

    reset();

    rotation_   = initial_rotation_;
    rotation_y_ = rotation_.y;
    distance_ = initial_distance_;
    target_position_ = initial_target_position_;
  }

  // リセット(rotationは維持)
  void reset() noexcept
  {
    field_center_   = initial_target_position_;
    field_distance_ = initial_distance_;
    
    skip_easing_ = false;
  }

  void forceCenter() noexcept
  {
    target_position_ = field_center_;
  }


  void rotate(glm::vec3 pos, glm::vec3 prev_pos)
  {
    pos      = glm::normalize(pos - target_position_);
    prev_pos = glm::normalize(prev_pos - target_position_);

    // 外積から回転量が決まる
    float cross = prev_pos.x * pos.z - prev_pos.z * pos.x;
    rotation_y_ += std::asin(cross);
  }

  void addYaw(float r) noexcept
  {
    rotation_y_ += r;
  }

  void distance(float d)
  {
    distance_ = d;
  }

  // 距離設定
  void setDistance(float rate)
  {
    auto distance = distance_ / rate;

    // 寄りと引きの限界付近で滑らかな挙動を得る
    if ((distance > distance_) && (distance > distance_range_.y))
    {
      // 引きの場合
      distance += (distance_range_.y - distance) * 0.5;
    }
    else if ((distance < distance_) && (distance < distance_range_.x))
    {
      // 寄せの場合
      distance += (distance_range_.x - distance) * 0.25;
    }
    distance_ = distance;
    
    field_distance_ = ci::clamp(distance_, distance_range_.x, distance_range_.y);
    
    skip_easing_ = true;
  }

  // 平行移動
  void setTranslate(const glm::vec3& v, ci::CameraPersp& camera)
  {
    target_position_ += v;
    field_center_ = target_position_;
    eye_position_ += v;
    camera.setEyePoint(eye_position_);

    skip_easing_ = true;
  }

  void force(bool value) noexcept
  {
    force_camera_ = value;
  }


  // フィールドの広さから注視点と距離を計算
  void calcViewRange(const glm::vec3& center, float radius, float fov, const glm::vec3& put_pos,
                     const ci::CameraPersp& camera)
  {
    // NOTE 縦画面と横画面で調整内容が違う
    auto aspect = camera.getAspectRatio();

    map_center_ = center;
    // FIXME マジックナンバー
    radius += (aspect > 1.0f) ? PANEL_SIZE / 4           // 横画面
                              : PANEL_SIZE;              // 縦画面

    float half_fov = toRadians(fov * 0.5f);
    float distance = radius / std::sin(half_fov);
    // カメラが斜め上から見下ろしているのを考慮
    float n = radius / std::cos(rotation_.x);
    // FIXME マジックナンバー
    distance -= n * 0.75f;

    // 強制モード
    if (force_camera_)
    {
      DOUT << "Force mode" << std::endl;

      field_center_.x = map_center_.x;
      field_center_.z = map_center_.z;
      field_distance_ = ci::clamp(distance,
                                  distance_range_.x, distance_range_.y);

      return;
    }

    // パネルを置く前にカメラ操作があった
    if (skip_easing_)
    {
      DOUT << "Skip Easing" << std::endl;

      static glm::vec3 tbl[] = {
        { -PANEL_SIZE / 2, 0, -PANEL_SIZE / 2 },
        {  PANEL_SIZE / 2, 0, -PANEL_SIZE / 2 },
        { -PANEL_SIZE / 2, 0,  PANEL_SIZE / 2 },
        {  PANEL_SIZE / 2, 0,  PANEL_SIZE / 2 },
      };

      auto in_view = std::all_of(std::begin(tbl), std::end(tbl),
                                 [this, &camera, &put_pos](const auto& ofs)
                                 {
                                   auto p1 = camera.worldToNdc(put_pos + ofs);
                                   glm::vec2 p{ p1.x, p1.y };
                                   return retarget_rect_.contains(p);
                                 });

      // bool in_view = true;
      // for (const auto& ofs : tbl)
      // {
      //   auto p1 = camera.worldToNdc(put_pos + ofs);

      //   DOUT << p1 << std::endl;

      //   glm::vec2 p(p1.x, p1.y);
      //   if (!retarget_rect_.contains(p))
      //   {
      //     in_view = false;
      //     break;
      //   }
      // }

      if (in_view)
      {
        // パネルを置く場所が画面からはみ出しそうでなければそのまま
        DOUT << "Keep camera" << std::endl; 
        return;
      }

      // 甘めに調整するモード発動
      ease_distance_rate_ = 0.6f;
      skip_easing_ = false;
    }

    if (ease_distance_rate_ > 0.1f)
    {
      auto c = glm::mix(map_center_, target_position_, ease_distance_rate_);
      field_center_.x = c.x;
      field_center_.z = c.z;

      auto d = glm::mix(distance, distance_, ease_distance_rate_);
      field_distance_ = ci::clamp(std::max(d, distance_),
                                  distance_range_.x, distance_range_.y);
      ease_distance_rate_ *= 0.8f;
    }
    else
    {
      field_center_.x = map_center_.x;
      field_center_.z = map_center_.z;

      field_distance_ = ci::clamp(std::max(distance, distance_),
                                  distance_range_.x, distance_range_.y);
    }
  }

  // 内容を他のクラスへ反映 
  void applyDetail(ci::CameraPersp& camera, View& view) 
  {
    float d = ci::clamp(distance_, distance_range_.x, distance_range_.y) - distance_range_.x;
    float t = d / (distance_range_.y - distance_range_.x);
    float x = glm::mix(angle_range_.x, angle_range_.y, t);
    glm::quat q(glm::vec3{ rotation_.x + x, rotation_.y, 0 });
    glm::vec3 p = q * glm::vec3{ 0, 0, -distance_ };
    camera.lookAt(p + target_position_, target_position_);
    eye_position_ = camera.getEyePoint();

    view.setupShadowCamera(target_position_);
  }

  const glm::vec3& getTargetPosition() const noexcept
  {
    return target_position_;
  }

  // distanceのeaseデモ開始
  void beginDemo(float duration, const std::string& easing, float distance)
  {
    active_ = false;
    demo_   = true;

    demo_time_           = 0.0f;
    demo_duration_       = duration;
    demo_start_distance_ = distance;
    demo_difference_     = field_distance_ - distance;
    demo_ease_           = getEaseFunc(easing);

    distance_ = distance;
  }


private:
  bool active_ = true;
  bool demo_   = false;

  // 向きと注視点からの距離
  glm::vec2 rotation_;
  float distance_;

  // 回転操作を滑らかにするために用意
  float rotation_y_;

  // 注視位置
  glm::vec3 target_position_;
  // カメラ位置
  glm::vec3 eye_position_;

  // ピンチング操作時の距離の範囲
  glm::vec2 distance_range_;
  glm::vec2 angle_range_;

  // 補間用係数
  glm::dvec2 ease_rate_;
  glm::dvec2 rotate_rate_;

  // 再追尾用の範囲
  ci::Rectf retarget_rect_; 

  // カメラ計算を優先
  bool force_camera_ = false;
  // カメラを動かしたので補間をスキップする
  bool skip_easing_ = false;
  // 補間具合
  float ease_distance_rate_= 0;

  // Fieldの中心座標
  glm::vec3 field_center_;
  float field_distance_;

  glm::vec3 map_center_;

  // 演出用
  float demo_duration_;
  float demo_time_;
  float demo_start_distance_;
  float demo_difference_;
  ci::EaseFn demo_ease_;

  // カメラを初期状態に戻すための変数
  glm::vec2 initial_rotation_;
  float initial_distance_;
  glm::vec3 initial_target_position_;
};

}
