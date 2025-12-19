#pragma once

class GameTimer {
 public:
  GameTimer();

  float TotalTime() const;  // в секундах
  float DeltaTime() const;  // в секундах

  void Reset();  // Вызывать перед циклом сообщений
  void Start();  // Вызывать при возобновлении
  void Stop();   // Вызывать при паузе
  void Tick();   // Вызывать каждый кадр

 private:
  double mSecondsPerCount;
  double mDeltaTime;

  __int64 mBaseTime;
  __int64 mPausedTime;
  __int64 mStopTime;
  __int64 mPrevTime;
  __int64 mCurrTime;

  bool mStopped;
};
