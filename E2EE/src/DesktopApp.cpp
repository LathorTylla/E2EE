#include "Prerequisites.h"
#include "Client.h"
#include "Server.h"

#include <windows.h>
#include <dwmapi.h>
#include <richedit.h>
#include <uxtheme.h>

#include <algorithm>
#include <atomic>
#include <cwchar>
#include <exception>
#include <memory>
#include <string>
#include <thread>

#pragma comment(lib, "Dwmapi.lib")
#pragma comment(lib, "UxTheme.lib")

namespace {

  // -----------------------------------------------------------------------------
  // Paleta 
  // -----------------------------------------------------------------------------
  constexpr COLORREF kWindow = RGB(22, 24, 29);
  constexpr COLORREF kSidebar = RGB(17, 19, 23);
  constexpr COLORREF kSurface = RGB(31, 34, 40);
  constexpr COLORREF kSurfaceRaised = RGB(38, 42, 49);
  constexpr COLORREF kSurfaceMuted = RGB(46, 50, 58);
  constexpr COLORREF kBorder = RGB(57, 62, 72);

  constexpr COLORREF kText = RGB(238, 240, 243);
  constexpr COLORREF kMuted = RGB(148, 154, 165);

  constexpr COLORREF kAccent = RGB(132, 169, 246);
  constexpr COLORREF kAccentPressed = RGB(111, 148, 226);
  constexpr COLORREF kAccentText = RGB(18, 25, 39);

  constexpr COLORREF kDanger = RGB(224, 137, 144);
  constexpr COLORREF kDangerSurface = RGB(58, 38, 43);

  constexpr COLORREF kIncoming = RGB(155, 181, 235);

  constexpr UINT WM_APP_MESSAGE = WM_APP + 1;
  constexpr UINT WM_APP_STATUS = WM_APP + 2;
  constexpr UINT WM_APP_READY = WM_APP + 3;
  constexpr UINT WM_APP_TYPING = WM_APP + 4;
  constexpr UINT_PTR kTypingTimer = 1;
  constexpr int kMessageCharacterLimit = 4096;

  enum ControlId {
    IdClient = 100,
    IdServer,
    IdAlias,
    IdIp,
    IdPort,
    IdConnect,
    IdHistory,
    IdMessage,
    IdSend,
    IdStatus,
    IdSessionTitle,
    IdSafety,
    IdTyping,
    IdCounter,
    IdAbout
  };

  struct AppState {
    HWND window{};

    HWND clientRadio{};
    HWND serverRadio{};
    HWND aliasEdit{};
    HWND ipEdit{};
    HWND portEdit{};
    HWND connectButton{};
    HWND history{};
    HWND messageEdit{};
    HWND sendButton{};
    HWND status{};
    HWND sessionTitle{};
    HWND safety{};
    HWND typing{};
    HWND counter{};
    HWND aboutButton{};

    HFONT regular{};
    HFONT medium{};
    HFONT title{};
    HFONT mono{};
    HFONT smallFont{};

    HBRUSH windowBrush{};
    HBRUSH sidebarBrush{};
    HBRUSH surfaceBrush{};

    std::unique_ptr<Client> client;
    std::unique_ptr<Server> server;
    std::thread connectionThread;
    std::atomic<bool> closing{ false };
    bool connected{ false };
    bool localTyping{ false };
    std::wstring peerName{ L"Contacto" };
  };

  // -----------------------------------------------------------------------------
  // Conversión de texto
  // -----------------------------------------------------------------------------
  std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) {
      return {};
    }

    const int size = MultiByteToWideChar(
      CP_UTF8,
      0,
      value.data(),
      static_cast<int>(value.size()),
      nullptr,
      0
    );

    std::wstring result(size, L'\0');

    MultiByteToWideChar(
      CP_UTF8,
      0,
      value.data(),
      static_cast<int>(value.size()),
      result.data(),
      size
    );

    return result;
  }

  std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
      return {};
    }

    const int size = WideCharToMultiByte(
      CP_UTF8,
      0,
      value.data(),
      static_cast<int>(value.size()),
      nullptr,
      0,
      nullptr,
      nullptr
    );

    std::string result(size, '\0');

    WideCharToMultiByte(
      CP_UTF8,
      0,
      value.data(),
      static_cast<int>(value.size()),
      result.data(),
      size,
      nullptr,
      nullptr
    );

    return result;
  }

  std::wstring GetText(HWND control) {
    const int length = GetWindowTextLengthW(control);
    std::wstring text(length + 1, L'\0');

    GetWindowTextW(
      control,
      text.data(),
      static_cast<int>(text.size())
    );

    text.resize(length);
    return text;
  }

  void PostOwnedText(HWND window, UINT message, const std::wstring& value) {
    auto* copy = new std::wstring(value);

    if (!PostMessageW(
      window,
      message,
      0,
      reinterpret_cast<LPARAM>(copy))) {
      delete copy;
    }
  }

  // -----------------------------------------------------------------------------
  // Helpers visuales
  // -----------------------------------------------------------------------------
  void SetFont(HWND control, HFONT font) {
    SendMessageW(
      control,
      WM_SETFONT,
      reinterpret_cast<WPARAM>(font),
      TRUE
    );
  }

  HWND MakeControl(
    AppState& app,
    DWORD exStyle,
    const wchar_t* klass,
    const wchar_t* text,
    DWORD style,
    int id
  ) {
    return CreateWindowExW(
      exStyle,
      klass,
      text,
      style | WS_CHILD | WS_VISIBLE,
      0,
      0,
      0,
      0,
      app.window,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
      GetModuleHandleW(nullptr),
      nullptr
    );
  }

  void ApplyRoundedRegion(HWND control, int radius) {
    RECT rect{};
    GetClientRect(control, &rect);

    HRGN region = CreateRoundRectRgn(
      rect.left,
      rect.top,
      rect.right + 1,
      rect.bottom + 1,
      radius,
      radius
    );

    if (!SetWindowRgn(control, region, TRUE)) {
      DeleteObject(region);
    }
  }

  void SetSingleLineEditPadding(HWND edit, int left, int right) {
    SendMessageW(
      edit,
      EM_SETMARGINS,
      EC_LEFTMARGIN | EC_RIGHTMARGIN,
      MAKELPARAM(left, right)
    );
  }

  void SetRichEditPadding(
    HWND edit,
    int left,
    int top,
    int right,
    int bottom
  ) {
    RECT rect{};
    GetClientRect(edit, &rect);

    rect.left += left;
    rect.top += top;
    rect.right -= right;
    rect.bottom -= bottom;

    SendMessageW(
      edit,
      EM_SETRECT,
      0,
      reinterpret_cast<LPARAM>(&rect)
    );
  }

  void DrawControlOutline(
    HDC dc,
    HWND parent,
    HWND control,
    int radius,
    COLORREF color
  ) {
    if (!control || !IsWindowVisible(control)) {
      return;
    }

    RECT rect{};
    GetWindowRect(control, &rect);

    MapWindowPoints(
      HWND_DESKTOP,
      parent,
      reinterpret_cast<POINT*>(&rect),
      2
    );

    InflateRect(&rect, 1, 1);

    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));

    RoundRect(
      dc,
      rect.left,
      rect.top,
      rect.right,
      rect.bottom,
      radius,
      radius
    );

    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
  }

  void DrawSidebarSeparator(HDC dc, int x, int height) {
    HPEN pen = CreatePen(PS_SOLID, 1, kBorder);
    HGDIOBJ oldPen = SelectObject(dc, pen);

    MoveToEx(dc, x, 0, nullptr);
    LineTo(dc, x, height);

    SelectObject(dc, oldPen);
    DeleteObject(pen);
  }

  // -----------------------------------------------------------------------------
  // Historial
  // -----------------------------------------------------------------------------
  void AppendMessage(
    AppState& app,
    const std::wstring& sender,
    const std::wstring& body,
    bool outgoing
  ) {
    const LONG end = GetWindowTextLengthW(app.history);
    SendMessageW(app.history, EM_SETSEL, end, end);

    PARAFORMAT2 paragraph{ sizeof(paragraph) };
    paragraph.dwMask =
      PFM_ALIGNMENT |
      PFM_SPACEAFTER |
      PFM_STARTINDENT |
      PFM_RIGHTINDENT;

    paragraph.wAlignment = outgoing ? PFA_RIGHT : PFA_LEFT;
    paragraph.dySpaceAfter = 180;
    paragraph.dxStartIndent = 180;
    paragraph.dxRightIndent = 180;

    SendMessageW(
      app.history,
      EM_SETPARAFORMAT,
      0,
      reinterpret_cast<LPARAM>(&paragraph)
    );

    CHARFORMAT2W senderFormat{ sizeof(senderFormat) };
    senderFormat.dwMask = CFM_COLOR | CFM_BOLD | CFM_SIZE | CFM_FACE;
    senderFormat.dwEffects = CFE_BOLD;
    senderFormat.crTextColor = outgoing ? kAccent : kIncoming;
    senderFormat.yHeight = 185;
    wcscpy_s(senderFormat.szFaceName, L"Segoe UI Semibold");

    SendMessageW(
      app.history,
      EM_SETCHARFORMAT,
      SCF_SELECTION,
      reinterpret_cast<LPARAM>(&senderFormat)
    );

    SYSTEMTIME now{};
    GetLocalTime(&now);
    wchar_t time[16]{};
    swprintf_s(time, L"%02u:%02u", now.wHour, now.wMinute);
    const std::wstring heading = sender + L"  ·  " + time + L"\r\n";

    SendMessageW(
      app.history,
      EM_REPLACESEL,
      FALSE,
      reinterpret_cast<LPARAM>(heading.c_str())
    );

    CHARFORMAT2W bodyFormat{ sizeof(bodyFormat) };
    bodyFormat.dwMask = CFM_COLOR | CFM_SIZE | CFM_FACE;
    bodyFormat.crTextColor = kText;
    bodyFormat.yHeight = 215;
    wcscpy_s(bodyFormat.szFaceName, L"Segoe UI");

    SendMessageW(
      app.history,
      EM_SETCHARFORMAT,
      SCF_SELECTION,
      reinterpret_cast<LPARAM>(&bodyFormat)
    );

    const std::wstring content = body + L"\r\n\r\n";

    SendMessageW(
      app.history,
      EM_REPLACESEL,
      FALSE,
      reinterpret_cast<LPARAM>(content.c_str())
    );

    SendMessageW(app.history, EM_SCROLLCARET, 0, 0);
  }

  // -----------------------------------------------------------------------------
  // Estado de conexión
  // -----------------------------------------------------------------------------
  std::wstring FriendlyStatus(const std::wstring& status) {
    if (status == L"listening") {
      return L"● Esperando conexión segura";
    }

    if (status == L"client_connected") {
      return L"● Cliente conectado · negociando claves";
    }

    if (status == L"connected") {
      return L"● Conectado · negociando claves";
    }

    if (status == L"securing_session") {
      return L"● Protegiendo la sesión";
    }

    if (status == L"secure") {
      return L"● Sesión cifrada · AES-256-GCM";
    }

    if (status == L"connection_failed") {
      return L"● No fue posible conectar";
    }

    if (status == L"handshake_failed") {
      return L"● Falló el intercambio de claves";
    }

    if (status == L"listen_failed") {
      return L"● No fue posible abrir el puerto";
    }

    if (status == L"connection_closed") {
      return L"● La otra persona se desconectó";
    }

    if (status == L"authentication_failed") {
      return L"● Mensaje alterado o sesión no válida";
    }

    return L"● Desconectado";
  }

  void UpdateConnectionUi(AppState& app, bool connected) {
    app.connected = connected;

    SetWindowTextW(
      app.connectButton,
      connected ? L"Desconectar" : L"Conectar"
    );

    EnableWindow(app.sendButton, connected);
    EnableWindow(app.messageEdit, connected);
    EnableWindow(app.clientRadio, !connected);
    EnableWindow(app.serverRadio, !connected);
    EnableWindow(app.aliasEdit, !connected);

    EnableWindow(
      app.ipEdit,
      !connected &&
      SendMessageW(
        app.clientRadio,
        BM_GETCHECK,
        0,
        0
      ) == BST_CHECKED
    );

    EnableWindow(app.portEdit, !connected);

    InvalidateRect(app.connectButton, nullptr, TRUE);
    InvalidateRect(app.sendButton, nullptr, TRUE);
    InvalidateRect(app.messageEdit, nullptr, TRUE);
    if (!connected) {
      app.localTyping = false;
      KillTimer(app.window, kTypingTimer);
      SetWindowTextW(app.typing, L"");
      SetWindowTextW(app.safety, L"Verificación disponible al conectar");
      SetWindowTextW(app.counter, L"0 / 4096");
    }
  }

  // -----------------------------------------------------------------------------
  // Red
  // -----------------------------------------------------------------------------
  void ConfigureCallbacks(AppState& app, Client& client) {
    client.SetMessageHandler(
      [window = app.window](const std::string& message) {
        PostOwnedText(
          window,
          WM_APP_MESSAGE,
          Utf8ToWide(message)
        );
      }
    );

    client.SetStatusHandler(
      [window = app.window](const std::string& status) {
        PostOwnedText(
          window,
          WM_APP_STATUS,
          Utf8ToWide(status)
        );
      }
    );

    client.SetTypingHandler(
      [window = app.window](bool typing) {
        PostMessageW(window, WM_APP_TYPING, typing ? TRUE : FALSE, 0);
      }
    );
  }

  void ConfigureCallbacks(AppState& app, Server& server) {
    server.SetMessageHandler(
      [window = app.window](const std::string& message) {
        PostOwnedText(
          window,
          WM_APP_MESSAGE,
          Utf8ToWide(message)
        );
      }
    );

    server.SetStatusHandler(
      [window = app.window](const std::string& status) {
        PostOwnedText(
          window,
          WM_APP_STATUS,
          Utf8ToWide(status)
        );
      }
    );

    server.SetTypingHandler(
      [window = app.window](bool typing) {
        PostMessageW(window, WM_APP_TYPING, typing ? TRUE : FALSE, 0);
      }
    );
  }

  void Disconnect(AppState& app) {
    if (app.client) {
      app.client->Disconnect();
    }

    if (app.server) {
      app.server->Disconnect();
    }

    if (
      app.connectionThread.joinable() &&
      app.connectionThread.get_id() != std::this_thread::get_id()
      ) {
      app.connectionThread.join();
    }

    app.client.reset();
    app.server.reset();

    UpdateConnectionUi(app, false);
    SetWindowTextW(app.status, L"● Desconectado");
  }

  void BeginConnection(AppState& app) {
    if (app.connected || app.connectionThread.joinable()) {
      return;
    }

    const bool serverMode =
      SendMessageW(
        app.serverRadio,
        BM_GETCHECK,
        0,
        0
      ) == BST_CHECKED;

    const std::wstring portText = GetText(app.portEdit);

    wchar_t* end = nullptr;
    const long portValue = wcstol(
      portText.c_str(),
      &end,
      10
    );

    if (
      !end ||
      *end != L'\0' ||
      portValue < 1 ||
      portValue > 65535
      ) {
      SetWindowTextW(
        app.status,
        L"● El puerto debe estar entre 1 y 65535"
      );

      return;
    }

    const std::string ip = WideToUtf8(GetText(app.ipEdit));
    const std::string alias = WideToUtf8(GetText(app.aliasEdit));

    if (alias.empty() || alias.size() > 64) {
      SetWindowTextW(app.status, L"● Escribe un nombre de hasta 64 bytes");
      return;
    }

    SetWindowTextW(
      app.status,
      serverMode ? L"● Abriendo servidor…" : L"● Conectando…"
    );

    EnableWindow(app.connectButton, FALSE);

    app.connectionThread = std::thread(
      [&app,
      serverMode,
      port = static_cast<int>(portValue),
      ip,
      alias]() {
        bool ready = false;

        try {
          if (serverMode) {
            app.server = std::make_unique<Server>(port);
            app.server->SetDisplayName(alias);
            ConfigureCallbacks(app, *app.server);

            ready =
              app.server->Start() &&
              app.server->WaitForClient() &&
              app.server->StartReceiving();
          }
          else {
            app.client = std::make_unique<Client>(ip, port);
            app.client->SetDisplayName(alias);
            ConfigureCallbacks(app, *app.client);

            ready =
              app.client->Connect() &&
              app.client->PerformHandshake() &&
              app.client->StartReceiving();
          }
        }
        catch (const std::exception& error) {
          PostOwnedText(
            app.window,
            WM_APP_STATUS,
            L"error: " + Utf8ToWide(error.what())
          );
        }

        if (!app.closing) {
          PostMessageW(
            app.window,
            WM_APP_READY,
            ready,
            0
          );
        }
      }
    );
  }

  void SendCurrentMessage(AppState& app) {
    if (!app.connected) {
      return;
    }

    const std::wstring text = GetText(app.messageEdit);

    if (text.empty()) {
      return;
    }

    const std::string utf8 = WideToUtf8(text);

    const bool sent =
      app.client
      ? app.client->SendEncryptedMessage(utf8)
      : app.server &&
      app.server->SendEncryptedMessage(utf8);

    if (sent) {
      std::wstring alias = GetText(app.aliasEdit);

      AppendMessage(
        app,
        alias.empty() ? L"Tú" : alias,
        text,
        true
      );

      SetWindowTextW(app.messageEdit, L"");
      SetFocus(app.messageEdit);
    }
    else {
      SetWindowTextW(
        app.status,
        L"● No se pudo enviar el mensaje"
      );
    }
  }

  void SendTypingState(AppState& app, bool typing) {
    if (!app.connected || app.localTyping == typing) return;

    const bool sent = app.client
      ? app.client->SendTypingNotification(typing)
      : app.server && app.server->SendTypingNotification(typing);

    if (sent) app.localTyping = typing;
  }

  // -----------------------------------------------------------------------------
  // Layout
  // -----------------------------------------------------------------------------
  void Layout(AppState& app, int width, int height) {
    constexpr int sidebar = 296;
    constexpr int sidePadding = 24;
    constexpr int contentPadding = 32;
    constexpr int buttonWidth = 108;
    constexpr int gap = 12;

    MoveWindow(
      app.clientRadio,
      sidePadding,
      144,
      112,
      30,
      TRUE
    );

    MoveWindow(
      app.serverRadio,
      148,
      144,
      112,
      30,
      TRUE
    );

    MoveWindow(
      app.aliasEdit,
      sidePadding,
      215,
      248,
      42,
      TRUE
    );

    MoveWindow(
      app.ipEdit,
      sidePadding,
      301,
      248,
      42,
      TRUE
    );

    MoveWindow(
      app.portEdit,
      sidePadding,
      387,
      248,
      42,
      TRUE
    );

    MoveWindow(
      app.connectButton,
      sidePadding,
      457,
      248,
      44,
      TRUE
    );

    MoveWindow(app.aboutButton, sidePadding, height - 112, 248, 34, TRUE);

    MoveWindow(
      app.status,
      sidePadding,
      height - 62,
      248,
      30,
      TRUE
    );

    const int contentLeft = sidebar + contentPadding;
    const int contentWidth =
      std::max(
        320,
        width - sidebar - contentPadding * 2
      );

    MoveWindow(
      app.sessionTitle,
      contentLeft,
      18,
      contentWidth,
      36,
      TRUE
    );

    MoveWindow(app.safety, contentLeft, 54, contentWidth, 24, TRUE);

    const int composerY = height - 70;
    const int composerWidth =
      std::max(
        180,
        contentWidth - buttonWidth - gap
      );

    const int historyHeight =
      std::max(
        220,
        composerY - 90 - 42
      );

    MoveWindow(
      app.history,
      contentLeft,
      90,
      contentWidth,
      historyHeight,
      TRUE
    );

    MoveWindow(app.typing, contentLeft + 4, composerY - 28,
               contentWidth - 120, 22, TRUE);
    MoveWindow(app.counter, contentLeft + contentWidth - 110,
               composerY - 28, 110, 22, TRUE);

    MoveWindow(
      app.messageEdit,
      contentLeft,
      composerY,
      composerWidth,
      46,
      TRUE
    );

    MoveWindow(
      app.sendButton,
      contentLeft + composerWidth + gap,
      composerY,
      buttonWidth,
      46,
      TRUE
    );

    ApplyRoundedRegion(app.aliasEdit, 14);
    ApplyRoundedRegion(app.ipEdit, 14);
    ApplyRoundedRegion(app.portEdit, 14);
    ApplyRoundedRegion(app.connectButton, 18);
    ApplyRoundedRegion(app.aboutButton, 14);
    ApplyRoundedRegion(app.history, 20);
    ApplyRoundedRegion(app.messageEdit, 16);
    ApplyRoundedRegion(app.sendButton, 18);

    SetSingleLineEditPadding(app.aliasEdit, 14, 14);
    SetSingleLineEditPadding(app.ipEdit, 14, 14);
    SetSingleLineEditPadding(app.portEdit, 14, 14);
    SetSingleLineEditPadding(app.messageEdit, 16, 16);

    SetRichEditPadding(
      app.history,
      16,
      14,
      16,
      14
    );

    InvalidateRect(app.window, nullptr, TRUE);
  }

  // -----------------------------------------------------------------------------
  // Botones owner-draw
  // -----------------------------------------------------------------------------
  void DrawButton(const DRAWITEMSTRUCT& item) {
    const bool disabled =
      (item.itemState & ODS_DISABLED) != 0;

    const bool pressed =
      (item.itemState & ODS_SELECTED) != 0;

    const bool focused =
      (item.itemState & ODS_FOCUS) != 0;

    const int id = GetDlgCtrlID(item.hwndItem);

    wchar_t text[64]{};
    GetWindowTextW(
      item.hwndItem,
      text,
      64
    );

    const bool isSend = id == IdSend;
    const bool isAbout = id == IdAbout;
    const bool isDisconnect =
      id == IdConnect &&
      wcscmp(text, L"Desconectar") == 0;

    COLORREF fill = kSurfaceRaised;
    COLORREF border = kBorder;
    COLORREF textColor = kText;

    if (disabled) {
      fill = kSurfaceMuted;
      border = kSurfaceMuted;
      textColor = kMuted;
    }
    else if (isSend) {
      fill = pressed ? kAccentPressed : kAccent;
      border = fill;
      textColor = kAccentText;
    }
    else if (isDisconnect) {
      fill = pressed ? RGB(72, 45, 50) : kDangerSurface;
      border = kDanger;
      textColor = kDanger;
    }
    else if (isAbout) {
      fill = pressed ? kSurfaceMuted : kSidebar;
      border = kBorder;
      textColor = kMuted;
    }
    else if (pressed) {
      fill = kSurfaceMuted;
      border = kAccent;
    }

    RECT rect = item.rcItem;
    InflateRect(&rect, -1, -1);

    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(
      PS_SOLID,
      focused ? 2 : 1,
      focused && !disabled ? kAccent : border
    );

    HGDIOBJ oldBrush = SelectObject(item.hDC, brush);
    HGDIOBJ oldPen = SelectObject(item.hDC, pen);

    RoundRect(
      item.hDC,
      rect.left,
      rect.top,
      rect.right,
      rect.bottom,
      18,
      18
    );

    SelectObject(item.hDC, oldBrush);
    SelectObject(item.hDC, oldPen);

    DeleteObject(brush);
    DeleteObject(pen);

    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, textColor);

    HFONT font = reinterpret_cast<HFONT>(
      SendMessageW(item.hwndItem, WM_GETFONT, 0, 0)
      );

    HGDIOBJ oldFont = SelectObject(
      item.hDC,
      font ? font : GetStockObject(DEFAULT_GUI_FONT)
    );

    DrawTextW(
      item.hDC,
      text,
      -1,
      &rect,
      DT_CENTER |
      DT_VCENTER |
      DT_SINGLELINE |
      DT_END_ELLIPSIS
    );

    SelectObject(item.hDC, oldFont);
  }

  // -----------------------------------------------------------------------------
  // Window procedure
  // -----------------------------------------------------------------------------
  LRESULT CALLBACK WindowProc(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
  ) {
    auto* app = reinterpret_cast<AppState*>(
      GetWindowLongPtrW(window, GWLP_USERDATA)
      );

    switch (message) {
    case WM_NCCREATE: {
      auto* create =
        reinterpret_cast<CREATESTRUCTW*>(lParam);

      SetWindowLongPtrW(
        window,
        GWLP_USERDATA,
        reinterpret_cast<LONG_PTR>(
          create->lpCreateParams
          )
      );

      return TRUE;
    }

    case WM_CREATE: {
      app = reinterpret_cast<AppState*>(
        GetWindowLongPtrW(window, GWLP_USERDATA)
        );

      app->window = window;

      app->windowBrush = CreateSolidBrush(kWindow);
      app->sidebarBrush = CreateSolidBrush(kSidebar);
      app->surfaceBrush = CreateSolidBrush(kSurface);

      app->regular = CreateFontW(
        -18,
        0,
        0,
        0,
        FW_NORMAL,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH,
        L"Segoe UI"
      );

      app->medium = CreateFontW(
        -17,
        0,
        0,
        0,
        FW_SEMIBOLD,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH,
        L"Segoe UI Semibold"
      );

      app->title = CreateFontW(
        -29,
        0,
        0,
        0,
        FW_SEMIBOLD,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH,
        L"Segoe UI Variable Display"
      );

      app->mono = CreateFontW(
        -16,
        0,
        0,
        0,
        FW_NORMAL,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        FIXED_PITCH,
        L"Cascadia Mono"
      );

      app->smallFont = CreateFontW(
        -15,
        0,
        0,
        0,
        FW_NORMAL,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH,
        L"Segoe UI"
      );

      app->clientRadio = MakeControl(
        *app,
        0,
        L"BUTTON",
        L"Cliente",
        BS_AUTORADIOBUTTON |
        BS_FLAT |
        WS_GROUP |
        WS_TABSTOP,
        IdClient
      );

      app->serverRadio = MakeControl(
        *app,
        0,
        L"BUTTON",
        L"Servidor",
        BS_AUTORADIOBUTTON |
        BS_FLAT |
        WS_TABSTOP,
        IdServer
      );

      SendMessageW(
        app->clientRadio,
        BM_SETCHECK,
        BST_CHECKED,
        0
      );

      app->aliasEdit = MakeControl(
        *app,
        0,
        L"EDIT",
        L"Fabián",
        ES_AUTOHSCROLL |
        WS_TABSTOP,
        IdAlias
      );

      app->ipEdit = MakeControl(
        *app,
        0,
        L"EDIT",
        L"127.0.0.1",
        ES_AUTOHSCROLL |
        WS_TABSTOP,
        IdIp
      );

      app->portEdit = MakeControl(
        *app,
        0,
        L"EDIT",
        L"12345",
        ES_NUMBER |
        ES_AUTOHSCROLL |
        WS_TABSTOP,
        IdPort
      );

      app->connectButton = MakeControl(
        *app,
        0,
        L"BUTTON",
        L"Conectar",
        BS_OWNERDRAW |
        WS_TABSTOP,
        IdConnect
      );

      app->status = MakeControl(
        *app,
        0,
        L"STATIC",
        L"● Desconectado",
        SS_LEFT |
        SS_CENTERIMAGE,
        IdStatus
      );

      app->aboutButton = MakeControl(
        *app, 0, L"BUTTON", L"Acerca de E2EE",
        BS_OWNERDRAW | WS_TABSTOP, IdAbout
      );

      app->sessionTitle = MakeControl(
        *app,
        0,
        L"STATIC",
        L"Conversación privada",
        SS_LEFT |
        SS_CENTERIMAGE,
        IdSessionTitle
      );

      app->safety = MakeControl(
        *app, 0, L"STATIC", L"Verificación disponible al conectar",
        SS_LEFT | SS_CENTERIMAGE, IdSafety
      );

      app->typing = MakeControl(
        *app, 0, L"STATIC", L"",
        SS_LEFT | SS_CENTERIMAGE, IdTyping
      );

      app->counter = MakeControl(
        *app, 0, L"STATIC", L"0 / 4096",
        SS_RIGHT | SS_CENTERIMAGE, IdCounter
      );

      // Sin WS_EX_CLIENTEDGE para evitar bordes rectos clásicos.
      app->history = MakeControl(
        *app,
        0,
        MSFTEDIT_CLASS,
        L"",
        ES_MULTILINE |
        ES_READONLY |
        ES_AUTOVSCROLL |
        ES_NOHIDESEL |
        WS_VSCROLL,
        IdHistory
      );

      app->messageEdit = MakeControl(
        *app,
        0,
        L"EDIT",
        L"",
        ES_AUTOHSCROLL |
        WS_TABSTOP,
        IdMessage
      );

      app->sendButton = MakeControl(
        *app,
        0,
        L"BUTTON",
        L"Enviar",
        BS_OWNERDRAW |
        WS_TABSTOP,
        IdSend
      );

      for (HWND control : {
        app->clientRadio,
          app->serverRadio,
          app->aliasEdit,
          app->ipEdit,
          app->portEdit,
          app->status,
          app->safety,
          app->typing,
          app->counter,
          app->history,
          app->messageEdit
      }) {
        SetFont(control, app->regular);
      }

      SetFont(app->connectButton, app->medium);
      SetFont(app->aboutButton, app->smallFont);
      SetFont(app->sendButton, app->medium);
      SetFont(app->sessionTitle, app->title);

      SetWindowTheme(
        app->clientRadio,
        L"DarkMode_Explorer",
        nullptr
      );

      SetWindowTheme(
        app->serverRadio,
        L"DarkMode_Explorer",
        nullptr
      );

      SetWindowTheme(
        app->history,
        L"DarkMode_Explorer",
        nullptr
      );

      SetWindowTheme(
        app->messageEdit,
        L"DarkMode_Explorer",
        nullptr
      );

      SendMessageW(
        app->history,
        EM_SETBKGNDCOLOR,
        0,
        kSurface
      );

      SendMessageW(
        app->history,
        EM_SETREADONLY,
        TRUE,
        0
      );

      SendMessageW(
        app->aliasEdit,
        EM_SETCUEBANNER,
        FALSE,
        reinterpret_cast<LPARAM>(L"Tu nombre")
      );

      SendMessageW(
        app->ipEdit,
        EM_SETCUEBANNER,
        FALSE,
        reinterpret_cast<LPARAM>(L"Ej. 127.0.0.1")
      );

      SendMessageW(
        app->portEdit,
        EM_SETCUEBANNER,
        FALSE,
        reinterpret_cast<LPARAM>(L"1–65535")
      );

      SendMessageW(
        app->messageEdit,
        EM_SETCUEBANNER,
        FALSE,
        reinterpret_cast<LPARAM>(L"Escribe un mensaje…")
      );

      SendMessageW(app->messageEdit, EM_LIMITTEXT, kMessageCharacterLimit, 0);

      EnableWindow(app->messageEdit, FALSE);
      EnableWindow(app->sendButton, FALSE);

      AppendMessage(
        *app,
        L"E2EE",
        L"Conecta con otra persona para iniciar una conversación cifrada.",
        false
      );

      return 0;
    }

    case WM_SIZE:
      if (app) {
        Layout(
          *app,
          LOWORD(lParam),
          HIWORD(lParam)
        );
      }

      return 0;

    case WM_GETMINMAXINFO: {
      auto* limits =
        reinterpret_cast<MINMAXINFO*>(lParam);

      limits->ptMinTrackSize = { 900, 620 };
      return 0;
    }

    case WM_PAINT: {
      PAINTSTRUCT paint{};
      HDC dc = BeginPaint(window, &paint);

      RECT client{};
      GetClientRect(window, &client);

      FillRect(dc, &client, app->windowBrush);

      RECT sidebar{
        0,
        0,
        296,
        client.bottom
      };

      FillRect(dc, &sidebar, app->sidebarBrush);
      DrawSidebarSeparator(dc, 295, client.bottom);

      SetBkMode(dc, TRANSPARENT);

      HGDIOBJ previousFont =
        SelectObject(dc, app->title);

      SetTextColor(dc, kText);
      TextOutW(dc, 24, 25, L"E2EE", 4);

      SelectObject(dc, app->smallFont);
      SetTextColor(dc, kMuted);
      TextOutW(
        dc,
        24,
        65,
        L"Mensajería privada cifrada",
        26
      );

      SelectObject(dc, app->medium);
      SetTextColor(dc, kMuted);

      TextOutW(dc, 24, 116, L"Modo", 4);
      TextOutW(dc, 24, 187, L"Nombre", 6);
      TextOutW(dc, 24, 273, L"Dirección IP", 12);
      TextOutW(dc, 24, 359, L"Puerto", 6);

      DrawControlOutline(
        dc,
        window,
        app->aliasEdit,
        14,
        kBorder
      );

      DrawControlOutline(
        dc,
        window,
        app->ipEdit,
        14,
        kBorder
      );

      DrawControlOutline(
        dc,
        window,
        app->portEdit,
        14,
        kBorder
      );

      DrawControlOutline(
        dc,
        window,
        app->history,
        20,
        kBorder
      );

      DrawControlOutline(
        dc,
        window,
        app->messageEdit,
        16,
        IsWindowEnabled(app->messageEdit)
        ? kBorder
        : kSurfaceMuted
      );

      SelectObject(dc, previousFont);
      EndPaint(window, &paint);

      return 0;
    }

    case WM_CTLCOLORSTATIC: {
      HDC dc = reinterpret_cast<HDC>(wParam);
      HWND control = reinterpret_cast<HWND>(lParam);
      const int id = GetDlgCtrlID(control);

      SetBkMode(dc, TRANSPARENT);

      if (
        id == IdAlias ||
        id == IdIp ||
        id == IdPort ||
        id == IdMessage ||
        id == IdHistory
        ) {
        SetTextColor(
          dc,
          IsWindowEnabled(control)
          ? kText
          : kMuted
        );

        SetBkColor(dc, kSurface);

        return reinterpret_cast<LRESULT>(
          app->surfaceBrush
          );
      }

      if (id == IdStatus) {
        SetTextColor(dc, kMuted);

        return reinterpret_cast<LRESULT>(
          app->sidebarBrush
          );
      }

      if (id == IdSafety || id == IdTyping || id == IdCounter) {
        SetTextColor(dc, kMuted);
        return reinterpret_cast<LRESULT>(app->windowBrush);
      }

      if (id == IdClient || id == IdServer) {
        SetTextColor(
          dc,
          IsWindowEnabled(control)
          ? kText
          : kMuted
        );

        return reinterpret_cast<LRESULT>(
          app->sidebarBrush
          );
      }

      SetTextColor(dc, kText);

      return reinterpret_cast<LRESULT>(
        app->windowBrush
        );
    }

    case WM_CTLCOLOREDIT: {
      HDC dc = reinterpret_cast<HDC>(wParam);
      HWND control = reinterpret_cast<HWND>(lParam);

      SetTextColor(
        dc,
        IsWindowEnabled(control)
        ? kText
        : kMuted
      );

      SetBkColor(dc, kSurface);

      return reinterpret_cast<LRESULT>(
        app->surfaceBrush
        );
    }

    case WM_CTLCOLORBTN: {
      HDC dc = reinterpret_cast<HDC>(wParam);

      SetBkMode(dc, TRANSPARENT);
      SetTextColor(dc, kText);

      return reinterpret_cast<LRESULT>(
        app->sidebarBrush
        );
    }

    case WM_DRAWITEM:
      DrawButton(
        *reinterpret_cast<DRAWITEMSTRUCT*>(lParam)
      );

      return TRUE;

    case WM_COMMAND: {
      const int id = LOWORD(wParam);

      if (id == IdServer || id == IdClient) {
        const bool clientMode = id == IdClient;

        EnableWindow(
          app->ipEdit,
          clientMode
        );

        SetWindowTextW(
          app->sessionTitle,
          clientMode
          ? L"Conversación privada"
          : L"Sala privada"
        );

        InvalidateRect(window, nullptr, TRUE);
      }
      else if (
        id == IdConnect &&
        HIWORD(wParam) == BN_CLICKED
        ) {
        if (app->connected) {
          Disconnect(*app);
        }
        else {
          BeginConnection(*app);
        }
      }
      else if (
        id == IdSend &&
        HIWORD(wParam) == BN_CLICKED
        ) {
        SendCurrentMessage(*app);
      }
      else if (id == IdAbout && HIWORD(wParam) == BN_CLICKED) {
        MessageBoxW(
          window,
          L"E2EE Desktop · versión 2\n\n"
          L"Mensajería privada punto a punto para Windows.\n\n"
          L"• AES-256-GCM con autenticación\n"
          L"• RSA-3072 y OAEP-SHA-256\n"
          L"• Protocolo TCP enmarcado\n"
          L"• Código de seguridad verificable\n\n"
          L"Proyecto de portafolio desarrollado en C++ y Win32. "
          L"Compara el código de seguridad por otro canal para verificar la identidad.",
          L"Acerca de E2EE",
          MB_OK | MB_ICONINFORMATION
        );
      }
      else if (
        id == IdMessage &&
        HIWORD(wParam) == EN_UPDATE
        ) {
        EnableWindow(
          app->sendButton,
          app->connected &&
          GetWindowTextLengthW(
            app->messageEdit
          ) > 0
        );

        InvalidateRect(
          app->sendButton,
          nullptr,
          TRUE
        );

        const int length = GetWindowTextLengthW(app->messageEdit);
        wchar_t counter[32]{};
        swprintf_s(counter, L"%d / %d", length, kMessageCharacterLimit);
        SetWindowTextW(app->counter, counter);

        if (length > 0 && app->connected) {
          SendTypingState(*app, true);
          SetTimer(window, kTypingTimer, 850, nullptr);
        }
        else {
          KillTimer(window, kTypingTimer);
          SendTypingState(*app, false);
        }
      }

      return 0;
    }

    case WM_APP_MESSAGE: {
      std::unique_ptr<std::wstring> text(
        reinterpret_cast<std::wstring*>(lParam)
      );

      AppendMessage(
        *app,
        app->peerName,
        *text,
        false
      );

      SetWindowTextW(app->typing, L"");

      return 0;
    }

    case WM_APP_STATUS: {
      std::unique_ptr<std::wstring> text(
        reinterpret_cast<std::wstring*>(lParam)
      );

      SetWindowTextW(
        app->status,
        FriendlyStatus(*text).c_str()
      );

      if (
        *text == L"connection_closed" ||
        *text == L"disconnected"
        ) {
        UpdateConnectionUi(*app, false);
      }

      InvalidateRect(app->status, nullptr, TRUE);
      return 0;
    }

    case WM_APP_READY:
      if (app->connectionThread.joinable()) {
        app->connectionThread.join();
      }

      EnableWindow(app->connectButton, TRUE);
      UpdateConnectionUi(*app, wParam == TRUE);

      if (!wParam) {
        app->client.reset();
        app->server.reset();

        SetWindowTextW(
          app->status,
          L"● No fue posible establecer la sesión"
        );
      }
      else {
        const std::string peer = app->client
          ? app->client->GetPeerName()
          : app->server->GetPeerName();
        const std::string safety = app->client
          ? app->client->GetSessionSafetyNumber()
          : app->server->GetSessionSafetyNumber();

        app->peerName = Utf8ToWide(peer);
        SetWindowTextW(app->sessionTitle, (L"Chat con " + app->peerName).c_str());
        SetWindowTextW(
          app->safety,
          (L"Código de seguridad  ·  " + Utf8ToWide(safety)).c_str()
        );
        SetFocus(app->messageEdit);
      }

      return 0;

    case WM_APP_TYPING:
      SetWindowTextW(
        app->typing,
        wParam == TRUE ? (app->peerName + L" está escribiendo…").c_str() : L""
      );
      return 0;

    case WM_TIMER:
      if (wParam == kTypingTimer) {
        KillTimer(window, kTypingTimer);
        SendTypingState(*app, false);
        return 0;
      }
      break;

    case WM_KEYDOWN:
      if (
        wParam == VK_RETURN &&
        GetFocus() == app->messageEdit
        ) {
        SendCurrentMessage(*app);
        return 0;
      }

      break;

    case WM_CLOSE:
      app->closing = true;
      Disconnect(*app);
      DestroyWindow(window);
      return 0;

    case WM_DESTROY:
      DeleteObject(app->regular);
      DeleteObject(app->medium);
      DeleteObject(app->title);
      DeleteObject(app->mono);
      DeleteObject(app->smallFont);

      DeleteObject(app->windowBrush);
      DeleteObject(app->sidebarBrush);
      DeleteObject(app->surfaceBrush);

      PostQuitMessage(0);
      return 0;
    }

    return DefWindowProcW(
      window,
      message,
      wParam,
      lParam
    );
  }

} // namespace

int WINAPI wWinMain(
  HINSTANCE instance,
  HINSTANCE,
  PWSTR,
  int showCommand
) {
  LoadLibraryW(L"Msftedit.dll");

  SetProcessDpiAwarenessContext(
    DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
  );

  AppState app;

  WNDCLASSEXW windowClass{
    sizeof(windowClass)
  };

  windowClass.style = CS_HREDRAW | CS_VREDRAW;
  windowClass.lpfnWndProc = WindowProc;
  windowClass.hInstance = instance;
  windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  windowClass.hbrBackground = nullptr;
  windowClass.lpszClassName = L"E2EE.Desktop.Window";

  if (!RegisterClassExW(&windowClass)) {
    return 1;
  }

  HWND window = CreateWindowExW(
    0,
    windowClass.lpszClassName,
    L"E2EE · Private Desktop Chat",
    WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    1120,
    720,
    nullptr,
    nullptr,
    instance,
    &app
  );

  if (!window) {
    return 1;
  }

  // Barra de título oscura.
  const BOOL dark = TRUE;

  DwmSetWindowAttribute(
    window,
    20,
    &dark,
    sizeof(dark)
  );

  // Bordes redondeados de Windows 11.
  // 33 = DWMWA_WINDOW_CORNER_PREFERENCE
  // 2  = DWMWCP_ROUND
  const DWORD cornerPreference = 2;

  DwmSetWindowAttribute(
    window,
    33,
    &cornerPreference,
    sizeof(cornerPreference)
  );

  ShowWindow(window, showCommand);
  UpdateWindow(window);

  MSG message{};

  while (
    GetMessageW(
      &message,
      nullptr,
      0,
      0
    ) > 0
    ) {
    if (
      message.message == WM_KEYDOWN &&
      message.wParam == VK_RETURN &&
      message.hwnd == app.messageEdit
      ) {
      SendCurrentMessage(app);
      continue;
    }

    TranslateMessage(&message);
    DispatchMessageW(&message);
  }

  return static_cast<int>(message.wParam);
}
