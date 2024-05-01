/*
The MIT License (MIT)

Copyright (c) 2024 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <cstdint>
#include <algorithm>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

using byte = uint8_t;
#include <exedit.hpp>
using namespace ExEdit;

////////////////////////////////
// 主要情報源の変数アドレス．
////////////////////////////////
inline constinit struct ExEdit092 {
	AviUtl::FilterPlugin* fp;
	decltype(fp->func_WndProc) func_wndproc;
	constexpr static const char* info_exedit092 = "拡張編集(exedit) version 0.92 by ＫＥＮくん";
	bool init(AviUtl::FilterPlugin* this_fp)
	{
		if (fp != nullptr) return true;
		AviUtl::SysInfo si; this_fp->exfunc->get_sys_info(nullptr, &si);
		for (int i = 0; i < si.filter_n; i++) {
			auto that_fp = this_fp->exfunc->get_filterp(i);
			if (that_fp->information != nullptr &&
				0 == std::strcmp(that_fp->information, info_exedit092)) {
				fp = that_fp;
				func_wndproc = fp->func_WndProc;
				init_pointers();
				return true;
			}
		}
		return false;
	}

	LayerSetting*	LayerSettings;				// 0x188498
	int32_t*		current_scene;				// 0x1a5310
	int32_t*		curr_timeline_layer_height;	// 0x0a3e20
	HWND*			timeline_v_scroll_bar;		// 0x158d34
	int32_t*		timeline_v_scroll_pos;		// 0x1a5308
	int32_t*		timeline_height_in_layers;	// 0x0a3fbc

	void(*nextundo)();							// 0x08d150
	void(*setundo)(uint32_t, uint32_t);			// 0x08d290

private:
	void init_pointers()
	{
		auto pick_addr = [this, exedit_base = reinterpret_cast<uintptr_t>(fp->dll_hinst)]
			<class T>(T& target, ptrdiff_t offset) { target = reinterpret_cast<T>(exedit_base + offset); };
		pick_addr(LayerSettings,				0x188498);
		pick_addr(current_scene,				0x1a5310);
		pick_addr(curr_timeline_layer_height,	0x0a3e20);
		pick_addr(timeline_v_scroll_bar,		0x158d34);
		pick_addr(timeline_v_scroll_pos,		0x1a5308);
		pick_addr(timeline_height_in_layers,	0x0a3fbc);

		pick_addr(nextundo,						0x08d150);
		pick_addr(setundo,						0x08d290);
	}
} exedit;


////////////////////////////////
// 縦スクロールバー操作．
////////////////////////////////
constexpr struct LayerScrollBar {
	void set_pos(int pos, AviUtl::EditHandle* editp) const
	{
		auto hwnd = *exedit.timeline_v_scroll_bar;
		if (hwnd == nullptr) return;

		const SCROLLINFO si{ .cbSize = sizeof(si), .fMask = SIF_POS, .nPos = pos };
		::SetScrollInfo(hwnd, SB_CTL, &si, true);
		exedit.fp->func_WndProc(exedit.fp->hwnd, WM_VSCROLL,
			(pos << 16) | SB_THUMBTRACK, reinterpret_cast<LPARAM>(hwnd),
			editp, exedit.fp);
	}
	int get_pos() const { return *exedit.timeline_v_scroll_pos; }
	int get_page_size() const { return *exedit.timeline_height_in_layers; }
} tl_scroll_v;


////////////////////////////////
// ExEdit のコールバック乗っ取り．
////////////////////////////////
class Drag {
	constexpr static size_t num_layers = 100;
	static inline constinit bool is_dragging = false, turning_undisp = false;
	static inline constinit int layer_prev = 0;

	static auto& layer_flags(int layer) {
		return exedit.LayerSettings[layer + num_layers * (*exedit.current_scene)].flag;
	}
	// returns if the flag UnDisp is set.
	static bool is_layer_undisp(int layer) { return is_layer_undisp(layer_flags(layer)); }
	static bool is_layer_undisp(LayerSetting::Flag flags) {
		return has_flag_or(flags, LayerSetting::Flag::UnDisp);
	}
	// returns if the flag did change.
	static bool set_layer_undisp(int layer, bool undisp)
	{
		auto& flags = layer_flags(layer);
		if (is_layer_undisp(flags) ^ undisp) {
			// push undo buffer.
			set_layer_undo(layer);

			// modify the flag.
			flags ^= LayerSetting::Flag::UnDisp;

			// flag did change.
			return true;
		}
		// flag didn't change.
		return false;
	}

	static void set_layer_undo(int layer) { exedit.setundo(layer, 0x10); }

	constexpr static int x_leftmost_timeline = 64, y_topmost_timeline = 42;
	// シーンによらず最上段レイヤーは 0 扱い．
	static int PointToLayer(int y) {
		y -= y_topmost_timeline;
		y = floor_div(y, *exedit.curr_timeline_layer_height);
		y += *exedit.timeline_v_scroll_pos;

		return std::clamp<int>(y, 0, num_layers);
	}
	// division that rounds toward negative infinity.
	// `divisor` is assumed to be positive.
	static constexpr auto floor_div(auto dividend, auto const divisor) {
		if (dividend < 0) dividend -= divisor - 1;
		return dividend / divisor;
	}

	static bool on_drag(int layer_mouse, AviUtl::EditHandle* editp, AviUtl::FilterPlugin* fp)
	{
		// scroll vertically if the mouse is outside the window.
		scroll_vertically_on_drag(layer_mouse, editp);

		{
			// limit the target layers to the visible ones.
			auto top_layer = tl_scroll_v.get_pos();
			layer_mouse = std::clamp(layer_mouse, top_layer, top_layer + tl_scroll_v.get_page_size() - 1);
		}

		// return if the position didn't change.
		if (layer_mouse == layer_prev) return false;

		// set the range to check.
		int from, until;
		if (layer_mouse < layer_prev) from = layer_mouse, until = layer_prev;
		else from = layer_prev + 1, until = layer_mouse + 1;

		// update the previous mouse position.
		layer_prev = layer_mouse;

		bool updated = false;
		for (int L = from; L < until; L++)
			updated |= set_layer_undisp(L, turning_undisp);

		return updated;
	}

	static void exit_drag(HWND hwnd) {
		is_dragging = false;
		if (::GetCapture() == hwnd)
			::ReleaseCapture();
	}

	// scrolls the timeline vertically when dragging over the visible area.
	// there are at least 100 ms intervals between scrolls.
	static void scroll_vertically_on_drag(int layer_mouse, AviUtl::EditHandle* editp) {
		constexpr uint32_t interval_min = 100;

		constexpr auto check_tick = [] {
		#pragma warning(suppress : 28159) // 32 bit is enough.
			auto curr = ::GetTickCount();

			static constinit decltype(curr) prev = 0;
			if (curr - prev >= interval_min) {
				prev = curr;
				return true;
			}
			return false;
		};

		int dir;
		if (auto rel_pos = layer_mouse - tl_scroll_v.get_pos();
			rel_pos < 0) dir = -1;
		else if (rel_pos >= tl_scroll_v.get_page_size()) dir = +1;
		else return;
		if (!check_tick()) return;

		// then scroll.
		tl_scroll_v.set_pos(tl_scroll_v.get_pos() + dir, editp);
	}

public:
	static BOOL func_wndproc_detour(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, AviUtl::EditHandle* editp, AviUtl::FilterPlugin* fp)
	{
		if (is_dragging) {
			if (!fp->exfunc->is_editing(editp) || fp->exfunc->is_saving(editp)) {
				// AviUtl isn't in a suitable state. abort dragging.
				exit_drag(hwnd);
				goto default_handler;
			}

			switch (message) {
			case WM_MOUSEMOVE:
				if (on_drag(PointToLayer(static_cast<int16_t>(lparam >> 16)), editp, fp)) {
					::InvalidateRect(hwnd, nullptr, FALSE);
					return TRUE;
				}
				return FALSE;

			case WM_LBUTTONDOWN:
			case WM_RBUTTONDOWN:
			case WM_MBUTTONDOWN:
			case WM_XBUTTONDOWN:
			case WM_LBUTTONUP:
			case WM_RBUTTONUP:
			case WM_MBUTTONUP:
			case WM_XBUTTONUP:
				// any mouse button would stop this drag.
			case WM_CAPTURECHANGED: // drag aborted.
				exit_drag(hwnd);
				return FALSE;

			default:
				// otherwise let it be handled normally.
				goto default_handler;
			}
		}
		else {
			// assume left click with no modifier keys.
			if (message != WM_LBUTTONDOWN || (wparam & ~MK_LBUTTON) != 0) goto default_handler;

			// the mouse should be on a layer header.
			int mouse_x = static_cast<int16_t>(lparam), mouse_y = static_cast<int16_t>(lparam >> 16);
			if (!(mouse_x < x_leftmost_timeline && mouse_y >= y_topmost_timeline)) goto default_handler;

			// AviUtl must be in a suitable state.
			if (!fp->exfunc->is_editing(editp) || fp->exfunc->is_saving(editp)) goto default_handler;

			// Then, initiate the drag.
			is_dragging = true;
			::SetCapture(hwnd);

			// initialize related variables.
			layer_prev = PointToLayer(mouse_y);
			auto& clicked_flag = layer_flags(layer_prev);
			turning_undisp = !is_layer_undisp(clicked_flag);

			// prepare and push to the undo buffer.
			exedit.nextundo();
			set_layer_undo(layer_prev);

			// update the clicked layer.
			clicked_flag ^= LayerSetting::Flag::UnDisp;

			// redraw the updated layer.
			::InvalidateRect(hwnd, nullptr, FALSE);
			return TRUE;
		}

	default_handler:
		return exedit.func_wndproc(hwnd, message, wparam, lparam, editp, fp);
	}
};


////////////////////////////////
// AviUtlに渡す関数の定義．
////////////////////////////////
static BOOL func_init(AviUtl::FilterPlugin* fp)
{
	// 情報源確保．
	if (!exedit.init(fp)) {
		::MessageBoxA(nullptr, "拡張編集0.92が見つかりませんでした．",
			fp->name, MB_OK | MB_ICONEXCLAMATION);
		return FALSE;
	}

	// コールバック関数差し替え．
	exedit.fp->func_WndProc = &Drag::func_wndproc_detour;

	return TRUE;
}


////////////////////////////////
// Entry point.
////////////////////////////////
BOOL WINAPI DllMain(HINSTANCE hinst, DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
		::DisableThreadLibraryCalls(hinst);
		break;
	}
	return TRUE;
}


////////////////////////////////
// 看板．
////////////////////////////////
#define PLUGIN_NAME		"レイヤー一括切り替え"
#define PLUGIN_VERSION	"v1.03-beta1"
#define PLUGIN_AUTHOR	"sigma-axis"
#define PLUGIN_INFO_FMT(name, ver, author)	(name##" "##ver##" by "##author)
#define PLUGIN_INFO		PLUGIN_INFO_FMT(PLUGIN_NAME, PLUGIN_VERSION, PLUGIN_AUTHOR)

extern "C" __declspec(dllexport) AviUtl::FilterPluginDLL* __stdcall GetFilterTable(void)
{
	// （フィルタとは名ばかりの）看板．
	using Flag = AviUtl::FilterPlugin::Flag;
	static constinit AviUtl::FilterPluginDLL filter{
		.flag = Flag::NoConfig | Flag::AlwaysActive | Flag::ExInformation,
		.name = PLUGIN_NAME,

		.func_init = func_init,
		.information = PLUGIN_INFO,
	};
	return &filter;
}
