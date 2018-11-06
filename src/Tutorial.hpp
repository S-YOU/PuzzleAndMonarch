﻿#pragma once

//
// チュートリアル
//
// 1. ４箇所のBlankを指し示し「パネル移動」
//    移動操作で次へ
// 2. 手持ちパネルを指し示し「パネル回転」
//    回転操作で次へ
// 3. 手持ちパネルを指し示し「長押しで設置」
//    設置操作で次へ
// 4. 道は道同士、森は森同士、草原は草原同士で繋がらないとパネルは置けない
// 5. 街と手持ちパネルを指し示し「道で繋ぐ→得点」
// 6. 森と手持ちパネルを指し示し「森を完成→得点」
// 7. 教会とその周囲のBlankを指し示し「周囲をパネルで埋める→得点」
//


#include "Task.hpp"
#include "CountExec.hpp"
#include "UICanvas.hpp"
#include "TweenUtil.hpp"
#include "EventSupport.hpp"
#include "CountExec.hpp"


namespace ngs {

class Tutorial 
  : public Task
{
  enum {
    PANEL_MOVE,
    PANEL_ROTATE,
    PANEL_PUT,

    GET_TOWN,
    GET_FOREST,
    GET_CHURCH,
  };


public:
  Tutorial(const ci::JsonTree& params, Event<Arguments>& event, UI::Drawer& drawer, TweenCommon& tween_common)
    : event_(event),
      canvas_(event, drawer, tween_common,
              params["ui.camera"],
              Params::load(params.getValueForKey<std::string>("tutorial.canvas")),
              Params::load(params.getValueForKey<std::string>("tutorial.tweens")))
  {
    // TIPS コールバック関数にダミーを割り当てておく
    update_ = [](u_int) { return std::vector<glm::vec3>(); };

    setupAdvice();
    startTutorial();

    // Pause操作
    holder_ += event.connect("GameMain:pause",
                             [this](const Connection&, const Arguments&)
                             {
                               pause_ = true;
                               // Pause中はチュートリアルの指示を消す
                               canvas_.startTween("pause");
                             });
    holder_ += event.connect("GameMain:resume",
                             [this](const Connection&, const Arguments&)
                             {
                               pause_ = false;
                               canvas_.startTween("resume");
                             });

    // Callback登録
    holder_ += event.connect("Tutorial:callback",
                             [this](const Connection&, const Arguments& args)
                             {
                               DOUT << "Tutorial:callback" << std::endl;
                               update_ = boost::any_cast<const std::function<std::vector<glm::vec3> (u_int)>&>(args.at("callback"));
                             });

    // Tutorial終了
    holder_ += event.connect("Game:Finish",
                              [this](const Connection&, const Arguments&)
                              {
                                pause_ = true;
                                canvas_.startTween("pause");
                                dispAdvice();
                              });

    auto wipe_delay    = params.getValueForKey<double>("ui.wipe.delay");
    auto wipe_duration = params.getValueForKey<double>("ui.wipe.duration");

    holder_ += event.connect("agree:touch_ended",
                             [this, wipe_delay, wipe_duration](const Connection&, const Arguments&) noexcept
                             {
                               DOUT << "Agree." << std::endl;
                               canvas_.active(false);
                               canvas_.startCommonTween("root", "out-to-right");
                               count_exec_.add(wipe_delay,
                                               [this]() noexcept
                                               {
                                                 event_.signal("Tutorial:Finished", Arguments());
                                               });
                               count_exec_.add(wipe_duration,
                                               [this]() noexcept
                                               {
                                                 finishTask();
                                               });
                             });

    holder_ += event.connect("Game:Aborted",
                             [this](const Connection&, const Arguments&)
                             {
                               finishTask();
                             });

    canvas_.startTween("start");
    setupCommonTweens(event_, holder_, canvas_, "agree");
  }

  ~Tutorial() = default;
  

private:
  bool update(double current_time, double delta_time) noexcept override
  {
    count_exec_.update(delta_time);
    if (pause_) return active_;

    // 更新関数を実行
    const u_int kinds[]{
      0b10,
      0b1,
      0b1,
      0,
      0,
      0b101,
      0b101,
      0b1001,
      0b1001,
      0,
      0b10000,
      0b10000,
      0,
      0,
    };

    indication_positions_ = update_(kinds[level_]);
    updateIndiration();

    return active_;
  }
  
  // タスク終了
  void finishTask()
  {
    active_ = false;
  }


  // 指示を出す
  void startTutorial()
  {
    const char* id[]{
      "Game:PanelMove",          // 移動
      "Game:PanelRotate",        // 回転
      "Game:PutPanel",           // 設置
      "Game:PutPanel",           // パネルを置ける条件
      "Game:PutPanel",           // パネルを置ける条件
      "Game:PutPanel",           // 道の説明
      "Game:PutPanel",           // 道の説明
      "Game:PutPanel",           // 森の説明
      "Game:PutPanel",           // 森の説明
      "Game:PutPanel",
      "Game:PutPanel",           // 教会の説明
      "Game:PutPanel",           // 教会の説明
      "Game:PutPanel",
      nullptr 
    };

    const auto* p = id[level_];

    if (p)
    {
      count_exec_.add(0.2,
                      [this, p]()
                      {
                        holder_ += event_.connect(p,
                                                  [this](const Connection& connection, const Arguments&)
                                                  {
                                                    ++level_;
                                                    connection.disconnect();
                                                    // 次の指示
                                                    startTutorial();
                                                  });
                      });
      setTutorialText();
    }
  }

  void setTutorialText()
  {
    const char* id[]{
      "Tutorial02",
      "Tutorial03",
      "Tutorial04",
      "Tutorial05",
      "Tutorial05",
      "Tutorial06",
      "Tutorial06",
      "Tutorial07",
      "Tutorial07",
      "Tutorial08",
      "Tutorial09",
      "Tutorial09",
      "Tutorial10"
     };

    canvas_.setWidgetText("text", AppText::get(id[level_]));
  }

  // 指示位置表示
  void updateIndiration()
  {
    int i = 0;
    for (const auto& pos : indication_positions_)
    {
      char id[16];
      sprintf(id, "arrow%d", i);
      canvas_.enableWidget(id, true);

      // 正規化座標→スクリーン座標
      auto p = canvas_.ndcToPos(pos);
      canvas_.setWidgetParam(id, "offset", p);

      ++i;
    }
    // 残りは非表示
    for (; i < 8; ++i)
    {
      char id[16];
      sprintf(id, "arrow%d", i);
      canvas_.enableWidget(id, false);
    }
  }


  // 言語圏によって表示位置を変更する
  void setupAdvice()
  {
    // 言語圏によって表示位置を変更
    auto offset_x = std::stof(AppText::get("Tutorial11"));
    auto* rect = boost::any_cast<ci::Rectf*>(canvas_.getWidgetParam("advice", "rect"));
    rect->x1 += offset_x;
    rect->x2 += offset_x;
    canvas_.setWidgetParam("advice", "rect", *rect);
  }

  // 助言を表示
  void dispAdvice()
  {
    canvas_.enableWidget("advice");

    static const char* tbl[]{
      "check%d",
      "advice%d",
    };

    for (const auto* t : tbl)
    {
      for (size_t i = 0; i < 3; ++i)
      {
        char id[16];
        sprintf(id, t, i);
        canvas_.setTweenTarget(id, "check", i);
      }
      canvas_.startTween("check");
    }

    for (int i = 0; i < 3; ++i)
    {
      count_exec_.add(2.0 + i * 0.3,
                      [this]()
                      {
                        Arguments args{
                          { "name", std::string("advice") }
                        };
                        event_.signal("UI:sound", args);
                      });
    }

    canvas_.enableWidget("touch");
    std::vector<std::pair<std::string, std::string>> widgets{
      { "touch", "touch:icon" },
    };
    UI::startButtonTween(count_exec_, canvas_, 4.0, 0.2, widgets);
  }


  
  Event<Arguments>& event_;
  ConnectionHolder holder_;

  CountExec count_exec_;

  UI::Canvas canvas_;

  int level_ = 0;

  // Field座標→UI座標へ変換する関数
  std::function<std::vector<glm::vec3> (u_int)> update_;
  std::vector<glm::vec3> indication_positions_; 

  bool pause_  = false;
  bool active_ = true;
};

}
