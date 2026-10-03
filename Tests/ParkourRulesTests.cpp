// ParkourRules.h のネイティブテスト（UE不要）
//   g++ -std=c++20 -Wall -Wextra -I Source/Parquele/Public Tests/ParkourRulesTests.cpp -o build/rules_tests && build/rules_tests
#include "ParkourRules.h"

#include <cmath>
#include <cstdio>
#include <limits>

using namespace ParkourRules;

static int Failures = 0;

#define CHECK(Cond)                                                   \
	do                                                                \
	{                                                                 \
		if (!(Cond))                                                  \
		{                                                             \
			std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #Cond); \
			++Failures;                                               \
		}                                                             \
	} while (0)

static bool Near(float A, float B, float Eps = 0.01f) { return std::fabs(A - B) <= Eps; }

static void TestClassifyObstacle()
{
	const FObstacleThresholds T;
	CHECK(ClassifyObstacle(40.f, 60.f, T) == EObstacleAction::None);       // 段差扱い
	CHECK(ClassifyObstacle(50.f, 60.f, T) == EObstacleAction::Vault);
	CHECK(ClassifyObstacle(100.f, 60.f, T) == EObstacleAction::Vault);     // 標準ヴォルト障害物
	CHECK(ClassifyObstacle(120.f, 99.f, T) == EObstacleAction::Vault);
	CHECK(ClassifyObstacle(100.f, 100.f, T) == EObstacleAction::Mantle);   // 奥行き100以上は上に乗る
	CHECK(ClassifyObstacle(121.f, 10.f, T) == EObstacleAction::Mantle);
	CHECK(ClassifyObstacle(200.f, 400.f, T) == EObstacleAction::Mantle);   // 標準マントル壁
	CHECK(ClassifyObstacle(300.f, 600.f, T) == EObstacleAction::WallClimb); // 標準クライム壁
	CHECK(ClassifyObstacle(320.f, 600.f, T) == EObstacleAction::WallClimb);
	CHECK(ClassifyObstacle(321.f, 600.f, T) == EObstacleAction::None);
	CHECK(ClassifyObstacle(std::numeric_limits<float>::infinity(), 0.f, T) == EObstacleAction::None);
	CHECK(ClassifyObstacle(std::nanf(""), 0.f, T) == EObstacleAction::None);
}

static void TestAngleToWall()
{
	CHECK(Near(AngleToWallDeg(-1.f), 90.f));
	CHECK(Near(AngleToWallDeg(0.f), 0.f));
	CHECK(Near(AngleToWallDeg(-0.5f), 30.f));
	CHECK(Near(AngleToWallDeg(0.7f), 0.f)); // 壁から離れている
}

static void TestChooseWallAction()
{
	const FObstacleThresholds O;
	const FWallRunThresholds W;
	const float Inf = std::numeric_limits<float>::infinity();
	CHECK(ChooseWallAction(100.f, 60.f, 90.f, 650.f, O, W) == EWallAction::Vault);
	CHECK(ChooseWallAction(100.f, 60.f, 52.f, 650.f, O, W) == EWallAction::Vault);   // 補正10°以内
	CHECK(ChooseWallAction(100.f, 60.f, 45.f, 650.f, O, W) == EWallAction::None);    // 斜めすぎ
	CHECK(ChooseWallAction(200.f, 400.f, 80.f, 0.f, O, W) == EWallAction::Mantle);   // 静止からでも可
	CHECK(ChooseWallAction(300.f, 600.f, 85.f, 650.f, O, W) == EWallAction::WallClimb);
	CHECK(ChooseWallAction(300.f, 600.f, 45.f, 650.f, O, W) == EWallAction::WallRun); // 正対していなければ壁走り
	CHECK(ChooseWallAction(Inf, 0.f, 30.f, 650.f, O, W) == EWallAction::WallRun);
	CHECK(ChooseWallAction(Inf, 0.f, 60.f, 500.f, O, W) == EWallAction::WallRun);
	CHECK(ChooseWallAction(Inf, 0.f, 8.f, 650.f, O, W) == EWallAction::WallRun);     // 15°-補正10°
	CHECK(ChooseWallAction(Inf, 0.f, 4.f, 650.f, O, W) == EWallAction::None);
	CHECK(ChooseWallAction(Inf, 0.f, 30.f, 499.f, O, W) == EWallAction::None);       // 速度不足
	CHECK(ChooseWallAction(Inf, 0.f, 75.f, 900.f, O, W) == EWallAction::None);       // 登れない壁に正対
	CHECK(ChooseWallAction(150.f, 50.f, 30.f, 900.f, O, W) == EWallAction::None);    // 低い壁は壁走り不可
}

static void TestLanding()
{
	const FLandingThresholds T;
	CHECK(EvaluateLanding(399.f, -1.f, T) == ELandingResult::Normal);
	CHECK(EvaluateLanding(500.f, 0.1f, T) == ELandingResult::Roll);
	CHECK(EvaluateLanding(500.f, 0.f, T) == ELandingResult::Roll);
	CHECK(EvaluateLanding(500.f, 0.25f, T) == ELandingResult::Roll);
	CHECK(EvaluateLanding(500.f, 0.26f, T) == ELandingResult::Stagger);
	CHECK(EvaluateLanding(500.f, 1e9f, T) == ELandingResult::Stagger);
}

static void TestStickAndSpeed()
{
	CHECK(EvaluateStick(0.1f, 0.15f, 0.6f).InputScale == 0.f);
	CHECK(!EvaluateStick(0.59f, 0.15f, 0.6f).bRun);
	CHECK(Near(EvaluateStick(0.375f, 0.15f, 0.6f).InputScale, 0.5f));
	CHECK(EvaluateStick(0.6f, 0.15f, 0.6f).bRun);
	CHECK(EvaluateStick(1.f, 0.15f, 0.6f).InputScale == 1.f);

	CHECK(Near(MaxGroundSpeed(false, 1.f, 0.f, 350.f, 650.f, 900.f), 350.f));
	CHECK(Near(MaxGroundSpeed(true, 0.f, 0.f, 350.f, 650.f, 900.f), 650.f));
	CHECK(Near(MaxGroundSpeed(true, 1.f, 0.f, 350.f, 650.f, 900.f), 900.f));
	CHECK(Near(MaxGroundSpeed(true, 1.f, 0.15f, 350.f, 650.f, 900.f), 1035.f));

	CHECK(Near(StepSprintAlpha(0.f, true, 1.f, 2.f), 0.5f));
	CHECK(Near(StepSprintAlpha(0.5f, false, 0.5f, 2.f), 0.f));
	CHECK(StepSprintAlpha(0.9f, true, 1.f, 2.f) == 1.f);
}

static void TestJumpApex()
{
	// 初速 v、重力 g×倍率 の最高到達点 v²/(2g') が 120cm になること
	const float G = 980.f, Scale = 1.6f;
	const float V = JumpVelocityForApex(120.f, G, Scale);
	CHECK(Near(V * V / (2.f * G * Scale), 120.f, 0.1f));
}

static void TestTimedWindow()
{
	FTimedWindow W;
	CHECK(!W.IsOpen());
	W.Open(0.12f);
	W.Tick(0.1f);
	CHECK(W.IsOpen());
	W.Tick(0.03f);
	CHECK(!W.IsOpen());
	W.Open(0.15f);
	CHECK(W.Consume());
	CHECK(!W.Consume());
}

int main()
{
	TestClassifyObstacle();
	TestAngleToWall();
	TestChooseWallAction();
	TestLanding();
	TestStickAndSpeed();
	TestJumpApex();
	TestTimedWindow();
	if (Failures == 0) std::printf("All ParkourRules tests passed\n");
	return Failures == 0 ? 0 : 1;
}
