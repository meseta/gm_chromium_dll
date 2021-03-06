#include <iostream>

#include <cef_app.h>
#include <cef_client.h>
#include <wrapper/cef_helpers.h>

#include "dllapi.h"

class RenderHandler : public CefRenderHandler {
public:
	RenderHandler(int w, int h, unsigned char * buff) : width(w), height(h), buffptr(buff) {
	}

	~RenderHandler() {
		buffptr = nullptr;
	}

	bool GetViewRect(CefRefPtr<CefBrowser> browser, CefRect &rect) {
		rect = CefRect(0, 0, width, height);
		return true;
	}

	void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList &dirtyRects, const void * buffer, int w, int h) {
		std::cout << "Chromium: Paint" << std::endl;
		if (buffptr && type == PET_VIEW) {
			memcpy(buffptr, buffer, w*h * 4);
			debug_paintcount = (int)&browser;
			new_paint = true;
		}
	}

	void OnCursorChange(CefRefPtr<CefBrowser> browser, CefCursorHandle cursor, CursorType type, const CefCursorInfo& custom_cursor_info) {
		cursor_type = type;
	}
	void resize(int w, int h) {
		width = w;
		height = h;
	}
	int get_debug(void) {
		return debug_paintcount;
	}
	bool has_newpaint(void) {
		return new_paint;
	}
	CursorType get_cursor(void) {
		return cursor_type;
	}

private:
	int width;
	int height;
	bool resize_request = false;
	bool new_paint = false;
	unsigned char * buffptr = nullptr;
	int debug_paintcount = 0;
	CursorType cursor_type = CT_POINTER;

	IMPLEMENT_REFCOUNTING(RenderHandler);
};

// for manual render handler
class BrowserClient : public CefClient, public CefLifeSpanHandler, public CefLoadHandler {
public:
	BrowserClient(CefRefPtr<CefRenderHandler> ptr) : handler(ptr) {
	}

	virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() {
		return this;
	}

	virtual CefRefPtr<CefLoadHandler> GetLoadHandler() {
		return this;
	}

	virtual CefRefPtr<CefRenderHandler> GetRenderHandler() {
		return handler;
	}

	// CefLifeSpanHandler methods.
	void OnAfterCreated(CefRefPtr<CefBrowser> browser) {
		// Must be executed on the UI thread.
		CEF_REQUIRE_UI_THREAD();

		browser_id = browser->GetIdentifier();
	}

	bool DoClose(CefRefPtr<CefBrowser> browser) {
		// Must be executed on the UI thread.
		CEF_REQUIRE_UI_THREAD();

		// Closing the main window requires special handling. See the DoClose()
		// documentation in the CEF header for a detailed description of this
		// process.
		if (browser->GetIdentifier() == browser_id)
		{
			// Set a flag to indicate that the window close should be allowed.
			closing = true;
		}

		// Allow the close. For windowed browsers this will result in the OS close
		// event being sent.
		return false;
	}

	void OnBeforeClose(CefRefPtr<CefBrowser> browser) {
	}

	void OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode) {
		std::cout << "Chromium: LoadEnd:" << httpStatusCode << std::endl;
		last_http_code = httpStatusCode;
		loaded = true;
	}

	bool OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefLoadHandler::ErrorCode errorCode, const CefString & failedUrl, CefString & errorText) {
		std::cout << "Chromium: LoadError" << std::endl;
		loaded = true;
	}

	void OnLoadingStateChange(CefRefPtr<CefBrowser> browser, bool isLoading, bool canGoBack, bool canGoForward) {
		std::cout << "Chromium: LoadChange" << std::endl;
		can_back = canGoBack;
		can_forward = canGoForward;
		if (isLoading) loaded = false;
		else loaded = true;
	}

	void OnLoadStart(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame) {
		std::cout << "Chromium: LoadStart" << std::endl;
		loaded = false;
		last_http_code = 0;
	}

	bool closeAllowed() const {
		return closing;
	}

	bool isLoaded() const {
		return loaded;
	}

	bool canBack() const {
		return can_back;
	}

	bool canForward() const {
		return can_forward;
	}

	int lastHttpCode() const {
		return last_http_code;
	}

private:
	int browser_id;
	bool closing = false;
	bool loaded = false;
	bool can_back = false;
	bool can_forward = false;
	int last_http_code = 0;
	CefRefPtr<CefRenderHandler> handler;

	IMPLEMENT_REFCOUNTING(BrowserClient);
};

class StringVisitor : public CefStringVisitor {
public:
	StringVisitor() {}
	virtual void Visit(const CefString& string) OVERRIDE {
		value.assign(string);
		visited = true;
		std::cout << "Chromium: StringVisit " << value.length() << std::endl;
	}

	bool has_value() {
		return visited;
	}

	const char * get_value(void) {
		return value.c_str();
	}

	void reset(void) {
		visited = false;
	}

private:
	bool visited = false;
	std::string value;

	IMPLEMENT_REFCOUNTING(StringVisitor);
};


class ChromiumApp : public CefApp, public CefBrowserProcessHandler, public CefRenderProcessHandler {
public:
	ChromiumApp() {}

	virtual CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() OVERRIDE {
		return this;
	}
	virtual CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() OVERRIDE {
		return this;
	}
	
	virtual void OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context);

	void store_value(std::string in) {
		value.assign(in);
		visited = true;
	}

	const char * get_value(void) {
		return value.c_str();
	}

	bool has_value(void) {
		return visited;
	}

	void reset(void) {
		visited = false;
	}

private:
	std::string value;
	bool visited = false;

	IMPLEMENT_REFCOUNTING(ChromiumApp);
};

class BoundV8Handler : public CefV8Handler {
public:
	BoundV8Handler(ChromiumApp * p) : localapp(p) {}

	virtual bool Execute(const CefString& name,
		CefRefPtr<CefV8Value> object,
		const CefV8ValueList& arguments,
		CefRefPtr<CefV8Value>& retval,
		CefString& exception) OVERRIDE {

		if (name == "transfer") {
			// Return my string value.
			retval = CefV8Value::CreateBool(true);
			localapp->store_value(std::string(arguments[0]->GetStringValue()));
			std::cout << "Chromium: transfer " << std::string(arguments[0]->GetStringValue()) << std::endl;
			return true;
		}

		// Function does not exist.
		return false;
	}
private:
	ChromiumApp * localapp;
	// Provide the reference counting implementation for this class.
	IMPLEMENT_REFCOUNTING(BoundV8Handler);
};

void ChromiumApp::OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context) {
	CefRefPtr<CefV8Value> object = context->GetGlobal();
	CefRefPtr<CefV8Handler> handler = new BoundV8Handler(this);
	CefRefPtr<CefV8Value> func = CefV8Value::CreateFunction("transfer", handler);
	object->SetValue("transfer", func, V8_PROPERTY_ATTRIBUTE_NONE);

	std::cout << "Chromium: bound transfer function" << std::endl;
}

CefRefPtr<BrowserClient> browserClient = nullptr;
CefRefPtr<RenderHandler> renderHandler = nullptr;
CefRefPtr<CefBrowser> browser = nullptr;
CefRefPtr<ChromiumApp> app = new ChromiumApp();
CefRefPtr<StringVisitor> visitor = new StringVisitor();

int init(int width, int height, int fps, unsigned char * buffptr, bool single) {
	CefMainArgs args;
	if (CefExecuteProcess(args, nullptr, nullptr) >= 0) {
		return -1;
	}

	CefSettings settings;
	settings.single_process = single; // NOTE: this is borken for V8 stuff
	settings.command_line_args_disabled = true;
	settings.windowless_rendering_enabled = true;
	settings.log_severity = LOGSEVERITY_ERROR;

	if (!CefInitialize(args, settings, app.get(), nullptr)) {
		return -1;
	}

	renderHandler = new RenderHandler(width, height, buffptr);

	CefWindowInfo window_info;
	CefBrowserSettings browserSettings;
	browserSettings.background_color = 0x00ffffff;
	browserSettings.windowless_frame_rate = fps;
	window_info.SetAsWindowless(NULL);
	browserClient = new BrowserClient(renderHandler);
	browser = CefBrowserHost::CreateBrowserSync(window_info, browserClient.get(), "about:blank", browserSettings, nullptr);
	browser->GetHost()->SetFocus(true);
	std::cout << "Chromium: Initialized" << std::endl;
	return 0;
}

int loop(void) {
	if (browserClient->closeAllowed()) {
		return -1;
	}

	CefDoMessageLoopWork();
	return 0;
}

void close(void) {
	browser->GetHost()->CloseBrowser(false);
	CefDoMessageLoopWork();

	browser = nullptr;
	browserClient = nullptr;
	renderHandler = nullptr;

	CefShutdown();
	std::cout << "Chromium: End" << std::endl;
}

extern "C" {
	DLL_API double chromium_create(double width, double height, double fps, unsigned char * ptr);
	DLL_API double chromium_create_test(double width, double height, double fps, unsigned char * ptr);
	DLL_API double chromium_step(void);
	DLL_API double chromium_cleanup(void);

	DLL_API double chromium_set_url(const char * url);
	DLL_API double chromium_set_string(const char * str, const char * url);
	DLL_API const char * chromium_get_url();

	DLL_API double chromium_execute(const char * js);

	DLL_API double chromium_stop(void);
	DLL_API double chromium_reload(void);
	DLL_API double chromium_back(void);
	DLL_API double chromium_forward(void);

	DLL_API double chromium_resize(double width, double height);

	DLL_API double chromium_get_cursor(void);
	DLL_API double chromium_get_isloaded(void);
	DLL_API double chromium_get_can_back(void);
	DLL_API double chromium_get_can_forward(void);
	DLL_API double chromium_get_last_http_code(void);
	DLL_API double chromium_get_debug_value(double value);

	DLL_API double chromium_event_mousemove(double x, double y);
	DLL_API double chromium_event_mousewheel(double x, double y);
	DLL_API double chromium_event_mousebutton(double x, double y, double button, double set);

	DLL_API double chromium_event_keyboard(double key, double mod, double set);
	DLL_API double chromium_event_keychar(double key);

	DLL_API double chromium_request_source(void);
	DLL_API double chromium_check_source(void);
	DLL_API const char * chromium_get_source(void);

	DLL_API double chromium_check_transfer(void);
	DLL_API const char * chromium_get_transfer(void);

	double chromium_create(double width, double height, double fps, unsigned char * ptr) {
		return init(int(width), int(height), int(fps), ptr, false);
	}

	double chromium_create_test(double width, double height, double fps, unsigned char * ptr) {
		return init(int(width), int(height), int(fps), ptr, true);
	}

	double chromium_step(void) {
		loop();
		return renderHandler->has_newpaint();
	}

	double chromium_cleanup(void) {
		close();
		return 0;
	}

	double chromium_set_url(const char * url) {
		std::string url_str(url);
		browser->GetMainFrame()->LoadURL(url_str);
		std::cout << "Chromium: Set URL " << url_str << std::endl;
		return 0;
	}

	double chromium_set_string(const char * str, const char * url) {
		std::string string_str(str);
		std::string url_str(url);
		browser->GetMainFrame()->LoadString(string_str, url_str);
		return 0;
	}

	const char * chromium_get_url(void) {
		CefString url_str = browser->GetMainFrame()->GetURL();
		return url_str.ToString().c_str();
	}


	double chromium_execute(const char * js) {
		std::string js_str(js);
		CefRefPtr<CefFrame> frame = browser->GetMainFrame();
		frame->ExecuteJavaScript(js, frame->GetURL(), 0);

		std::cout << "Chromium: execute" << std::endl;
		return 0;
	}

	double chromium_stop(void) {
		browser->StopLoad();
		return 0;
	}

	double chromium_reload(void) {
		browser->Reload();
		return 0;
	}

	double chromium_back(void) {
		browser->GoBack();
		return 0;
	}

	double chromium_forward(void) {
		browser->GoForward();
		return 0;
	}

	double chromium_resize(double width, double height) {
		renderHandler->resize(int(width), int(height));
		browser->GetHost()->WasResized();
		browser->GetHost()->SetFocus(true);
		return 0;
	}

	double chromium_get_cursor(void) {
		return double(renderHandler->get_cursor());
	}

	double chromium_get_isloaded(void) {
		return double(browserClient->isLoaded());
	}

	double chromium_get_can_back(void) {
		return double(browserClient->canBack());
	}

	double chromium_get_can_forward(void) {
		return double(browserClient->canForward());
	}

	double chromium_get_last_http_code(void) {
		return double(browserClient->lastHttpCode());
	}

	double chromium_get_debug_value(double value) {
		switch (int(value)) {
		case 0:
			return double(browser->GetFrameCount());
		default:
			return double(renderHandler->get_debug());
		}
	}

	double chromium_event_mousemove(double x, double y) {
		CefMouseEvent event;
		event.x = int(x);
		event.y = int(y);

		browser->GetHost()->SetFocus(true);
		browser->GetHost()->SendMouseMoveEvent(event, false);
		return 0;
	}

	double chromium_event_mousewheel(double hscroll, double vscroll) {
		CefMouseEvent event;
		browser->GetHost()->SendMouseWheelEvent(event, int(hscroll), int(vscroll));
		return 0;
	}

	double chromium_event_mousebutton(double x, double y, double button, double set) {
		CefMouseEvent event;
		event.x = int(x);
		event.y = int(y);
		bool up = true;
		if (set >= 0.5) up = false; // some GML 0.5 bool shit

		// get cef_button
		CefBrowserHost::MouseButtonType cef_button;

		switch (int(button)) {
		default:
		case 0:
			cef_button = MBT_LEFT;
			break;
		case 1:
			cef_button = MBT_MIDDLE;
			break;
		case 2:
			cef_button = MBT_RIGHT;
			break;
		}

		browser->GetHost()->SetFocus(true);
		browser->GetHost()->SendMouseClickEvent(event, cef_button, up, 1);
		return 0;
	}

	double chromium_event_keyboard(double key, double mod, double set) {
		CefKeyEvent event;
		event.modifiers = int(mod);
		event.windows_key_code = int(key);

		if (set >= 0.5) {
			event.type = KEYEVENT_RAWKEYDOWN;
			browser->GetHost()->SendKeyEvent(event);
		}
		else {
			event.type = KEYEVENT_KEYUP;
			browser->GetHost()->SendKeyEvent(event);
		}
		return 0;
	}

	double chromium_event_keychar(double key) {
		CefKeyEvent event;
		event.modifiers = 0;
		event.windows_key_code = int(key);
		event.type = KEYEVENT_CHAR;
		browser->GetHost()->SendKeyEvent(event);
		return 0;
	}

	double chromium_request_source(void) {
		visitor->reset();
		browser->GetMainFrame()->GetSource(visitor);
		std::cout << "Chromium: Request Source" << std::endl;
		return 0;
	}

	double chromium_check_source(void) {
		return double(visitor->has_value());
	}

	const char * chromium_get_source(void) {
		visitor->reset();
		return visitor->get_value();
	}

	double chromium_check_transfer(void) {
		return double(app->has_value());
	}

	const char * chromium_get_transfer(void) {
		app->reset();
		return app->get_value();
	}
}
