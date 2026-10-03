// パルクール判定の純粋ロジック。UEに依存しないので Tests/ でネイティブにテストできる。
// 数値の意味は仕様書「ムーブメント仕様」「手触り・爽快感の設計」を参照。単位は cm / 秒 / 度。
#pragma once

#include <algorithm>
#include <cmath>

namespace ParkourRules
{
	enum class EObstacleAction : unsigned char { None, Vault, Mantle, WallClimb };
	enum class EWallAction : unsigned char { None, Vault, Mantle, WallClimb, WallRun };
	enum class ELandingResult : unsigned char { Normal, Roll, Stagger };

	struct FObstacleThresholds
	{
		float VaultMinHeight = 50.f;
		float VaultMaxHeight = 120.f;
		float VaultMaxDepth = 100.f;
		float MantleMaxHeight = 220.f;
		float WallClimbMaxHeight = 320.f;
	};

	struct FWallRunThresholds
	{
		float MinAngleDeg = 15.f;
		float MaxAngleDeg = 60.f;
		float MinSpeed = 500.f;
		/** 進行方向補正の上限。進入角がこの分だけ外れていても補正して発動する */
		float AssistDeg = 10.f;
	};

	struct FLandingThresholds
	{
		float RollMinFallHeight = 400.f;
		float RollInputWindow = 0.25f;
	};

	/** 障害物の高さ・奥行き（足元基準）から正面アクションを決める。高さが NaN/無限大なら None */
	inline EObstacleAction ClassifyObstacle(float Height, float Depth, const FObstacleThresholds& T)
	{
		if (!(Height >= T.VaultMinHeight)) return EObstacleAction::None;
		if (Height <= T.VaultMaxHeight) return Depth < T.VaultMaxDepth ? EObstacleAction::Vault : EObstacleAction::Mantle;
		if (Height <= T.MantleMaxHeight) return EObstacleAction::Mantle;
		if (Height <= T.WallClimbMaxHeight) return EObstacleAction::WallClimb;
		return EObstacleAction::None;
	}

	/**
	 * 進行方向と壁面のなす角（0°=壁と平行、90°=正対）。
	 * DirDotNormal は水平進行方向（正規化）と壁の法線（壁から外向き）の内積。壁から離れる向きなら 0。
	 */
	inline float AngleToWallDeg(float DirDotNormal)
	{
		const float Into = std::clamp(-DirDotNormal, 0.f, 1.f);
		return std::asin(Into) * 57.29577951f;
	}

	/** 上アクション入力時、前方の壁に対して何をするか */
	inline EWallAction ChooseWallAction(float Height, float Depth, float AngleDeg, float Speed,
		const FObstacleThresholds& O, const FWallRunThresholds& W)
	{
		const EObstacleAction Obstacle = ClassifyObstacle(Height, Depth, O);
		const bool bLowObstacle = Obstacle == EObstacleAction::Vault || Obstacle == EObstacleAction::Mantle;
		// 正対：ウォールラン上限より深い角度。ヴォルト・マントルは補正幅ぶん浅くても正面扱い
		const bool bFront = AngleDeg > W.MaxAngleDeg || (bLowObstacle && AngleDeg >= W.MaxAngleDeg - W.AssistDeg);
		if (bFront)
		{
			switch (Obstacle)
			{
			case EObstacleAction::Vault: return EWallAction::Vault;
			case EObstacleAction::Mantle: return EWallAction::Mantle;
			case EObstacleAction::WallClimb: return EWallAction::WallClimb;
			default: return EWallAction::None;
			}
		}
		// マントルできない高さの壁だけをウォールラン対象にする（高さ不明＝無限大も含む）
		const bool bTallWall = !(Height <= O.MantleMaxHeight);
		if (bTallWall && AngleDeg >= W.MinAngleDeg - W.AssistDeg && AngleDeg <= W.MaxAngleDeg && Speed >= W.MinSpeed)
		{
			return EWallAction::WallRun;
		}
		return EWallAction::None;
	}

	/** 高所からの着地判定。SecondsSinceDownPress は着地時点で下アクションを押してからの経過秒 */
	inline ELandingResult EvaluateLanding(float FallHeight, float SecondsSinceDownPress, const FLandingThresholds& T)
	{
		if (FallHeight < T.RollMinFallHeight) return ELandingResult::Normal;
		if (SecondsSinceDownPress >= 0.f && SecondsSinceDownPress <= T.RollInputWindow) return ELandingResult::Roll;
		return ELandingResult::Stagger;
	}

	struct FStickResponse
	{
		/** AddMovementInput に渡す倍率 0〜1 */
		float InputScale = 0.f;
		bool bRun = false;
	};

	/** スティックの倒し量（0〜1）を歩き／走りに振り分ける。歩き域はデッドゾーン〜走り閾値を 0〜1 に再マップ */
	inline FStickResponse EvaluateStick(float Raw, float DeadZone, float RunThreshold)
	{
		if (Raw < DeadZone) return {};
		if (Raw >= RunThreshold) return { 1.f, true };
		return { (Raw - DeadZone) / (RunThreshold - DeadZone), false };
	}

	/** 地上の最高速度。走行中は SprintAlpha（0〜1）に応じて走行速度→最高速度へ。TopBonus はフローによる上限上げ（0.15=+15%） */
	inline float MaxGroundSpeed(bool bRun, float SprintAlpha, float TopBonus, float Walk, float Run, float Top)
	{
		if (!bRun) return Walk;
		const float Alpha = std::clamp(SprintAlpha, 0.f, 1.f);
		return Run + (Top * (1.f + TopBonus) - Run) * Alpha;
	}

	/** 自動加速の進行度を更新。走り続けていれば RampTime 秒で 1、途切れたら倍速で戻る */
	inline float StepSprintAlpha(float Alpha, bool bSustainingRun, float DeltaTime, float RampTime)
	{
		if (RampTime <= 0.f) return bSustainingRun ? 1.f : 0.f;
		const float Rate = DeltaTime / RampTime;
		return std::clamp(bSustainingRun ? Alpha + Rate : Alpha - Rate * 2.f, 0.f, 1.f);
	}

	/** 最高到達点 Apex を得る初速。上昇中の重力倍率を考慮する */
	inline float JumpVelocityForApex(float Apex, float GravityAbs, float RisingGravityScale)
	{
		return std::sqrt(2.f * Apex * GravityAbs * RisingGravityScale);
	}

	/** コヨーテタイム・先行入力用の時間窓 */
	class FTimedWindow
	{
	public:
		void Open(float Duration) { Remaining = Duration; }
		void Close() { Remaining = 0.f; }
		void Tick(float DeltaTime) { Remaining = std::max(0.f, Remaining - DeltaTime); }
		bool IsOpen() const { return Remaining > 0.f; }
		bool Consume()
		{
			const bool bWasOpen = IsOpen();
			Remaining = 0.f;
			return bWasOpen;
		}

	private:
		float Remaining = 0.f;
	};
}
