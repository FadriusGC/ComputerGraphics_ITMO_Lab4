#include "D3DWindow.h"

#include "Common.h"

bool D3DWindow::Initialize(HINSTANCE hInstance, int width, int height,
                           const wchar_t* title) {
  m_width = width;
  m_height = height;

  WNDCLASS wc = {};
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = StaticWindowProc;
  wc.hInstance = hInstance;
  wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  wc.lpszMenuName = nullptr;
  wc.lpszClassName = L"D3D12WindowClass";

  if (!RegisterClass(&wc)) {
    MessageBox(nullptr, L"Failed to register window class", L"Error",
               MB_OK | MB_ICONERROR);
    return false;
  }

  RECT rect = {0, 0, m_width, m_height};
  AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

  m_hWnd =
      CreateWindow(L"D3D12WindowClass", title, WS_OVERLAPPEDWINDOW,
                   CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left,
                   rect.bottom - rect.top, nullptr, nullptr, hInstance, this);

  if (!m_hWnd) {
    MessageBox(nullptr, L"Failed to create window", L"Error",
               MB_OK | MB_ICONERROR);
    return false;
  }

  return true;
}

LRESULT CALLBACK D3DWindow::StaticWindowProc(HWND hWnd, UINT msg, WPARAM wParam,
                                             LPARAM lParam) {
  D3DWindow* pWindow = nullptr;

  if (msg == WM_NCCREATE) {
    CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
    pWindow = reinterpret_cast<D3DWindow*>(pCreate->lpCreateParams);
    SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pWindow));
    if (pWindow) {
      pWindow->m_hWnd = hWnd;
    }
  } else {
    pWindow =
        reinterpret_cast<D3DWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
  }

  if (pWindow) {
    return pWindow->HandleMessage(msg, wParam, lParam);
  }

  return DefWindowProc(hWnd, msg, wParam, lParam);
}

LRESULT D3DWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_ACTIVATE:
      if (LOWORD(wParam) == WA_INACTIVE) {
        m_appPaused = true;
      } else {
        m_appPaused = false;
      }
      return 0;

    case WM_SIZE:
      m_width = LOWORD(lParam);
      m_height = HIWORD(lParam);

      if (wParam == SIZE_MINIMIZED) {
        m_minimized = true;
        m_maximized = false;
        m_appPaused = true;
      } else if (wParam == SIZE_MAXIMIZED) {
        m_minimized = false;
        m_maximized = true;
        m_appPaused = false;
      } else if (wParam == SIZE_RESTORED) {
        if (m_minimized) {
          m_minimized = false;
          m_appPaused = false;
        } else if (m_maximized) {
          m_maximized = false;
          m_appPaused = false;
        }
      }
      return 0;

    case WM_ENTERSIZEMOVE:
      m_resizing = true;
      m_appPaused = true;
      return 0;

    case WM_EXITSIZEMOVE:
      m_resizing = false;
      m_appPaused = false;
      return 0;

    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;

    case WM_GETMINMAXINFO:
      ((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
      ((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
      return 0;

    case WM_PAINT:
      PAINTSTRUCT ps;
      BeginPaint(m_hWnd, &ps);
      EndPaint(m_hWnd, &ps);
      return 0;
  }

  return DefWindowProc(m_hWnd, msg, wParam, lParam);
}
