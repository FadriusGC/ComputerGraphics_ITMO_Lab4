#pragma once

#include <windows.h>

#include <string>

#include "Common.h"

class D3DWindow {
 public:
  D3DWindow() : m_hWnd(nullptr), m_width(WIDTH), m_height(HEIGHT) {}
  ~D3DWindow() = default;

  bool Initialize(HINSTANCE hInstance, int width, int height,
                  const wchar_t* title);
  HWND GetHWND() const { return m_hWnd; }
  int GetWidth() const { return m_width; }
  int GetHeight() const { return m_height; }
  bool IsPaused() const { return m_appPaused || m_minimized || m_resizing; }
  bool IsResizing() const { return m_resizing; }
  void SetPaused(bool paused) { m_appPaused = paused; }

 private:
  static LRESULT CALLBACK StaticWindowProc(HWND hWnd, UINT msg, WPARAM wParam,
                                           LPARAM lParam);
  LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

  HWND m_hWnd;
  int m_width;
  int m_height;

  bool m_appPaused = false;
  bool m_minimized = false;
  bool m_maximized = false;
  bool m_resizing = false;
};
