#include "BoxApp.h"

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE prevInstance,
                   _In_ LPSTR cmdLine, _In_ int showCmd) {
  (void)prevInstance;
  (void)cmdLine;

#if defined(DEBUG) || defined(_DEBUG)
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

  try {
    BoxApp app(hInstance);
    if (!app.Initialize()) {
      return 1;
    }

    ShowWindow(app.GetMainWnd(), showCmd);
    UpdateWindow(app.GetMainWnd());

    return app.Run();

  } catch (const std::exception& e) {
    MessageBoxA(nullptr, e.what(), "Error", MB_OK | MB_ICONERROR);
    return 1;
  }
}
